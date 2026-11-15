#pragma once

// 画面の上の共通の帯（音量・電池）。LVGL で描く。帯を作って、部品を並べるだけ。
// 音量は widgets/volume_icon、電池は widgets/battery_icon が、それぞれ周期的に見て更新する。
namespace status_bar {

/** @brief 帯を作る。LVGL のタスクを起動する前に 1 回だけ呼ぶ */
void create();

/** @brief 帯の高さ [px]。本体の画面は、この下に置く */
constexpr int HEIGHT = 14;

}  // namespace status_bar
