#pragma once

// 画面の上の共通の帯（音量・電池）。LVGL で描く。
namespace status_bar {

/**
 * @brief 帯を作る。LVGL のタスクを起動する前に 1 回だけ呼ぶ
 *
 * 音量は volume:: の状態を一定の周期で見て、変わったときだけ描き直す（LVGL のタスクの中で動く）。
 * 電池は仮の表示（固定値）。
 */
void create();

/** @brief 帯の高さ [px]。本体の画面は、この下に置く */
constexpr int HEIGHT = 14;

}  // namespace status_bar
