#pragma once

// 再生中の画面。帯の下に、曲名、再生位置のバー、再生状態と時間、スペクトルアナライザーを出す。LVGL で描く。
// 再生の制御は app/playback が行う。この画面は表示とキーの受け付けだけを行う。
// 再生の終了通知の処理（playback::update）と表示の更新は、LVGL のタスクの中でタイマーが周期的に行う。
namespace now_playing {

/** @brief 画面を閉じるとき（Esc）に呼ばれる関数。再生は止めた後で呼ぶ */
using CloseFn = void (*)();

/**
 * @brief 再生中の画面を作る（最初は隠れている）。status_bar::create() の後、
 *        LVGL のタスクを起動する前に 1 回だけ呼ぶ
 *
 * @param[in] onClose Esc で再生を止めて画面を閉じた後に呼ぶ関数
 */
void create(CloseFn onClose);

/**
 * @brief 再生中の画面を出す。playback::play() が成功した後に、LVGL のタスクの中（イベントの中）から呼ぶ
 *
 * 曲名などは playback の今の状態から出す。
 */
void open();

}  // namespace now_playing
