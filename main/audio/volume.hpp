#pragma once

// 音量（0〜16 の 17 段階）の管理。setVolume に渡す値は volume.cpp の表から引く。段階は NVS に保存し、起動時に復元する。
namespace volume {

/** @brief 音量の最大段階。0 を無音とした 17 段階 */
constexpr int MAX_LEVEL = 16;

/** @brief NVS に保存がない初回起動時の段階。最大では大きすぎるので、低めにしてある */
constexpr int DEFAULT_LEVEL = 4;

/** @brief NVS から段階を読んでスピーカーに反映する。失敗しても既定値で続ける。speaker::begin() の後に呼ぶ */
void begin();

/** @brief 1 段上げる。消音中なら解除してから上げる。端では止まる */
void up();

/** @brief 1 段下げる。消音中なら解除してから下げる。端では止まる */
void down();

/** @brief 消音と解除を切り替える。保存はしない */
void toggleMute();

/** @brief 現在の段階（0〜16）。消音中も覚えている段階を返す */
int level();

/** @brief 消音中か */
bool muted();

}  // namespace volume
