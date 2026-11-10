#pragma once

#include <string>

// 再生中の画面。帯の下に、曲名、再生位置のバー、再生状態と時間を出す。LVGL で描く。
// 再生の終了通知（player::receive）と再生位置は、LVGL のタスクの中でタイマーが周期的に見て更新する。
namespace now_playing {

/**
 * @brief 再生中の画面を作る（最初は隠れている）。status_bar::create() の後、
 *        LVGL のタスクを起動する前に 1 回だけ呼ぶ
 */
void create();

/**
 * @brief WAV を解析して再生を始め、再生中の画面に移る。LVGL のタスクの中（イベントの中）から呼ぶ
 *
 * 解析か再生の開始に失敗したときは、ログに出して何もしない（一覧に残る）。
 *
 * @param[in] name 一覧の WAV のファイル名
 */
void open(const std::string &name);

}  // namespace now_playing
