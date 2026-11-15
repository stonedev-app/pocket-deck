# ドキュメント

pocket-deck の文書の目次。内容ごとに、次の 3 つの区分に分ける。

- `architecture/`: ソースの構成、タスクやモジュールの役割分担など、アプリの内部設計を書く。
- `hardware/`: Cardputer-Adv のハードウェアと、その周辺機器の使い方を書く。
- `guide/`: SD カードの準備やキー操作など、使い方を書く。

## 現在ある文書

- [architecture/](architecture/overview.md): アプリの内部設計
  - [全体の構成](architecture/overview.md): 層の構成と、各モジュールの役割
  - [タスク](architecture/tasks.md): タスクの優先度・コア・スタックと、その理由。タスクの間をつなぐキューや通知
  - [処理の流れ](architecture/sequences.md): 起動・再生・音量などのシーケンス図
- [guide/](guide/usage.md): 使い方
  - [使い方](guide/usage.md): SD カードの準備、キー操作、画面の見方
- [hardware/](hardware/README.md): Cardputer-Adv のハード構成の早見表と、機器ごとの説明
  - [ディスプレイ](hardware/display.md)
  - [キーボード](hardware/keyboard.md)
  - [スピーカー](hardware/speaker.md)
  - [SD カード](hardware/sdcard.md)
  - [電池](hardware/battery.md)
