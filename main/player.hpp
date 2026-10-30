#pragma once

#include "wav.hpp"

// WAV（16bit PCM）の再生。M5.Speaker に playRaw でチャンクを渡す。画面表示には依存しない。
// 使い方: M5.Speaker.begin() を起動時に 1 回呼んでおく。start() が再生用のタスクを作り、
// そのタスクがファイルを読んで playRaw に渡す。呼び出し側のタスクは止まらない。
// 再生が終わったかどうかは isPlaying() で確認する。

namespace player {

/**
 * @brief 再生用のタスクを作って再生を始める。すでに再生中なら何もしない
 *
 * @param[in] path WAV ファイルのパス
 * @param[in] info wav::parse() が返した解析結果。data チャンクの範囲だけを読む
 * @return タスクを作れたら true
 */
bool start(const char *path, const wav::Info &info);

// 再生を止める。再生用のタスクが終わるまで待つ。再生中でなければ何もしない。
void stop();

// 再生中なら true。最後まで読み終えて音が鳴り終わるか、stop() で止めると false になる。
bool isPlaying();

}  // namespace player
