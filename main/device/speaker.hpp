#pragma once

#include <stddef.h>
#include <stdint.h>

// スピーカー（ES8311 コーデック。I2S は M5Unified が設定する）。M5.Speaker を包む。M5.begin() の後に使う。
namespace speaker {

/**
 * @brief スピーカーのタスクの優先度とコアを設定して起動する
 *
 * M5.begin() の後に、起動時に 1 回だけ呼ぶ。setVolume() は begin() の後でないと反映されない。
 * M5.Speaker.begin() は開始済みだと何もしないので、end() で止めてから設定し直す。
 */
void begin();

/** @brief 音量を設定する @param v 0（無音）〜255 */
void setVolume(uint8_t v);

/**
 * @brief 16bit PCM を再生キューに積む。キューに空きがないときは、空きが出るまで待つ
 *
 * data は鳴り終わるまで参照され続けるので、その間は書き換えない。
 *
 * @param data 16bit PCM
 * @param count 要素数（バイト数ではない）
 * @param sampleRate サンプリング周波数 [Hz]
 * @param stereo ステレオ（L/R 交互）なら true
 * @param channel チャンネル番号
 */
void play(const int16_t *data, size_t count, uint32_t sampleRate, bool stereo, uint8_t channel);

/** @brief channel が再生中（キューに残りがある）か */
bool isPlaying(uint8_t channel);

/** @brief channel の再生を止め、キューのデータを捨てる */
void stop(uint8_t channel);

}  // namespace speaker
