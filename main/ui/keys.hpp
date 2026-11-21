#pragma once

#include <stdint.h>

#include "lvgl.h" // IWYU pragma: keep

// LVGL のキーコードのうち、アプリ固有の意味を持たせたもの。
// ui.cpp が操作（input::Action）から変換し、now_playing.cpp が受け取る。
//
// 項目間の選択の移動は LV_KEY_NEXT / LV_KEY_PREV で行う（LV_KEY_UP / DOWN は項目の中身の
// スクロールに使われ、選択は動かない）。再生中の画面ではフォーカスを固定しているので、
// LV_KEY_NEXT / PREV は使えず、画面のイベントまで届く別のキーに割り当てる。
namespace ui::keys {

inline constexpr uint32_t PLAY_PAUSE = ' ';
inline constexpr uint32_t NEXT_TRACK = LV_KEY_RIGHT;
inline constexpr uint32_t PREV_TRACK = LV_KEY_LEFT;
inline constexpr uint32_t REPEAT = 'r';

}  // namespace ui::keys
