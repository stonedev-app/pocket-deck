# pocket-deck

M5Stack Cardputer-Adv（ESP32-S3）向けの ESP-IDF プロジェクトです。
音楽再生や Podcast などの音声再生アプリを、複数切り替えて動かすことを目指しています。
キー操作は vim 風にする予定です。

現在は設計検討の段階で、動作確認用のサンプルコード（1秒ごとにカウントをログ出力するだけ）のみです。

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

## ビルド・書き込み・モニタ

ターゲットは `sdkconfig.defaults` で esp32s3 に設定済みのため、`idf.py set-target` は不要です。

書き込みは、側面の電源スイッチを OFF にした状態で、USB-C ケーブルで PC に接続して行います。

```sh
eim run "idf.py build"
eim run "idf.py flash monitor"
```

モニタの終了は `Ctrl+]`。
