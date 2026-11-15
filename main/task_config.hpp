#pragma once

#include <stdint.h>

#include "freertos/FreeRTOS.h"  // IWYU pragma: keep

// タスクの優先度・コア・スタックを、ここに集める。
//
// 優先度の方針: 音の経路（スピーカー・再生用）> 描画（LVGL）・キーボード > イコライザーの計算。
// コアの方針: 音の経路（スピーカー・再生用・イコライザーの計算）はコア 0、描画（LVGL）はコア 1。
// 指定しないと描画も音もコア 0 に集まり、再生中は描画が間に合わない（実機で計測、#57）。
namespace task_config {

// 動かすコアを指定しないときの値（OS が選ぶ）
inline constexpr BaseType_t CORE_ANY = tskNO_AFFINITY;

struct Spec {
    UBaseType_t priority;
    BaseType_t core;
    uint32_t stackBytes;
};

// LVGL のタスク。描画のために、音の経路とは別のコア 1 に置く
inline constexpr Spec LVGL = {5, 1, 8192};

// キーボードの入力タスク。I2C の読み書きとキューへの送信しかしないので、スタックは 4KB で足りる。
// 優先度は LVGL と同じ。ISR の通知で起きたときだけ動くので CPU はほとんど使わない
inline constexpr Spec KEYBOARD = {5, CORE_ANY, 4096};

// 再生用のタスク。ファイル読み出しと playRaw の呼び出しだけなので 4KB で足りる。
// 優先度はスピーカーのタスクと同じ 6（LVGL の 5 より高い）。再開後は SD の読み出しが遅く、
// 同じ 5 だと LVGL の描画に CPU を取られて、音のキューが空になるため。
// SD の読み出しは CPU を使い続けるので、描画と同じコアだと描画が間に合わない
inline constexpr Spec PLAYER = {6, 0, 4096};

// イコライザーの計算のタスク。1 回に約 6ms（最大約 12ms、実機で計測）かかるので、
// 優先度を音の経路より低い 1 にして、音を優先する。
// 計算が間に合わなければ、そのチャンクは捨てる（棒の動きが粗くなるだけ）
inline constexpr Spec SPECTRUM = {1, 0, 3072};

// スピーカーのタスク（M5Unified が作る。スタックは M5Unified の既定のままなので 0）。
// 優先度を LVGL・キーボードより高い 6 にする。既定の 2 では、イコライザーの描画が忙しいときに
// CPU を取れず、音が途切れた（実機で確認）
inline constexpr Spec SPEAKER = {6, 0, 0};

}  // namespace task_config
