#pragma once

#include <string>
#include <vector>

// SD カード（SPI 接続）のマウントと、ルート直下の WAV ファイルの一覧の取得。表示はしない。
// 使い方: M5.begin() の後に begin() を 1 回呼ぶ（SPI バスの初期化が M5GFX のディスプレイ用 SPI と衝突しないように）。
// その後、listWav() で一覧を取り、選んだ名前を wavPath() でパスにする。

namespace sdcard {

/**
 * @brief SD カードを SPI モードで /sdcard にマウントする
 *
 * マウント済みなら何もせず true を返す。
 * 失敗したときは、画面とログにエラーを出す（この時点では ui のタスクがなく、画面を使えるため）。
 *
 * @return マウントできたら true
 */
bool begin();

/**
 * @brief ルート直下の *.wav のファイル名をすべて names に入れる
 *
 * 見つけた総数はログに出す。サブディレクトリは辿らない。
 *
 * @param[out] names 取得したファイル名（パスは含まない）。呼び出しのたびに空にしてから入れる
 * @return 成功で true。ディレクトリを開けないときは false
 */
bool listWav(std::vector<std::string> &names);

/**
 * @brief 一覧のファイル名から、ファイルのフルパスを作る
 *
 * @param[in] name listWav() が返したファイル名
 * @return "/sdcard/<name>"
 */
std::string wavPath(const std::string &name);

}  // namespace sdcard
