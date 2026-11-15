#pragma once

// 画面の起動と、起動失敗の表示。通常の画面は LVGL（status_bar / file_list / now_playing）が描く
namespace ui {

/**
 * @brief 画面を起動する（LVGL を初期化し、帯・一覧・再生中の画面を作って、LVGL のタスクを動かす）
 *
 * 画面の切り替えもここで受け持つ。keyboard / player のキューと、曲の一覧（playlist::load()）が
 * できた後に、1 回だけ呼ぶ。
 *
 * @return 成功なら true
 */
bool start();

/**
 * @brief 起動時の失敗を画面に出す。LVGL を起動する前だけ使う
 *
 * M5.Display に直接描くので、LVGL が動いた後に呼ぶと描画が競合する。
 * 文字の大きさもここで設定する。
 *
 * @param[in] msg 失敗した箇所の名前
 */
void showError(const char *msg);

}  // namespace ui
