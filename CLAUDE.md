# pocket-deck

M5Stack Cardputer-Adv（ESP32-S3）向けの ESP-IDF v5.5.6 プロジェクト。詳細は README.md を参照。

実行環境は環境変数 `GITHUB_ACTIONS` で判断する。`true` なら「GitHub Actions 上」、それ以外は「ローカル」の節に従う。

## 共通

- 待ち時間は `pdMS_TO_TICKS` ではなく `msToTicks()`（`main/util/ticks.hpp`）を使う（HZ=100 で 10ms 未満が 0 tick になるため）。
- コミットメッセージは `<type>: <日本語の要約>` の形式にする（type = feat / fix / docs / refactor / chore など）。

## ローカル

- ビルドは README のとおり `eim run "idf.py build"` で実行する。
- 作業ブランチ名は `<issueの種類>/<作業者>/issue-<番号>` にする（例: `task/claude/issue-79`）。issue の種類は小文字にし、Claude の作業では作業者を `claude` にする。
- worktree で作業するときは、`git worktree add .claude/worktrees/issue-<番号> -b <ブランチ名>` で作ってから移る。

## GitHub Actions 上

- ビルドは、リポジトリのルートで `idf.py build` をそのまま実行する。`eim` は使えない。実機がないので `flash` / `monitor` は実行しない。
  - `cd`・`-C`・`&&`・パイプ（`| tail` など）は付けない。許可設定（`--allowedTools`）と一致せず、承認待ちで実行できなくなる。
  - 裏で動かさず、完了まで待つ形で実行する。`sleep`・`until`・`pgrep` などのループで待たない（許可設定にないので拒否され、ビルドの完了前に終了してしまう）。
- Issue から何を実装すべきか読み取れない場合は、実装・コミットはせず、Issue にコメントで質問して終了する。多少曖昧でも意図がほぼ読み取れる場合は、質問せずに実装を進める。
- ファイルの編集は Edit / Write ツールで行う。`python3`・`sed`・`echo >>` などのシェルでの書き換えは、許可設定にないので使わない。
- 作業ブランチは Actions が作るので、自分では作らない。
