#include "ui/widgets/battery_icon.hpp"

#include <cstdlib>

#include "device/battery.hpp"
#include "esp_log.h"
#include "ui/lvgl_util.hpp"

namespace battery_icon {

// 枠の大きさ [px]
static constexpr int W = 20;
static constexpr int H = 10;
// 残量を読む周期 [ms]
static constexpr uint32_t POLL_MS = 5000;
// 残量が少ないとみなす値 [%]（以下で色を変える）
static constexpr int LOW_PERCENT = 15;
// 表示を更新する最小の変化 [%]（ADC のぶれ対策）
static constexpr int BATTERY_HYSTERESIS = 3;

static const char *TAG = "battery_icon";
static lv_obj_t *s_fill = nullptr;
static lv_obj_t *s_label = nullptr;
// 表示中の残量（-1 は取得できていない、-2 は未表示）。LVGL のタスクの中だけで読み書きする
static int s_percent = -2;

/**
 * @brief 残量の数字、中身の長さ、色を、残量（%、不正なら負）に合わせる
 *
 * 数字は不正なら "--%"。残量が少ないときは、数字も中身も赤にする。
 */
static void apply(int percent)
{
    const lv_color_t color = percent >= 0 && percent <= LOW_PERCENT ? lv_color_make(0xff, 0x40, 0x40) : lv_color_white();
    if (percent < 0) {
        lv_label_set_text(s_label, "--%");
    } else {
        lv_label_set_text_fmt(s_label, "%d%%", percent);
    }
    lv_obj_set_style_text_color(s_label, color, 0);

    const int inner = W - 4;
    const int w = percent < 0 ? 0 : inner * percent / 100;
    // 幅 0 の箱は LVGL が扱いにくいので、非表示にする
    if (w <= 0) {
        lv_obj_add_flag(s_fill, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_remove_flag(s_fill, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_width(s_fill, w);
    lv_obj_set_style_bg_color(s_fill, color, 0);
}

/**
 * @brief 電池の残量を読んで、変わっていたら絵を更新する（LVGL のタスクで周期的に呼ばれる）
 *
 * ADC のぶれでちらつかないよう、前回の表示から BATTERY_HYSTERESIS 以上
 * 離れたときだけ更新する。色の切り替わりも表示中の残量で決まるので、最大 BATTERY_HYSTERESIS 遅れる。
 *
 * 充電中の表示はしない。Cardputer-Adv では M5.Power.isCharging() が常に charge_unknown を返し、
 * 充電状態が取れない（実機で確認）。
 */
static void poll(lv_timer_t *)
{
    const int raw = battery::percent();
    const bool valid = raw >= 0;

    int percent = s_percent;
    if (!valid) {
        percent = -1;
    } else if (s_percent < 0 || std::abs(raw - s_percent) >= BATTERY_HYSTERESIS || raw == 0 || raw == 100) {
        percent = raw;
    }

    if (percent == s_percent) return;
    if (!valid && s_percent != -1) {
        ESP_LOGW(TAG, "battery level unavailable");
    }
    s_percent = percent;
    apply(percent);
}

lv_obj_t *create(lv_obj_t *parent)
{
    // 残量の数字（電池の左）。親に先に作るので、並びは数字 → 電池になる
    s_label = lv_label_create(parent);
    lv_obj_set_style_text_font(s_label, &lv_font_montserrat_12, 0);
    // 桁数が変わっても幅が変わらないよう、最も長い "100%" が入る幅にして右寄せにする
    lvgl_util::setFixedWidthRight(s_label, &lv_font_montserrat_12, "100%");

    // 枠 + 突起をまとめる入れ物
    lv_obj_t *box = lvgl_util::createBox(parent, W + 2, H);

    lv_obj_t *frame = lvgl_util::createBox(box, W, H);
    lv_obj_set_pos(frame, 0, 0);
    lv_obj_set_style_border_width(frame, 1, 0);
    lv_obj_set_style_border_color(frame, lv_color_white(), 0);

    // 中身。枠の内側（1px の余白）に収める
    s_fill = lvgl_util::createBox(frame, W - 4, H - 4);
    lv_obj_set_pos(s_fill, 1, 1);
    lv_obj_set_style_bg_color(s_fill, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(s_fill, LV_OPA_COVER, 0);

    lv_obj_t *nub = lvgl_util::createBox(box, 2, 4);
    lv_obj_set_pos(nub, W, (H - 4) / 2);
    lv_obj_set_style_bg_color(nub, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(nub, LV_OPA_COVER, 0);

    poll(nullptr);
    lv_timer_create(poll, POLL_MS, nullptr);
    return box;
}

}  // namespace battery_icon
