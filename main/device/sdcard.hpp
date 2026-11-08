#pragma once

#include <string>
#include <vector>

// SD カード（SPI 接続）のマウントと、ルート直下の WAV ファイルの一覧の取得。表示はしない。
// 使い方: M5.begin() の後に begin() を 1 回呼ぶ（SPI バスの初期化が M5GFX のディスプレイ用 SPI と衝突しないように）。
// その後、listWav() で一覧を取り、選んだ名前を wavPath() でパスにする。

namespace sdcard {

// 一覧に入れるファイル名の最大数。画面に 7 行しか入らないので、それを超える分は選べない
constexpr int MAX_WAV_FILES = 7;

// /sdcard にマウントする。失敗したときは、画面とログにエラーを出して false を返す。
bool begin();

// ルート直下の *.wav のファイル名を最大 MAX_WAV_FILES 件、names に入れる。見つけた総数はログに出す。
// ディレクトリを開けないときは false を返す。
bool listWav(std::vector<std::string> &names);

// 一覧のファイル名から、ファイルのフルパスを作る。
std::string wavPath(const std::string &name);

}  // namespace sdcard
