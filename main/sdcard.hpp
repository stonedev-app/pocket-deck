#pragma once

// SD カード（SPI 接続）のマウントと、ルート直下の WAV ファイルの一覧表示。
// 使い方: M5.begin() の後に showWavList() を呼ぶ（SPI バスの初期化が M5GFX のディスプレイ用 SPI と衝突しないように）。
// 結果は画面（M5.Display）と ESP_LOGI に出す。画面に出すのは先頭の数件だけで、スクロールはしない。

namespace sdcard {

// /sdcard にマウントして、ルート直下の *.wav のファイル名を画面とログに出す。
// マウントなどに失敗したときは、画面とログにエラーを出して false を返す。
bool showWavList();

}  // namespace sdcard
