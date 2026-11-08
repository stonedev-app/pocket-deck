#pragma once

#include <stdint.h>

#include "freertos/FreeRTOS.h"  // IWYU pragma: keep
#include "freertos/queue.h"
#include "audio/wav.hpp"

// WAV（16bit PCM）の再生。M5.Speaker に playRaw でチャンクを渡す。画面表示には依存しない。
// 使い方: M5.Speaker.begin() を起動時に 1 回呼んでおく。start() が再生用のタスクを作り、
// そのタスクがファイルを読んで playRaw に渡す。呼び出し側のタスクは止まらない。
// 再生が終わると、再生用のタスクが終了の理由（EndReason）をキューに入れる。
// 呼び出し側は begin() の後に eventQueue() のハンドルを QueueSet に入れて待ち、receive() で取り出す。

namespace player {

// 再生が終わった理由。1 回の再生につき、タスクが終わるときに 1 件だけ送る
enum class EndReason : uint8_t {
    Finished,  // 最後まで再生した
    Stopped,   // stop() で止めた
    Failed,    // ファイルが開けない・読めないなどで続けられなかった
};

// キューの長さ。1 回の再生で 1 件しか出ず、受け取る側が毎回取り出すので 1 件でも足りるが、
// 取り出しが遅れても溜められるように 4 にした。満杯のときは新しい通知を捨ててログに出す
// （再生中に次の再生は始められないので、実際には満杯にならない）。
// QueueSet の大きさを決める側が使う
constexpr int EVENT_QUEUE_LEN = 4;

/**
 * @brief 終了通知のキューを作る。起動時に 1 回呼ぶ
 *
 * @return 成功で true
 */
bool begin();

// 終了通知のキューのハンドル。QueueSet に入れて待つためのもの。begin() が成功する前は nullptr。
QueueHandle_t eventQueue();

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

// 再生を止める。再生用のタスクが終わるまで待つ。再生中でなければ何もしない。
// 止めた場合も Stopped の通知が届く（stop() から戻った時点でキューに入っている）。
void stop();

}  // namespace player
