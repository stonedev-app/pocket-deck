#pragma once

#include <stdint.h>

// キーボードのイベントを、アプリの操作に変換する。音量のキーはここで処理する。
namespace input {

/** @brief アプリの操作。キーボードの種類に依存しないよう、意味だけで表す */
enum class Action : uint8_t {
    Next,       // 次の項目へ選択を移す
    Prev,       // 前の項目へ選択を移す
    Enter,      // 選んだ項目を決定する
    Esc,        // 戻る
    PlayPause,  // 一時停止 / 再開（停止中は最初から再生）
    NextTrack,  // 次の曲
    PrevTrack,  // 前の曲
    Repeat,     // 1 曲リピートの切り替え
};

/**
 * @brief キーを 1 件取り出し、操作に変換する。待たずに戻る
 *
 * 音量のキー（Fn + A / S / D）はここで処理し、false を返す（帯は volume:: の状態を見て描き直す）。
 *
 * @param[out] out 取り出した操作
 * @return 操作が取れたら true、なければ false
 */
bool poll(Action &out);

}  // namespace input
