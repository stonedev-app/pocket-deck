# pocket-deck

M5Stack Cardputer-Adv（ESP32-S3）向けの ESP-IDF プロジェクトです。
音楽再生や Podcast などの音声再生アプリを、複数切り替えて動かすことを目指しています。
キー操作は vim 風にする予定です。

現在は、SD カードのルートに置いた WAV（16bit PCM）を一覧から選んで再生できます（一時停止、次の曲・前の曲、1 曲リピート、音量の調整）。

ソースは `main/` の下で、次のように分かれています。

- `app/`: 再生の制御、曲の一覧、キーの割り当て
- `audio/`: WAV の再生、音量、イコライザーの計算
- `device/`: キーボード、SD カード、電池、ディスプレイ
- `ui/`: LVGL の画面（`screens/`）と部品（`widgets/`）

## 環境の準備

ESP-IDF は、ESP-IDF Installation Manager（EIM）でインストールし、使うバージョンを選択します。
EIM の詳細は[公式ドキュメント](https://docs.espressif.com/projects/idf-im-ui/en/latest/)を参照してください。

インストール済みのバージョンと、選択中のバージョンは次のコマンドで確認できます。

```sh
eim list
```

v5.5.6 がインストール済みの場合は、次の `eim install` は不要です。

```sh
eim install -i v5.5.6
```

使うバージョンを選択します。

```sh
eim select v5.5.6
```

## 依存ライブラリ

[M5Unified](https://github.com/m5stack/M5Unified)（ディスプレイ描画の M5GFX も依存として含まれます）を使っています。
`main/idf_component.yml` に記載があり、初回の `idf.py build` で `managed_components/` に自動でダウンロードされます。ダウンロードにはネットワーク接続が必要です。

## ビルド・書き込み・モニタ

ターゲットは `sdkconfig.defaults` で esp32s3 に設定済みのため、`idf.py set-target` は不要です。

書き込みは、側面の電源スイッチを OFF にした状態で、USB-C ケーブルで PC に接続して行います。

```sh
eim run "idf.py build"
eim run "idf.py flash monitor"
```

モニタの終了は `Ctrl+]`。

## ドキュメント

- [Cardputer-Adv 周辺機器の使い方](docs/cardputer-adv-peripherals.md): ディスプレイ・キーボード・IMU・スピーカー・マイク・赤外線送信を使うための情報
