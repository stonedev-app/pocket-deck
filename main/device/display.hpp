#pragma once

#include <stdint.h>

// ディスプレイ。M5GFX への依存は、ここ（display.cpp）だけに置く。
namespace display {

/** @brief 画面の幅 [px]。M5.begin() の後に呼ぶ */
int32_t width();

/** @brief 画面の高さ [px]。M5.begin() の後に呼ぶ */
int32_t height();

/**
 * @brief 描いた領域を画面に転送する。転送は、戻るまでに終える
 *
 * @param[in] x      転送先の左上の x
 * @param[in] y      転送先の左上の y
 * @param[in] w      幅 [px]
 * @param[in] h      高さ [px]
 * @param[in] pixels RGB565（16bit、ホストのバイト順）の画素。w * h 個、行の順に並ぶ
 */
void flush(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *pixels);

}  // namespace display
