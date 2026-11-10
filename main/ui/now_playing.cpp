#include "ui/now_playing.hpp"

#include <stdint.h>

#include "audio/player.hpp"
#include "audio/wav.hpp"
#include "device/sdcard.hpp"
#include "esp_log.h"
#include "lvgl.h"
#include "ui/file_list.hpp"
#include "ui/status_bar.hpp"

namespace now_playing {

static const char *TAG = "now_playing";

static constexpr int MARGIN = 4;
static constexpr int TITLE_Y = 4;
static constexpr int BAR_Y = 30;
static constexpr int BAR_H = 8;
static constexpr int INFO_Y = 44;
// イコライザー（段階 5）の場所として、INFO_Y の下を空けておく
// 再生の状態を見る周期 [ms]
static constexpr uint32_t POLL_MS = 100;

// 以下は LVGL のタスクの中（と、起動前の create()）だけで読み書きする
static lv_obj_t *s_root = nullptr;
static lv_obj_t *s_title = nullptr;
static lv_obj_t *s_bar = nullptr;
static lv_obj_t *s_state = nullptr;
static lv_obj_t *s_time = nullptr;
static bool s_active = false;   // 再生中の画面を出している間 true
static bool s_playing = false;  // 再生中の表示。終了通知を受けたら下ろす
static bool s_finished = false;  // 最後まで再生して終わった
static uint32_t s_durationMs = 0;
static lv_obj_t *s_prevFocus = nullptr;  // 一覧で選んでいた行。戻るときに選び直す

/** @brief 状態の文字と、時間の文字と、バーを今の再生位置に合わせる */
static void refresh()
{
    uint32_t pos = s_finished ? s_durationMs : player::positionMs();
    if (pos > s_durationMs) pos = s_durationMs;
    lv_label_set_text(s_state, s_playing ? "再生中" : "停止");
    lv_label_set_text_fmt(s_time, "%02u:%02u / %02u:%02u", (unsigned)(pos / 60000),
                          (unsigned)(pos / 1000 % 60), (unsigned)(s_durationMs / 60000),
                          (unsigned)(s_durationMs / 1000 % 60));
    lv_bar_set_value(s_bar, s_durationMs ? (int32_t)((uint64_t)pos * 1000 / s_durationMs) : 0,
                     LV_ANIM_OFF);
}

/** @brief 再生を止めて一覧に戻る。止めたときの Stopped の通知は捨てる */
static void close()
{
    player::stop();
    player::EndReason reason;
    while (player::receive(reason, 0)) {}
    s_active = false;
    s_playing = false;

    lv_group_t *group = lv_group_get_default();
    lv_group_focus_freeze(group, false);
    lv_group_remove_obj(s_root);
    lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    file_list::setVisible(true);
    if (s_prevFocus) lv_group_focus_obj(s_prevFocus);
    s_prevFocus = nullptr;
}

/** @brief 再生中の画面で押されたキー。Esc で止めて一覧に戻る（Enter などは今は何もしない） */
static void onKey(lv_event_t *e)
{
    if (lv_event_get_key(e) == LV_KEY_ESC) close();
}

/** @brief 終了通知と再生位置を見て、画面を更新する（LVGL のタスクで周期的に呼ばれる） */
static void poll(lv_timer_t *)
{
    if (!s_active) return;
    player::EndReason reason;
    if (player::receive(reason, 0)) {
        ESP_LOGI(TAG, "playback end: %d", (int)reason);
        s_playing = false;
        // 最後まで再生したときは、位置が端数でずれないよう満了の表示にする
        s_finished = reason == player::EndReason::Finished;
    }
    refresh();
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
    lv_obj_set_style_text_font(s_root, &lv_font_source_han_sans_sc_16_cjk, 0);
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

    lv_timer_create(poll, POLL_MS, nullptr);
}

void open(const std::string &name)
{
    const std::string path = sdcard::wavPath(name);
    wav::Info info;
    char err[40] = "";
    if (!wav::parse(path.c_str(), info, err, sizeof(err))) {
        ESP_LOGE(TAG, "wav parse failed: %s: %s", name.c_str(), err);
        return;
    }
    ESP_LOGI(TAG, "wav: %s rate=%u ch=%u bits=%u data_offset=%u data_bytes=%u", name.c_str(),
             (unsigned)info.sampleRate, (unsigned)info.channels, (unsigned)info.bitsPerSample,
             (unsigned)info.dataOffset, (unsigned)info.dataBytes);

    // 古い通知が残っていると、新しい再生が終わったことになってしまうので捨てる
    player::EndReason reason;
    while (player::receive(reason, 0)) {}
    if (!player::start(path.c_str(), info)) {
        ESP_LOGE(TAG, "player start failed: %s", name.c_str());
        return;
    }

    s_durationMs = player::durationMs(info);
    s_playing = true;
    s_finished = false;
    s_active = true;
    lv_label_set_text(s_title, name.c_str());
    refresh();

    lv_group_t *group = lv_group_get_default();
    s_prevFocus = lv_group_get_focused(group);
    file_list::setVisible(false);
    lv_obj_remove_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    lv_group_add_obj(group, s_root);
    lv_group_focus_obj(s_root);
    // 再生中は項目の移動を止める（Next / Prev で一覧の隠れた行に移らないように）
    lv_group_focus_freeze(group, true);
}

}  // namespace now_playing
