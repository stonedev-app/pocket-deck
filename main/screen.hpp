#pragma once

// 画面のタスク。イベントループ（QueueSet の待ちとイベント処理）を持ち、描画はこのタスクだけが行う
namespace screen {

/**
 * @brief 画面のタスクを起動する
 *
 * keyboard::begin() と player::begin() の後に 1 回だけ呼ぶ。
 * 2 つのキューを QueueSet にまとめ、タスクがイベントを待つ。
 * @return 成功なら true。QueueSet の作成かタスクの作成に失敗したら false（タスクは動かない）
 */
bool start();

}  // namespace screen
