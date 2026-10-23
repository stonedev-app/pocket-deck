# pocket-deck

M5Stack Cardputer-Adv（ESP32-S3）向けの ESP-IDF v5.5.6 プロジェクト。詳細は README.md を参照。

## ビルドコマンドの実行方法

環境変数 `GITHUB_ACTIONS` で実行環境を判断する。

- `GITHUB_ACTIONS=true`（GitHub Actions 上）: `idf.py build` のように `idf.py` を直接実行する。`eim` は使えない。実機がないので `flash` / `monitor` は実行しない。
- それ以外（ローカル）: README のとおり `eim run "idf.py build"` で実行する。
