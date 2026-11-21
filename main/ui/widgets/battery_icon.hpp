#pragma once

#include "lvgl.h" // IWYU pragma: keep

// 電池の絵。残量は device/battery から一定の周期で読み、変わったときだけ描き直す（LVGL のタスクの中で動く）。
namespace battery_icon {

/** @brief 電池の絵を parent の中に作る。LVGL のタスクを起動する前に 1 回だけ呼ぶ */
lv_obj_t *create(lv_obj_t *parent);

}  // namespace battery_icon
