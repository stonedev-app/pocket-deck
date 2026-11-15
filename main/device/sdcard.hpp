#pragma once

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
 * 失敗したときは、画面とログにエラーを出す（この時点では ui のタスクがなく、画面を使えるため）。
 *
 * @return マウントできたら true
 */
bool begin();

}  // namespace sdcard
