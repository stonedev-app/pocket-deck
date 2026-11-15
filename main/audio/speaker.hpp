#pragma once

// スピーカー（ES8311 コーデック。I2S は M5Unified が設定する）の初期化。
namespace speaker {

/**
 * @brief スピーカーのタスクの優先度とコアを設定して起動し、音量を復元する
 *
 * M5.begin() の後に、起動時に 1 回だけ呼ぶ。volume::begin() は M5.Speaker.begin() の後でないと
 * 音量が反映されないので、ここで続けて呼ぶ。
 * M5.Speaker.begin() は開始済みだと何もしないので、end() で止めてから設定し直す。
 */
void begin();

}  // namespace speaker
