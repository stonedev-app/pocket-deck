#pragma once

#include <string>
#include <vector>

#include "wav.hpp"

// 画面の描画と、描画が使う状態。描画はメインタスクだけで行う
namespace ui {

/** @brief 描画が使う状態 */
struct State {
    std::vector<std::string> files;  // 一覧に出す WAV のファイル名
    int selected = 0;                // 一覧で選んでいる位置

    // 結果画面に出している WAV の解析結果
    wav::Info resultInfo;
    bool resultOk = false;
    char resultErr[40] = "";
    bool resultPlaying = false;  // 画面に出している状態。player::isPlaying() との差で終了を知る
};

/** @brief 描画が使う状態。更新は呼び出し側で行い、その後 drawList() / drawResult() で描き直す */
extern State state;

/** @brief 一覧画面を描く。見出しに音量を出し、選択中の行に印を付ける */
void drawList();

/** @brief 結果画面を描く。WAV の形式（解析に失敗したときは理由）と、再生中か停止中か、音量を出す */
void drawResult();

}  // namespace ui
