# SD カード

## 部品とつなぎ方

SD カードは、SDMMC ではなく、SPI モードで配線されている。

| 信号 | ピン |
|---|---|
| SCLK | GPIO40 |
| MOSI | GPIO14 |
| MISO | GPIO39 |
| CS | GPIO12 |

M5Unified に SD カードの機能はないので、ESP-IDF の API（SDSPI ホストと FAT）を直接使う。

## このプロジェクトでの使い方

ソースは `main/device/sdcard.hpp` / `main/device/sdcard.cpp`。

- `sdcard::begin(err, errSize)` が、SPI バスを初期化して、`/sdcard`（`sdcard::MOUNT_POINT`）にマウントする。マウント済みなら何もせず `true` を返す。
- `startup::run()`（`main/startup.cpp`）が、`M5.begin()` の後に 1 回呼ぶ。失敗したときは、理由が `err` に入り、`startup.cpp` の `fail()` が画面とログに出す。
- マウント後は、`fopen("/sdcard/...")` などの標準のファイル API で読み書きできる。曲の一覧は `main/app/playlist.cpp` が `/sdcard` から WAV を探して作る。WAV のヘッダは `main/audio/wav.cpp` が解析する。

### マウントの設定

- SPI バス: `spi_bus_initialize()` に、上の 3 本のピン、`quadwp_io_num = -1`、`quadhd_io_num = -1`（クアッド SPI ではない）、`max_transfer_sz = 4000`、`SDSPI_DEFAULT_DMA` を渡す。
- マウント: `esp_vfs_fat_sdspi_mount()` に、`SDSPI_HOST_DEFAULT()` のホスト、`gpio_cs = GPIO_NUM_12` のスロット、次の設定を渡す。
  - `format_if_mount_failed = false`
  - `max_files = 5`
  - `allocation_unit_size = 16 * 1024`
- 長いファイル名と UTF-8 のファイル名を読めるように、`sdkconfig.defaults` で `CONFIG_FATFS_LFN_HEAP=y` と `CONFIG_FATFS_API_ENCODING_UTF_8=y` を設定している。

### 失敗の見分け方

- `spi_bus_initialize()` の失敗は、`SPI init failed` として返す。
- マウントの失敗では、`ESP_FAIL` がファイルシステムのマウント失敗（`filesystem`）、それ以外がカードの初期化失敗（`card init`）。どちらも `esp_err_t` を、ログと理由の文字列に出す。

## 注意点

- **`M5.begin()` の後に呼ぶ**。SPI バスの初期化が、M5GFX のディスプレイ用の SPI と衝突しないようにするため。
- `format_if_mount_failed = false` なので、未フォーマットのカードは自動では初期化されない。カード上の WAV を誤って消さないための設定。
- マウントに失敗したときは、`spi_bus_free()` で SPI バスを解放する。しないと、再度 `begin()` したときに `spi_bus_initialize()` が失敗する。
- SD の読み出しは CPU を使い続けるので、再生用のタスクは、描画のタスクとは別のコア（コア 0）に置く（`main/task_config.hpp` の `PLAYER`）。
