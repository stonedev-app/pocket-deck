#pragma once

#include <stddef.h>

// SD カード（SPI 接続）のマウント。表示はしない。
// 使い方: M5.begin() の後に begin() を 1 回呼ぶ（SPI バスの初期化が M5GFX のディスプレイ用 SPI と衝突しないように）。
// WAV の一覧は app/playlist が MOUNT_POINT から取る。

namespace sdcard {

/** @brief マウント先のパス */
inline constexpr const char *MOUNT_POINT = "/sdcard";

/**
 * @brief SD カードを SPI モードで MOUNT_POINT にマウントする
 *
 * マウント済みなら何もせず true を返す。
 * 失敗したときは、ログに出し、理由を err に書く（画面には出さない。呼び出し側が出す）。
 *
 * @param[out] err     失敗の理由（改行区切り）を書くバッファ
 * @param[in]  errSize err のバイト数
 * @return マウントできたら true
 */
bool begin(char *err, size_t errSize);

}  // namespace sdcard
