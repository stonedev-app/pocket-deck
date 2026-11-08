#pragma once

#include <cstdint>

#include "freertos/FreeRTOS.h"  // IWYU pragma: keep

// ミリ秒を tick に変換する。ms > 0 のときは必ず 1 tick 以上を返す。
//
// CONFIG_FREERTOS_HZ=100（1 tick = 10ms）では、pdMS_TO_TICKS() が 10ms 未満を 0 に丸める。
// vTaskDelay(0) は待たずに戻るため、待ちループで使うと IDLE タスクが走れず、
// タスクウォッチドッグが発火する。
//
// 使い方: 待ち時間には pdMS_TO_TICKS() ではなく msToTicks() を使う。
//   vTaskDelay(msToTicks(5));  // 5ms は 1 tick（10ms）になる
//
// 10ms 以上では pdMS_TO_TICKS() と同じ値を返す。ms == 0 は 0 のまま（待たない指定）。
constexpr TickType_t msToTicks(uint32_t ms) {
    const TickType_t ticks = pdMS_TO_TICKS(ms);
    return (ms > 0 && ticks == 0) ? 1 : ticks;
}
