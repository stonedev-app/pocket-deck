# ハードウェア

M5Stack Cardputer-Adv（ESP32-S3）のうち、pocket-deckが使う機器の説明。

## 機器の一覧

| 機器 | 部品 | 接続 | 文書 |
|------|------|------|------|
| ディスプレイ | 240×135の液晶 | SPI | [display.md](display.md) |
| キーボード | TCA8418 | I2C | [keyboard.md](keyboard.md) |
| スピーカー | コーデックES8311 | I2S | [speaker.md](speaker.md) |
| SDカード | SDカードスロット | SPI | [sdcard.md](sdcard.md) |
| 電池 | ― | M5Unified経由 | [battery.md](battery.md) |

## 前提

- SoCはESP32-S3FN8。フラッシュは8MBで、PSRAMはない。
- ディスプレイ・スピーカー・電池は、M5Unifiedの`M5.begin()`で初期化される。使う前に呼ぶ。
- 初期化の順序は、[sequences.md](../architecture/sequences.md)を参照。

## 参考資料

- M5Cardputer-UserDemo（Adv用の`CardputerADV`ブランチ）: https://github.com/m5stack/M5Cardputer-UserDemo/tree/CardputerADV
  - `main/hal/hal_config.h`: ピン定義
  - `main/hal/hal.cpp`: SPIバスとSDカードのマウント
  - `main/hal/keyboard/`、`main/hal/utils/adafruit_tca8418/`: キーボードのドライバ
- M5Unified: https://github.com/m5stack/M5Unified（`src/M5Unified.inl`にボードごとのピンとコーデックの設定がある）
- M5GFX: https://github.com/m5stack/M5GFX
- 製品ページ: https://docs.m5stack.com/en/products/sku/K132-Adv
- TCA8418（TI）: https://www.ti.com/product/TCA8418
