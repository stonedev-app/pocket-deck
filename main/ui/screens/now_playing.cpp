#include "ui/screens/now_playing.hpp"

#include <stdint.h>

#include "app/playback.hpp"
#include "app/playlist.hpp"
#include "lvgl.h"   // IWYU pragma: keep
#include "ui/fonts/font_jp_16.h"
#include "ui/keys.hpp"
#include "ui/widgets/spectrum_view.hpp"
#include "ui/widgets/status_bar.hpp"

namespace now_playing {

static constexpr int MARGIN = 4;
static constexpr int TITLE_Y = 4;
static constexpr int BAR_Y = 30;
static constexpr int BAR_H = 8;
static constexpr int INFO_Y = 44;
// スペクトルアナライザーは INFO_Y の下の空き（高さ 54px）に置く
static constexpr int SPECTRUM_Y = 66;
static constexpr int SPECTRUM_H = 54;
// 再生の状態を見る周期 [ms]
static constexpr uint32_t POLL_MS = 100;

// 以下は LVGL のタスクの中（と、起動前の create()）だけで読み書きする
static lv_obj_t *s_root = nullptr;
static lv_obj_t *s_title = nullptr;
static lv_obj_t *s_bar = nullptr;
static lv_obj_t *s_state = nullptr;
static lv_obj_t *s_time = nullptr;
static lv_obj_t *s_repeatMark = nullptr;
static bool s_active = false;   // 再生中の画面を出している間 true
static CloseFn s_onClose = nullptr;

/**
 * @brief 状態の文字と、時間の文字と、バーを今の再生位置に合わせる
 *
 * 文字は、表示が変わるときだけ設定する。lv_label_set_text() は同じ文字でも描き直すので、
 * 周期的に呼ぶたびに設定すると、変わっていない文字の描画で曲名の描画が遅れる（#57）。
 */
static void refresh()
{
    // 表示中の内容。最初は必ず設定されるよう、ありえない値にしておく
    static int shownIndex = -2;
    static const char *shownState = nullptr;
    static int shownRepeat = -1;
    static uint32_t shownPosSec = UINT32_MAX;
    static uint32_t shownDurSec = UINT32_MAX;

    const playback::State state = playback::state();
    const bool repeat = playback::repeat();
    const uint32_t durationMs = playback::durationMs();
    const uint32_t pos = playback::positionMs();
    if (playback::index() != shownIndex) {
        shownIndex = playback::index();
        const char *name = playlist::nameAt(shownIndex);
        lv_label_set_text(s_title, name ? name : "");
    }
    const char *st = state == playback::State::Playing ? "再生中"
                     : state == playback::State::Paused ? "一時停止" : "停止";
    if (st != shownState) {
        shownState = st;
        lv_label_set_text(s_state, st);
    }
    if ((int)repeat != shownRepeat) {
        shownRepeat = repeat;
        lv_label_set_text(s_repeatMark, repeat ? "リピート" : "");
    }
    const uint32_t posSec = pos / 1000;
    const uint32_t durSec = durationMs / 1000;
    if (posSec != shownPosSec || durSec != shownDurSec) {
        shownPosSec = posSec;
        shownDurSec = durSec;
        lv_label_set_text_fmt(s_time, "%02u:%02u / %02u:%02u", (unsigned)(posSec / 60),
                              (unsigned)(posSec % 60), (unsigned)(durSec / 60), (unsigned)(durSec % 60));
    }
    lv_bar_set_value(s_bar, durationMs ? (int32_t)((uint64_t)pos * 1000 / durationMs) : 0,
                     LV_ANIM_OFF);
}

/** @brief 再生を止めて一覧に戻る */
static void close()
{
    playback::stop();
    s_active = false;
    spectrum_view::setActive(false);

    lv_group_t *group = lv_group_get_default();
    lv_group_focus_freeze(group, false);
    lv_group_remove_obj(s_root);
    lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    if (s_onClose) s_onClose();
}

/** @brief 再生中の画面で押されたキー */
static void onKey(lv_event_t *e)
{
    switch (lv_event_get_key(e)) {
    case LV_KEY_ESC:
        close();
        return;
    case ui::keys::PLAY_PAUSE:
        playback::togglePause();
        break;
    case ui::keys::NEXT_TRACK:  // 端では何もしない
        playback::next();
        break;
    case ui::keys::PREV_TRACK:
        playback::prev();
        break;
    case ui::keys::REPEAT:
        playback::toggleRepeat();
        break;
    default:
        return;
    }
    refresh();
}

/** @brief 終了通知と再生位置を見て、画面を更新する（LVGL のタスクで周期的に呼ばれる） */
static void poll(lv_timer_t *)
{
    if (!s_active) return;
    playback::update();
    refresh();
}

void create(CloseFn onClose)
{
    s_onClose = onClose;
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

    spectrum_view::create(s_root, MARGIN, SPECTRUM_Y, w, SPECTRUM_H);

    lv_timer_create(poll, POLL_MS, nullptr);
}

void open()
{
    lv_group_t *group = lv_group_get_default();
    s_active = true;
    spectrum_view::reset();
    spectrum_view::setActive(true);
    refresh();
    lv_obj_remove_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    lv_group_add_obj(group, s_root);
    lv_group_focus_obj(s_root);
    // 再生中は項目の移動を止める（Next / Prev で一覧の隠れた行に移らないように）
    lv_group_focus_freeze(group, true);
}

}  // namespace now_playing
