#pragma once

#include "lvgl.h" // IWYU pragma: keep

// 電池の絵と、残量の数字（%）。残量は device/battery から一定の周期で読み、変わったときだけ描き直す（LVGL のタスクの中で動く）。
namespace battery_icon {

/**
 * @brief 電池の絵と残量の数字（電池の左）を parent の中に作る。LVGL のタスクを起動する前に 1 回だけ呼ぶ
 *
 * 返り値は電池の箱だけで、数字のラベルは含まない。
 */
lv_obj_t *create(lv_obj_t *parent);

}  // namespace battery_icon
