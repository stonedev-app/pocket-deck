# pocket-deck

M5Stack Cardputer-Adv（ESP32-S3）向けのESP-IDFプロジェクトです。
音楽再生やPodcastなどの音声再生アプリを、複数切り替えて動かすことを目指しています。
キー操作はvim風にする予定です。

現在は、SDカードのルートに置いたWAV（16bit PCM）を一覧から選んで再生できます（一時停止、次の曲・前の曲、1曲リピート、音量の調整）。

ソースは`main/`の下で、次のように分かれています。

- `app/`: 再生の制御、曲の一覧、キーの割り当て
- `audio/`: WAVの再生、音量、イコライザーの計算
- `device/`: キーボード、SDカード、電池、ディスプレイ
- `ui/`: LVGLの画面（`screens/`）と部品（`widgets/`）

## 環境の準備

ESP-IDFは、ESP-IDF Installation Manager（EIM）でインストールし、使うバージョンを選択します。
EIMの詳細は[公式ドキュメント](https://docs.espressif.com/projects/idf-im-ui/en/latest/)を参照してください。

インストール済みのバージョンと、選択中のバージョンは次のコマンドで確認できます。

```sh
eim list
```

v5.5.6がインストール済みの場合は、次の`eim install`は不要です。

```sh
eim install -i v5.5.6
```

使うバージョンを選択します。

```sh
eim select v5.5.6
```

## 依存ライブラリ

[M5Unified](https://github.com/m5stack/M5Unified)（ディスプレイ描画のM5GFXも依存として含まれます）を使っています。
`main/idf_component.yml`に記載があり、初回の`idf.py build`で`managed_components/`に自動でダウンロードされます。ダウンロードにはネットワーク接続が必要です。

## ビルド・書き込み・モニタ

ターゲットは`sdkconfig.defaults`でesp32s3に設定済みのため、`idf.py set-target`は不要です。

書き込みは、側面の電源スイッチをOFFにした状態で、USB-CケーブルでPCに接続して行います。

```sh
eim run "idf.py build"
eim run "idf.py flash monitor"
```

モニタの終了は`Ctrl+]`。

## ドキュメント

文書の一覧は[docs/README.md](docs/README.md)を参照。
