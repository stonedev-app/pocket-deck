#include "ui/screens/file_list.hpp"

#include "app/playlist.hpp"
#include "esp_log.h"
#include "lvgl.h"  // IWYU pragma: keep
#include "ui/fonts/font_jp_16.h"
#include "ui/widgets/status_bar.hpp"

namespace file_list {

static const char *TAG = "file_list";

// 1 行の高さ [px]。画面 135px から帯 14px を引いた 121px に、約 6 行入る
static constexpr int ROW_H = 20;

static lv_obj_t *s_list = nullptr;
static SelectFn s_onSelect = nullptr;

/** @brief Enter（クリック）された行の位置を、選択の通知に渡す */
static void onClicked(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target_obj(e);
    const int index = (int)lv_obj_get_index(btn);
    ESP_LOGI(TAG, "enter: %d", index);
    if (s_onSelect) s_onSelect(index);
}

/** @brief index 番目の行（LVGL の部品）。なければ nullptr */
static lv_obj_t *rowAt(int index)
{
    // 0 件のときは子が「(none)」のラベルだけなので、行はない
    if (!s_list || index < 0 || index >= playlist::count()) return nullptr;
    return lv_obj_get_child(s_list, index);
}

void show(int index)
{
    if (!s_list) return;
    lv_obj_remove_flag(s_list, LV_OBJ_FLAG_HIDDEN);
    if (lv_obj_t *row = rowAt(index)) lv_group_focus_obj(row);
}

void hide()
{
    if (!s_list) return;
    lv_obj_add_flag(s_list, LV_OBJ_FLAG_HIDDEN);
}

void create(SelectFn onSelect)
{
    s_onSelect = onSelect;

    lv_obj_t *list = lv_list_create(lv_screen_active());
    s_list = list;
    lv_obj_set_size(list, LV_PCT(100), lv_display_get_vertical_resolution(nullptr) - status_bar::HEIGHT);
    lv_obj_align(list, LV_ALIGN_TOP_LEFT, 0, status_bar::HEIGHT);
    // 既定のテーマの枠・余白・角丸を外し、黒地にする
    lv_obj_set_style_pad_all(list, 0, 0);
    lv_obj_set_style_pad_row(list, 0, 0);
    lv_obj_set_style_border_width(list, 0, 0);
    lv_obj_set_style_radius(list, 0, 0);
    lv_obj_set_style_bg_color(list, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(list, LV_OPA_COVER, 0);
    // 日本語のファイル名を出すため、リスト全体（項目に引き継がれる）に日本語フォントを指定する
    lv_obj_set_style_text_font(list, &font_jp_16, 0);

    if (playlist::count() == 0) {
        lv_obj_t *label = lv_label_create(list);
        lv_label_set_text(label, "(none)");
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        return;
    }

    for (int i = 0; i < playlist::count(); i++) {
        lv_obj_t *btn = lv_list_add_button(list, nullptr, playlist::nameAt(i));
        lv_obj_set_height(btn, ROW_H);
        lv_obj_set_style_pad_top(btn, 0, 0);
        lv_obj_set_style_pad_bottom(btn, 0, 0);
        lv_obj_set_style_pad_left(btn, 4, 0);
        lv_obj_set_style_radius(btn, 0, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_outline_width(btn, 0, 0);
        lv_obj_set_style_bg_color(btn, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(btn, lv_color_white(), 0);
        // 選択中の行は、青い地に白の文字で強調する
        lv_obj_set_style_bg_color(btn, lv_color_make(0x20, 0x60, 0xc0), LV_STATE_FOCUSED);
        lv_obj_add_event_cb(btn, onClicked, LV_EVENT_CLICKED, nullptr);
    }
}

}  // namespace file_list
