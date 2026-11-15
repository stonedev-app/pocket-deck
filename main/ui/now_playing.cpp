#include "ui/now_playing.hpp"

#include <stdint.h>

#include "audio/player.hpp"
#include "audio/spectrum.hpp"
#include "audio/wav.hpp"
#include "device/sdcard.hpp"
#include "esp_log.h"
#include "lvgl.h"
#include "ui/file_list.hpp"
#include "ui/fonts/font_jp_16.h"
#include "ui/status_bar.hpp"

namespace now_playing {

static const char *TAG = "now_playing";

static constexpr int MARGIN = 4;
static constexpr int TITLE_Y = 4;
static constexpr int BAR_Y = 30;
static constexpr int BAR_H = 8;
static constexpr int INFO_Y = 44;
// イコライザーは INFO_Y の下の空き（高さ 54px）に置く
static constexpr int EQ_Y = 66;
static constexpr int EQ_H = 54;
static constexpr int EQ_GAP = 4;  // 棒の間の隙間 [px]
// 再生の状態を見る周期 [ms]
static constexpr uint32_t POLL_MS = 100;
// イコライザーを更新する周期 [ms]
static constexpr uint32_t EQ_POLL_MS = 40;
// 下がるときの 1 回あたりの減り幅（レベル 0〜100）。上がるときは一気に追いつく
static constexpr int EQ_FALL = 8;

// 以下は LVGL のタスクの中（と、起動前の create()）だけで読み書きする
static lv_obj_t *s_root = nullptr;
static lv_obj_t *s_title = nullptr;
static lv_obj_t *s_bar = nullptr;
static lv_obj_t *s_state = nullptr;
static lv_obj_t *s_time = nullptr;
static lv_obj_t *s_repeatMark = nullptr;
enum class State { Playing, Paused, Stopped };
static bool s_active = false;   // 再生中の画面を出している間 true
static State s_state_v = State::Stopped;  // 再生の状態。終了通知を受けたら Stopped にする
static bool s_finished = false;  // 最後まで再生して終わった
static bool s_repeat = false;    // 1 曲リピート
static int s_index = -1;         // 今の曲の、一覧での位置
static std::string s_path;       // 今の曲のパス（停止後の再生・リピートで使う）
static wav::Info s_info;
static uint32_t s_durationMs = 0;
static lv_obj_t *s_prevFocus = nullptr;  // 一覧で選んでいた行。戻るときに選び直す
static lv_obj_t *s_eq = nullptr;         // イコライザー。棒はすべてこの 1 個の中に描く
static int s_eqShown[spectrum::BANDS];  // 表示中のレベル（0〜100）

/**
 * @brief 状態の文字と、時間の文字と、バーを今の再生位置に合わせる
 *
 * 文字は、表示が変わるときだけ設定する。lv_label_set_text() は同じ文字でも描き直すので、
 * 周期的に呼ぶたびに設定すると、変わっていない文字の描画で曲名の描画が遅れる（#57）。
 */
static void refresh()
{
    // 表示中の内容。最初は必ず設定されるよう、ありえない値にしておく
    static const char *shownState = nullptr;
    static int shownRepeat = -1;
    static uint32_t shownPosSec = UINT32_MAX;
    static uint32_t shownDurSec = UINT32_MAX;

    uint32_t pos = s_finished ? s_durationMs : player::positionMs();
    if (pos > s_durationMs) pos = s_durationMs;
    const char *st = s_state_v == State::Playing ? "再生中" : s_state_v == State::Paused ? "一時停止" : "停止";
    if (st != shownState) {
        shownState = st;
        lv_label_set_text(s_state, st);
    }
    if ((int)s_repeat != shownRepeat) {
        shownRepeat = s_repeat;
        lv_label_set_text(s_repeatMark, s_repeat ? "リピート" : "");
    }
    const uint32_t posSec = pos / 1000;
    const uint32_t durSec = s_durationMs / 1000;
    if (posSec != shownPosSec || durSec != shownDurSec) {
        shownPosSec = posSec;
        shownDurSec = durSec;
        lv_label_set_text_fmt(s_time, "%02u:%02u / %02u:%02u", (unsigned)(posSec / 60),
                              (unsigned)(posSec % 60), (unsigned)(durSec / 60), (unsigned)(durSec % 60));
    }
    lv_bar_set_value(s_bar, s_durationMs ? (int32_t)((uint64_t)pos * 1000 / s_durationMs) : 0,
                     LV_ANIM_OFF);
}

/** @brief 溜まっている終了通知を捨てる（自分で止めた・始め直したときの通知を、終了と取り違えないため） */
static void drainEvents()
{
    player::EndReason reason;
    while (player::receive(reason, 0)) {}
}

/** @brief 再生を止めて一覧に戻る。止めたときの Stopped の通知は捨てる */
static void close()
{
    player::stop();
    drainEvents();
    s_active = false;
    s_state_v = State::Stopped;

    lv_group_t *group = lv_group_get_default();
    lv_group_focus_freeze(group, false);
    lv_group_remove_obj(s_root);
    lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    file_list::setVisible(true);
    if (s_prevFocus) lv_group_focus_obj(s_prevFocus);
    s_prevFocus = nullptr;
}

/** @brief 今の曲を最初から再生する（停止後の Space とリピートで使う）。成功で true */
static bool restart()
{
    player::stop();
    drainEvents();
    if (!player::start(s_path.c_str(), s_info)) {
        ESP_LOGE(TAG, "player restart failed");
        s_state_v = State::Stopped;
        return false;
    }
    s_state_v = State::Playing;
    s_finished = false;
    return true;
}

/** @brief 一覧の index 番目の曲を再生して、表示を切り替える。解析か開始に失敗したら false（今の再生は続ける） */
static bool playIndex(int index)
{
    const char *name = file_list::nameAt(index);
    if (!name) return false;
    const std::string path = sdcard::wavPath(name);
    wav::Info info;
    char err[40] = "";
    if (!wav::parse(path.c_str(), info, err, sizeof(err))) {
        ESP_LOGE(TAG, "wav parse failed: %s: %s", name, err);
        return false;
    }
    ESP_LOGI(TAG, "wav: %s rate=%u ch=%u bits=%u data_offset=%u data_bytes=%u", name,
             (unsigned)info.sampleRate, (unsigned)info.channels, (unsigned)info.bitsPerSample,
             (unsigned)info.dataOffset, (unsigned)info.dataBytes);

    // 前の曲が再生中・一時停止中でも止める。古い通知が残っていると、新しい再生が終わったことになる
    player::stop();
    drainEvents();
    if (!player::start(path.c_str(), info)) {
        ESP_LOGE(TAG, "player start failed: %s", name);
        s_state_v = State::Stopped;
        return false;
    }
    s_index = index;
    s_path = path;
    s_info = info;
    s_durationMs = player::durationMs(info);
    s_state_v = State::Playing;
    s_finished = false;
    lv_label_set_text(s_title, name);
    // 一覧に戻ったとき、今の曲の行を選ぶ
    if (lv_obj_t *row = file_list::rowAt(index)) s_prevFocus = row;
    refresh();
    return true;
}

/** @brief Space: 再生中は一時停止、一時停止中は続きから、停止中は最初から */
static void togglePause()
{
    switch (s_state_v) {
    case State::Playing:
        if (player::pause()) {
            drainEvents();  // 一時停止で出る Stopped の通知は終了ではない
            s_state_v = State::Paused;
        }
        break;
    case State::Paused:
        if (player::resume()) s_state_v = State::Playing;
        else ESP_LOGE(TAG, "player resume failed");
        break;
    case State::Stopped:
        restart();
        break;
    }
    refresh();
}

/** @brief 再生中の画面で押されたキー */
static void onKey(lv_event_t *e)
{
    switch (lv_event_get_key(e)) {
    case LV_KEY_ESC:
        close();
        break;
    case ' ':
        togglePause();
        break;
    case LV_KEY_RIGHT:  // 端では何もしない
        if (s_index + 1 < file_list::count()) playIndex(s_index + 1);
        break;
    case LV_KEY_LEFT:
        if (s_index > 0) playIndex(s_index - 1);
        break;
    case 'r':
        s_repeat = !s_repeat;
        refresh();
        break;
    default:
        break;
    }
}

/** @brief 終了通知と再生位置を見て、画面を更新する（LVGL のタスクで周期的に呼ばれる） */
static void poll(lv_timer_t *)
{
    if (!s_active) return;
    player::EndReason reason;
    if (player::receive(reason, 0)) {
        ESP_LOGI(TAG, "playback end: %d", (int)reason);
        s_state_v = State::Stopped;
        // 最後まで再生したときは、位置が端数でずれないよう満了の表示にする
        s_finished = reason == player::EndReason::Finished;
        // リピートがオンなら、同じ曲を最初から始める
        if (s_finished && s_repeat) restart();
    }
    refresh();
}

/** @brief レベル（0〜100）を棒の高さ [px] に直す */
static int eqBarHeight(int level)
{
    return level * EQ_H / 100;
}

/**
 * @brief イコライザーの棒をまとめて描く
 *
 * 棒を 1 本ずつ別の部品にすると、描き直す領域が棒の数だけに分かれ、領域ごとの手間で
 * 描画が間に合わなくなる（実機で計測、#57）。1 個の部品の中に、全部の棒を四角で描く。
 */
static void drawEq(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target_obj(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t area;
    lv_obj_get_coords(obj, &area);
    const int32_t pitch = lv_area_get_width(&area) / spectrum::BANDS;

    lv_draw_rect_dsc_t rect;
    lv_draw_rect_dsc_init(&rect);
    rect.bg_color = lv_color_make(0x40, 0xc0, 0x60);
    rect.bg_opa = LV_OPA_COVER;
    for (int b = 0; b < spectrum::BANDS; b++) {
        const int h = eqBarHeight(s_eqShown[b]);
        if (h <= 0) continue;
        const int32_t x = area.x1 + b * pitch + EQ_GAP / 2;
        lv_area_t bar = {x, area.y2 - h + 1, x + pitch - EQ_GAP - 1, area.y2};
        lv_draw_rect(layer, &rect, &bar);
    }
}

/**
 * @brief spectrum のレベルを読み、棒を更新する。上がるときは速く、下がるときは少しずつ
 *
 * 描き直すのは、高さが変わった棒を囲む範囲だけ（横は変わった棒の端から端、縦は動いた分）。
 */
static void eqPoll(lv_timer_t *)
{
    if (!s_active) return;
    uint8_t target[spectrum::BANDS];
    spectrum::levels(target);
    int first = -1, last = -1;       // 高さが変わった棒の範囲
    int hMin = EQ_H, hMax = 0;       // 変わった棒の、変わる前後の高さの範囲
    for (int b = 0; b < spectrum::BANDS; b++) {
        int cur = s_eqShown[b];
        if (target[b] >= cur) {
            cur = cur + (target[b] - cur + 1) * 3 / 4;  // 差の 3/4 まで一気に上げる
            if (cur > target[b]) cur = target[b];
        } else {
            cur -= EQ_FALL;
            if (cur < target[b]) cur = target[b];
        }
        const int before = eqBarHeight(s_eqShown[b]);
        const int after = eqBarHeight(cur);
        s_eqShown[b] = cur;
        if (before == after) continue;
        if (first < 0) first = b;
        last = b;
        if (before < hMin) hMin = before;
        if (after < hMin) hMin = after;
        if (before > hMax) hMax = before;
        if (after > hMax) hMax = after;
    }
    if (first < 0) return;

    lv_area_t area;
    lv_obj_get_coords(s_eq, &area);
    const int32_t pitch = lv_area_get_width(&area) / spectrum::BANDS;
    lv_area_t dirty = {area.x1 + first * pitch, area.y2 - hMax + 1, area.x1 + (last + 1) * pitch - 1,
                       area.y2 - hMin};
    lv_obj_invalidate_area(s_eq, &dirty);
}

/** @brief 棒をすべて 0 に戻す（画面を開くとき） */
static void eqReset()
{
    for (int b = 0; b < spectrum::BANDS; b++) s_eqShown[b] = 0;
    lv_obj_invalidate(s_eq);
}

void create()
{
    lv_obj_t *screen = lv_screen_active();
    s_root = lv_obj_create(screen);
    lv_obj_remove_style_all(s_root);
    lv_obj_set_size(s_root, LV_PCT(100), lv_display_get_vertical_resolution(nullptr) - status_bar::HEIGHT);
    lv_obj_align(s_root, LV_ALIGN_TOP_LEFT, 0, status_bar::HEIGHT);
    lv_obj_remove_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_root, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(s_root, &font_jp_16, 0);
    lv_obj_set_style_text_color(s_root, lv_color_white(), 0);
    lv_obj_add_event_cb(s_root, onKey, LV_EVENT_KEY, nullptr);
    // 一覧の選択の移動に混ざらないよう、表示している間だけグループに入れる
    lv_group_remove_obj(s_root);
    lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);

    const int w = lv_display_get_horizontal_resolution(nullptr) - MARGIN * 2;

    // 曲名。長いときは折り返さず、流して見せる
    s_title = lv_label_create(s_root);
    lv_label_set_long_mode(s_title, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    lv_obj_set_width(s_title, w);
    lv_obj_set_pos(s_title, MARGIN, TITLE_Y);

    s_bar = lv_bar_create(s_root);
    lv_bar_set_range(s_bar, 0, 1000);
    lv_obj_set_size(s_bar, w, BAR_H);
    lv_obj_set_pos(s_bar, MARGIN, BAR_Y);
    lv_obj_set_style_radius(s_bar, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(s_bar, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(s_bar, lv_color_make(0x40, 0x40, 0x40), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_bar, lv_color_make(0x20, 0x60, 0xc0), LV_PART_INDICATOR);

    s_state = lv_label_create(s_root);
    lv_obj_set_pos(s_state, MARGIN, INFO_Y);

    s_time = lv_label_create(s_root);
    lv_obj_align(s_time, LV_ALIGN_TOP_RIGHT, -MARGIN, INFO_Y);

    // リピートがオンの間だけ文字を出す（状態と時間の間）
    s_repeatMark = lv_label_create(s_root);
    lv_obj_set_style_text_color(s_repeatMark, lv_color_make(0x40, 0xc0, 0x60), 0);
    lv_obj_align(s_repeatMark, LV_ALIGN_TOP_MID, -10, INFO_Y);

    // イコライザー。棒は drawEq() が描く
    s_eq = lv_obj_create(s_root);
    lv_obj_remove_style_all(s_eq);
    lv_obj_set_size(s_eq, w, EQ_H);
    lv_obj_set_pos(s_eq, MARGIN, EQ_Y);
    lv_obj_remove_flag(s_eq, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(s_eq, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_eq, drawEq, LV_EVENT_DRAW_MAIN, nullptr);

    lv_timer_create(poll, POLL_MS, nullptr);
    lv_timer_create(eqPoll, EQ_POLL_MS, nullptr);
}

void open(const std::string &name)
{
    const int index = file_list::indexOf(name.c_str());
    lv_group_t *group = lv_group_get_default();
    s_prevFocus = lv_group_get_focused(group);
    if (!playIndex(index)) return;
    s_active = true;
    eqReset();
    file_list::setVisible(false);
    lv_obj_remove_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    lv_group_add_obj(group, s_root);
    lv_group_focus_obj(s_root);
    // 再生中は項目の移動を止める（Next / Prev で一覧の隠れた行に移らないように）
    lv_group_focus_freeze(group, true);
}

}  // namespace now_playing
