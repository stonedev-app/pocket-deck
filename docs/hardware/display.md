# ディスプレイ

## 部品とつなぎ方

- 240×135 ピクセルの横長の液晶。`M5.begin()` の直後の既定の向きで、`width()` = 240、`height()` = 135 になる（実機で確認）。
- 接続するピンは、M5Unified / M5GFX がボードを自動判定して設定する。
- 画素の形式は RGB565（16bit）。

## このプロジェクトでの使い方

ソースは `main/device/display.hpp` / `main/device/display.cpp`。M5GFX への依存は、このファイルだけに置いている。

| 関数 | 役割 |
|---|---|
| `display::width()` / `display::height()` | 画面の幅と高さ。`M5.begin()` の後に呼ぶ |
| `display::flush(x, y, w, h, pixels)` | 描いた領域（RGB565）を画面に転送する。`M5.Display.pushImage()` を `startWrite()` / `endWrite()` で挟み、戻るまでに転送を終える |

描画は LVGL が行う。`main/ui/ui.cpp` が `display::flush` を `lvgl_port::start()` に渡し、LVGL の描画バッファの内容がこの関数で画面に出る。LVGL の描画バッファは、画面全体（135 行）ではなく 40 行ぶんで、内部 SRAM に取る（`main/ui/lvgl_port.cpp` の `BUF_LINES`）。

初期化に失敗したときだけ、LVGL を起動する前なので、`main/ui/ui.cpp` の `ui::showError()` が `M5.Display` へ直接文字を出す。

## 注意点

- `display::width()` などは、`M5.begin()` の後でないと正しい値を返さない。
- `flush()` は転送が終わってから戻る前提で、LVGL 側が戻った直後に `lv_display_flush_ready()` を呼ぶ。非同期の転送にするときは、この前提を直す必要がある。
- SD カードの SPI バスは、`M5.begin()` の後に初期化する。ディスプレイ用の SPI と衝突しないようにするため（[sdcard.md](sdcard.md)）。
