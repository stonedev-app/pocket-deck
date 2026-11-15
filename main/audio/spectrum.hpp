#pragma once

#include <stddef.h>
#include <stdint.h>

// イコライザー用のレベル計算。再生（player）には依存せず、渡された PCM の窓から帯域ごとのレベルを求める。
// 使い方: 起動時に begin() で計算用のタスクを作る。再生を始めるとき start()、終わるとき stop() を呼び、
// 再生中は submit() にチャンクを渡す。画面側は levels() で読み出す。
// 計算は優先度の低い専用のタスクで行い、submit() は窓をコピーして知らせるだけで待たない。

namespace spectrum {

/** @brief イコライザーの帯域数 */
constexpr int BANDS = 12;

/**
 * @brief 計算用のタスクを作る。起動時に 1 回呼ぶ
 *
 * 失敗してもレベルが常に 0 になるだけで、再生には影響しない。
 *
 * @return 成功で true
 */
bool begin();

/**
 * @brief 再生の開始を知らせる。サンプルレートに合わせて係数を作り、レベルを 0 に戻す
 *
 * @param[in] sampleRate 再生する WAV のサンプルレート [Hz]
 */
void start(uint32_t sampleRate);

/**
 * @brief 再生の終了を知らせる。計算結果の書き込みを止めてから、レベルを 0 に戻す
 *
 * 止める・終わる・一時停止のどれでも呼ぶ。
 */
void stop();

/**
 * @brief data（n 要素）の先頭の窓を計算のタスクへ渡す。再生用のタスクから呼ぶ
 *
 * 窓をコピーして通知するだけで、計算は待たない。前の計算が終わっていなければ、このチャンクは捨てる。
 *
 * @param[in] data   16bit PCM。ステレオなら L/R 交互
 * @param[in] n      要素数（ステレオなら L/R の合計）
 * @param[in] stereo ステレオなら true
 */
void submit(const int16_t *data, size_t n, bool stereo);

/**
 * @brief 各帯域のレベル（0〜100）を読み出す。低い帯域から順
 *
 * 一時停止・停止・終了のときと無音のときは 0。どのタスクから呼んでもよい。
 * 音より最大でバッファ数個分（約 0.1 秒）先の内容になる。
 */
void levels(uint8_t out[BANDS]);

}  // namespace spectrum
