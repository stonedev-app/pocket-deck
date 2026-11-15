#pragma once

// 起動失敗の表示。通常の画面は LVGL（status_bar / file_list / now_playing）が描く
namespace ui {

/**
 * @brief 画面の部品を作る（帯と、その下の一覧、再生中の画面）。画面の切り替えもここで受け持つ
 *
 * playlist::load() の後、LVGL のタスクを起動する前に、lvgl_port から 1 回だけ呼ばれる。
 */
void build();

/**
 * @brief 起動時の失敗を画面に出す。LVGL を起動する前だけ使う
 *
 * M5.Display に直接描くので、LVGL が動いた後に呼ぶと描画が競合する。
 *
 * @param[in] msg 失敗した箇所の名前
 */
void showError(const char *msg);

}  // namespace ui
