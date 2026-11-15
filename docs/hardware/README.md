# ハードウェア

M5Stack Cardputer-Adv（ESP32-S3）のうち、pocket-deck が使う部品の説明。機器ごとにファイルを分けている。

## ハード構成の早見表

| 機器 | 部品 / 接続 | 文書 |
|---|---|---|
| ディスプレイ | 240×135 ピクセル（横長）。M5GFX が接続を自動設定する | [display.md](display.md) |
| キーボード | TCA8418（I2C アドレス 0x34）、INT=GPIO11 | [keyboard.md](keyboard.md) |
| スピーカー | コーデック ES8311。I2S の設定は M5Unified が行う | [speaker.md](speaker.md) |
| SD カード | SPI 接続。SCLK=GPIO40、MOSI=GPIO14、MISO=GPIO39、CS=GPIO12 | [sdcard.md](sdcard.md) |
| 電池 | 残量は `M5.Power` から読む | [battery.md](battery.md) |

そのほか、次の点が前提になる。

- 内部 I2C バスは SCL=GPIO9、SDA=GPIO8。キーボード（TCA8418）がつながっている。このバスは M5Unified が `M5.In_I2C` として管理する。
- SoC は ESP32-S3FN8（フラッシュ 8MB）。`sdkconfig.defaults` で `CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y` を設定し、`partitions.csv` を 8MB 前提で組んでいる。PSRAM は使っていない（`sdkconfig.defaults` に有効化の設定がない）ので、バッファは内部 SRAM に取る。
- M5Unified が `M5.begin()` でボードを判定し、ディスプレイ・内部 I2C・スピーカー・電池を初期化する。そのため、各機器を使う前に `M5.begin()` を呼ぶ。
- 起動時の初期化の順序は `main/startup.cpp` の `startup::run()` に書いてある。`M5.begin()` → スピーカーと音量 → SD カード → キーボード → … の順。

このプロジェクトで使っていない機器（IMU・マイク・赤外線送信など）は、ここでは扱わない。

## 参照元

- M5Cardputer-UserDemo（Adv 用の `CardputerADV` ブランチ）: https://github.com/m5stack/M5Cardputer-UserDemo/tree/CardputerADV
  - `main/hal/hal_config.h`: ピン定義
  - `main/hal/hal.cpp`: SPI バスと SD カードのマウント
  - `main/hal/keyboard/`、`main/hal/utils/adafruit_tca8418/`: キーボードのドライバ
- M5Unified: https://github.com/m5stack/M5Unified （`src/M5Unified.cpp` にボードごとのピンとコーデックの設定がある）
- M5GFX: https://github.com/m5stack/M5GFX
- 製品ページ: https://docs.m5stack.com/en/products/sku/K132-Adv
- TCA8418 のデータシート（TI）
