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

}  // namespace lvgl_util
