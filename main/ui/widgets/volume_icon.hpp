#pragma once

#include "lvgl.h"

// 音量の数字とアイコン（スピーカー + 弧、消音のときは暗いスピーカー + 斜線）。
// volume:: の状態を一定の周期で見て、変わったときだけ描き直す（LVGL のタスクの中で動く）。
namespace volume_icon {

/**
 * @brief 数字とアイコンを parent の中に左から順に作る。LVGL のタスクを起動する前に 1 回だけ呼ぶ
 *
 * @param[in] parent 置き先（横に並べる入れ物を想定）
 * @param[in] height アイコンの高さ [px]
 */
void create(lv_obj_t *parent, int height);

}  // namespace volume_icon
