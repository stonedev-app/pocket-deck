#pragma once

#include <string>

// 曲一覧のデータ。SD カードのルート直下の WAV のファイル名を持つ。表示はしない。
// 使い方: sdcard::begin() の後、画面を作る前に load() を 1 回呼ぶ。
namespace playlist {

/**
 * @brief ルート直下の *.wav のファイル名を取って、一覧を作り直す
 *
 * 見つけた総数はログに出す。サブディレクトリは辿らない。
 * 並び順は、大文字小文字を区別しない昇順。同じなら、バイト順
 * （日本語は UTF-8 のバイト順で、五十音順にはならない）。
 * ディレクトリを開けないときは、ログにエラーを出して 0 件にする。
 */
void load();

/** @brief 一覧の曲数 */
int count();

/** @brief 一覧の index 番目のファイル名（パスは含まない）。範囲外なら nullptr */
const char *nameAt(int index);

/** @brief 一覧の index 番目のファイルのフルパス（"/sdcard/<name>"）。範囲外なら空文字列 */
std::string pathAt(int index);

}  // namespace playlist
