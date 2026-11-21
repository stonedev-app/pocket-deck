#pragma once

#include <stdint.h>

#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "audio/wav.hpp"

// WAV（16bit PCM）の再生。device/speaker にチャンクを渡す。画面表示には依存しない。
// 使い方: speaker::begin() を起動時に 1 回呼んでおく。start() が再生用のタスクを作り、
// そのタスクがファイルを読んで speaker::play に渡す。呼び出し側のタスクは止まらない。
// 再生が終わると、再生用のタスクが終了の理由（EndReason）をキューに入れる。
// 呼び出し側は receive() で取り出す（ui のタイマーが周期的に見る）。
// スペクトルアナライザーのレベル計算は audio/spectrum が行う。

namespace player {

/** @brief 再生が終わった理由。1 回の再生につき、タスクが終わるときに 1 件だけ送る */
enum class EndReason : uint8_t {
    Finished,  // 最後まで再生した
    Stopped,   // stop() で止めた
    Failed,    // ファイルが開けない・読めないなどで続けられなかった
};

/**
 * @brief 終了通知のキューを作る。起動時に 1 回呼ぶ
 *
 * @return 成功で true
 */
bool begin();

/**
 * @brief 終了通知を 1 件取り出す
 *
 * @param[out] out     終了の理由
 * @param[in]  timeout 待つ時間（tick 数）
 * @return 取れたら true
 */
bool receive(EndReason &out, TickType_t timeout);

/**
 * @brief 再生用のタスクを作って再生を始める。すでに再生中なら何もしない
 *
 * @param[in] path WAV ファイルのパス
 * @param[in] info wav::parse() が返した解析結果。data チャンクの範囲だけを読む
 * @return タスクを作れたら true
 */
bool start(const char *path, const wav::Info &info);

/**
 * @brief 再生を一時停止する。再生用のタスクを止め、読み出し位置を覚える
 *
 * 止めるときの終了通知は Stopped で、stop() と同じく戻った時点でキューに入っている
 * （呼び出し側で必要に応じて捨てる）。positionMs() は止めた位置のまま動かない。
 *
 * @return 再生中で、止められたら true
 */
bool pause();

/**
 * @brief pause() で止めた位置から再生を続ける。一時停止中でなければ何もしない
 *
 * @return タスクを作れたら true
 */
bool resume();

/**
 * @brief WAV 全体の長さ [ms] を、wav::Info から出す
 */
uint32_t durationMs(const wav::Info &info);

/**
 * @brief 直近に start() した再生の位置 [ms] を返す
 *
 * playRaw に渡したバイト数で数えるので、実際の音より最大でバッファ数個分（約 0.1 秒）進む。
 * start() の前は 0。どのタスクから呼んでもよい。
 */
uint32_t positionMs();

/**
 * @brief 再生を止める。再生用のタスクが終わるまで待つ
 *
 * 再生中でなければ何もしない。止めた場合も Stopped の通知が届く
 * （stop() から戻った時点でキューに入っている）。
 * 待つ上限は player.cpp の STOP_WAIT_MS で、超えたらログに出して戻る。
 */
void stop();

}  // namespace player
