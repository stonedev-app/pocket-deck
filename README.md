# pocket-deck

M5Stack Cardputer-Adv（ESP32-S3）向けの ESP-IDF プロジェクトです。
音楽再生や Podcast などの音声再生アプリを、複数切り替えて動かすことを目指しています。
キー操作は vim 風です。

## 環境の準備

ESP-IDF は EIM でインストールし、使うバージョンを選択します。

```sh
eim install -i v5.5.6
eim select v5.5.6
```

## ビルド・書き込み・モニタ

```sh
eim run "idf.py build"
eim run "idf.py flash monitor"
```

モニタの終了は `Ctrl+]`。
