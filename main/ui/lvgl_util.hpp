#pragma once

#include "lvgl.h" // IWYU pragma: keep

// ui/ の共通の小さな道具。
namespace lvgl_util {

/** @brief 飾りのない箱を作る（既定のテーマの枠・余白・スクロールを外す） */
inline lv_obj_t *createBox(lv_obj_t *parent, int w, int h)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, w, h);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

/**
 * @brief 数字のラベルを、widest が入る固定幅・右寄せにする
 *
 * 桁数が変わっても、ラベルの幅は変わらない（隣の表示がずれない）。
 * 幅は widest をフォントで描いたときの幅から求める。
 */
inline void setFixedWidthRight(lv_obj_t *label, const lv_font_t *font, const char *widest)
{
    lv_point_t size;
    lv_text_get_size(&size, widest, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    lv_obj_set_width(label, size.x);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
}

}  // namespace lvgl_util
