#pragma once

// 起動失敗の表示。通常の画面は LVGL（status_bar / file_list / now_playing）が描く
namespace ui {

/**
 * @brief 起動時の失敗を画面に出す。LVGL を起動する前だけ使う
 *
 * M5.Display に直接描くので、LVGL が動いた後に呼ぶと描画が競合する。
 *
 * @param[in] msg 失敗した箇所の名前
 */
void showError(const char *msg);

}  // namespace ui
