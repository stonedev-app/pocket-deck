#pragma once

#include <stdint.h>

#include "freertos/FreeRTOS.h"  // IWYU pragma: keep

// Cardputer-Adv キーボード（TCA8418）の入力処理。表示には依存しない。
// 使い方: begin() が入力タスクとキューを作る。poll() でキューからイベントを待って取り出す。
// イベントは「押された瞬間」だけ（離上や押しっぱなしは出さない）。

namespace keyboard {

enum class Key : uint8_t {
    Char,       // 文字キー。KeyEvent::ch に ASCII 文字が入る
    Enter,
    Esc,
    Backspace,
    Delete,     // Fn + del
    Tab,
    Space,
    Up,
    Down,
    Left,
    Right,
    Fn,         // 修飾キー自体
    Shift,
    Ctrl,
    Opt,
    Alt,
};

enum : uint8_t {
    MOD_FN    = 1 << 0,
    MOD_SHIFT = 1 << 1,
    MOD_CTRL  = 1 << 2,
    MOD_OPT   = 1 << 3,
    MOD_ALT   = 1 << 4,
};

struct KeyEvent {
    Key key;
    char ch;            // key == Key::Char のときの文字（Shift / Fn 適用済み）。それ以外は 0
    uint8_t mods;       // 押された時点の修飾キーの状態（MOD_*）
    char name[8];       // 表示・ログ用のキー名（例: "a", "A", "Enter", "Esc", "Up"）

    /** @brief 修飾キー（Fn / Shift / Ctrl / Opt / Alt）自体の押下なら true */
    bool isModifier() const { return key >= Key::Fn; }
};

/**
 * @brief TCA8418 を初期化し、入力タスクとキューを作る
 *
 * M5.begin() の後に呼ぶ。
 *
 * @return 成功で true
 */
bool begin();

/**
 * @brief 押下イベントを 1 件取り出す
 *
 * begin() が成功していないときは false を返す。
 *
 * @param[out] out     取り出したイベント
 * @param[in]  timeout 待つ時間（tick 数）。portMAX_DELAY で無期限に待つ
 * @return イベントを取れたら true、時間内に取れなければ false
 */
bool poll(KeyEvent &out, TickType_t timeout);

}  // namespace keyboard
