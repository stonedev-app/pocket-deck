#pragma once

#include "lvgl.h" // IWYU pragma: keep

// イコライザー。棒を 1 個の部品にまとめて描き、変わった範囲だけを描き直す（#57 の対策）。
// spectrum のレベルを一定の周期で読んで更新する（LVGL のタスクの中で動く）。
namespace eq_view {

/**
 * @brief イコライザーを parent の中に作る。LVGL のタスクを起動する前に 1 回だけ呼ぶ
 *
 * 最初は動かない（setActive(true) で動き出す）。
 *
 * @param[in] parent 置き先
 * @param[in] x, y 置く位置 [px]
 * @param[in] w, h 大きさ [px]
 */
void create(lv_obj_t *parent, int x, int y, int w, int h);

/** @brief 棒をすべて 0 に戻す（画面を開くとき） */
void reset();

/** @brief 動かす / 止める。止めている間は spectrum を読まず、描き直しもしない */
void setActive(bool active);

}  // namespace eq_view
