#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "keyboard.hpp"
#include "player.hpp"
#include "wav.hpp"

// 画面の描画と、画面モード・キー処理・再生の扱い。メインタスクだけで使う
namespace ui {

/** @brief 画面モード */
enum class Mode : uint8_t {
    List,    // WAV 一覧
    Result,  // 結果表示（再生 / 停止）
};

/** @brief 画面の状態 */
struct State {
    Mode mode = Mode::List;          // 今の画面モード
    std::vector<std::string> files;  // 一覧に出す WAV のファイル名
    int selected = 0;                // 一覧で選んでいる位置

    // 結果画面に出している WAV の解析結果
    wav::Info resultInfo;
    bool resultOk = false;
    char resultErr[40] = "";
    bool resultPlaying = false;  // 画面に出している状態。player の終了通知を受けて下ろす
};

/** @brief 画面の状態。起動時に files を埋める以外は ui の中で更新する */
extern State state;

/** @brief 一覧画面を描く。見出しに音量を出し、選択中の行に印を付ける */
void drawList();

/** @brief 結果画面を描く。WAV の形式（解析に失敗したときは理由）と、再生中か停止中か、音量を出す */
void drawResult();

/**
 * @brief キー入力を今の画面モードに応じて処理し、必要なら描き直す
 *
 * Fn + A / S / D は音量操作。一覧は j / Down・k / Up で移動、Enter で結果表示。
 * 結果表示は Enter で再生 / 停止、Esc で（再生中なら止めて）一覧に戻る。
 */
void handleKey(const keyboard::KeyEvent &ev);

/** @brief 再生終了の通知を処理する。結果表示中で再生中の表示なら、停止中に戻して描き直す */
void handleEnd(player::EndReason reason);

}  // namespace ui
