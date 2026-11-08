# pocket-deck

M5Stack Cardputer-Adv（ESP32-S3）向けの ESP-IDF v5.5.6 プロジェクト。詳細は README.md を参照。

## ビルドコマンドの実行方法

環境変数 `GITHUB_ACTIONS` で実行環境を判断する。

- `GITHUB_ACTIONS=true`（GitHub Actions 上）: `idf.py build` のように `idf.py` を直接実行する。`eim` は使えない。実機がないので `flash` / `monitor` は実行しない。
  - リポジトリのルートで、`idf.py build` をそのまま実行する。
  - `cd`・`-C`・`&&`・パイプ（`| tail` など）は付けない。許可設定（`--allowedTools`）と一致せず、承認待ちで実行できなくなる。
- それ以外（ローカル）: README のとおり `eim run "idf.py build"` で実行する。

## GitHub Actions 上での作業ルール

- Issue から何を実装すべきか読み取れない場合は、実装・コミットはせず、Issue にコメントで質問して終了する。
- 多少曖昧でも意図がほぼ読み取れる場合は、質問せずに実装を進める。
- コミットメッセージは `<type>: <日本語の要約>` の形式にする(type = feat / fix / docs / refactor / chore など)。

## コーディングルール

- 待ち時間は `pdMS_TO_TICKS` ではなく `msToTicks()`（`main/util/ticks.hpp`）を使う（HZ=100 で 10ms 未満が 0 tick になるため）。
