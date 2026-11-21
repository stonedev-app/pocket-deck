# ハードウェア

M5Stack Cardputer-Adv（ESP32-S3）のうち、pocket-deckが使う部品の説明。機器ごとにファイルを分けている。

## ハード構成の早見表

| 機器 | 部品 / 接続 | 文書 |
|---|---|---|
| ディスプレイ | 240×135ピクセル（横長）。M5GFXが接続を自動設定する | [display.md](display.md) |
| キーボード | TCA8418（I2Cアドレス0x34）、INT=GPIO11 | [keyboard.md](keyboard.md) |
| スピーカー | コーデックES8311。I2Sの設定はM5Unifiedが行う | [speaker.md](speaker.md) |
| SDカード | SPI接続。SCLK=GPIO40、MOSI=GPIO14、MISO=GPIO39、CS=GPIO12 | [sdcard.md](sdcard.md) |
| 電池 | 残量は`M5.Power`から読む | [battery.md](battery.md) |

そのほか、次の点が前提になる。

- 内部I2CバスはSCL=GPIO9、SDA=GPIO8。キーボード（TCA8418）がつながっている。このバスはM5Unifiedが`M5.In_I2C`として管理する。
- SoCはESP32-S3FN8（フラッシュ8MB）。`sdkconfig.defaults`で`CONFIG_ESPTOOLPY_FLASHSIZE_8MB=y`を設定し、`partitions.csv`を8MB前提で組んでいる。この型番はPSRAMを内蔵しないので、バッファは内部SRAMに取る。
- M5Unifiedが`M5.begin()`でボードを判定し、ディスプレイ・内部I2C・スピーカー・電池を初期化する。そのため、各機器を使う前に`M5.begin()`を呼ぶ。
- 起動時の初期化の順序は`main/startup.cpp`の`startup::run()`に書いてある。`M5.begin()` → スピーカーと音量 → SDカード → キーボード → … の順。

このプロジェクトで使っていない機器（IMU・マイク・赤外線送信など）は、ここでは扱わない。

## 参照元

- M5Cardputer-UserDemo（Adv用の`CardputerADV`ブランチ）: https://github.com/m5stack/M5Cardputer-UserDemo/tree/CardputerADV
  - `main/hal/hal_config.h`: ピン定義
  - `main/hal/hal.cpp`: SPIバスとSDカードのマウント
  - `main/hal/keyboard/`、`main/hal/utils/adafruit_tca8418/`: キーボードのドライバ
- M5Unified: https://github.com/m5stack/M5Unified（`src/M5Unified.inl`にボードごとのピンとコーデックの設定がある）
- M5GFX: https://github.com/m5stack/M5GFX
- 製品ページ: https://docs.m5stack.com/en/products/sku/K132-Adv
- TCA8418のデータシート（TI）
