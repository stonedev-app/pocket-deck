#pragma once

// 画面の描画と、画面モード・キー処理・再生の扱い。
// イベントループを持つ画面のタスクを起動し、描画はそのタスクだけが行う
namespace ui {

/**
 * @brief WAV 一覧を読み込んで描画し、画面のタスクを起動する
 *
 * keyboard::begin() と player::begin() の後に 1 回だけ呼ぶ（それぞれのキューを QueueSet に入れるため）。
 *
 * @return 成功なら true。一覧の読み込み、QueueSet、タスクのどれかに失敗したら false（タスクは動かない）
 */
bool start();

/**
 * @brief 起動時の失敗を画面に出す。画面のタスクを起動する前だけ使う
 *
 * 描画は画面のタスクだけが行う決まりなので、タスクが動いた後に呼ぶと描画が競合する。
 *
 * @param[in] msg 失敗した箇所の名前
 */
void showError(const char *msg);

}  // namespace ui
