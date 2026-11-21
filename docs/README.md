# 文書の目次

## architecture/

アプリの内部設計を書く。

| 文書 | 内容 |
|------|------|
| [全体の構成](architecture/overview.md) | 層の構成と、ファイルごとの役割 |
| [タスク](architecture/tasks.md) | タスクの優先度・コア・スタックとその理由、タスクの間のキューや通知 |
| [処理の流れ](architecture/sequences.md) | 起動・再生・音量などのシーケンス図 |

## guide/

使う人向けの説明を書く。

| 文書 | 内容 |
|------|------|
| [使い方](guide/usage.md) | SDカードの準備、キー操作、画面構成 |

## hardware/

Cardputer-Advのうち、pocket-deckが使う部品の説明を書く。

| 文書 | 内容 |
|------|------|
| [ハード構成の早見表](hardware/README.md) | 部品と接続の一覧、共通の注意点、参照元 |
| [ディスプレイ](hardware/display.md) | 液晶の仕様と、画面への転送 |
| [キーボード](hardware/keyboard.md) | TCA8418のレジスタ、キー配列、Fn層 |
| [スピーカー](hardware/speaker.md) | 初期化、WAVの再生、音量 |
| [SDカード](hardware/sdcard.md) | SPIのピンと、マウントの設定 |
| [電池](hardware/battery.md) | 残量の読み方と、充電状態が取れない注意 |
