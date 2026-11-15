# 全体の構成

`main/` の層の構成と、各モジュールの役割をまとめる。タスクの一覧は [tasks.md](tasks.md)、処理の流れは [sequences.md](sequences.md) を見る。

## 層の構成

依存は上から下へ向かい、下の層は上の層を呼ばない。

```
ui → app → audio / device → util
```

| 層 | ディレクトリ | 役割 |
|----|--------------|------|
| 起動 | `pocket-deck.cpp`, `startup.cpp` | `app_main()` から `startup::run()` を呼び、各モジュールを順に初期化する |
| ui | `ui/` | 画面（LVGL）。表示と、キー入力の橋渡しをする |
| app | `app/` | アプリの判断。曲の一覧、再生の状態、キーの意味づけ。LVGL には依存しない |
| audio | `audio/` | 音。WAV の解析、再生、イコライザーの計算、音量 |
| device | `device/` | ハードウェアの入出力。ディスプレイ、キーボード、SD カード、電池 |
| util | `util/` | 共通の小さな道具（`msToTicks()`） |

`task_config.hpp` は層の外にあり、どの層からも読める。タスクの優先度・コア・スタックを集めた設定。

## モジュールの役割

### 起動

- `startup::run()`: 初期化の順序を持つ。失敗したら `ui::showError()` で画面に出して止まる。

### ui

- `ui::start()`: LVGL を起動し、画面を作る。キーの変換（`input::Action` → LVGL のキー）もここにある。
- `lvgl_port`: LVGL の初期化、描画バッファ、キー入力デバイス、LVGL のタスク（`lvglTask`）。
- `screens/file_list`: 曲の一覧。行が選ばれたら `ui.cpp` の `onSelect()` に知らせる。
- `screens/now_playing`: 再生中の画面。曲名、バー、状態、時間、リピートの表示と、再生中のキー操作。
- `widgets/status_bar`: 画面上の帯。`volume_icon` と `battery_icon` を並べる。
- `widgets/eq_view`: イコライザーの棒。`spectrum::levels()` を周期的に読んで描く。
- `keys.hpp`: 再生中の画面で使う、LVGL のキーコードの割り当て。

### app

- `input::poll()`: `keyboard::poll()` で 1 件取り、`Action` に変える。音量のキー（Fn + A / S / D）はここで `volume::` を呼んで処理する。
- `playlist`: SD カードのルート直下の `*.wav` の名前を集め、並べて持つ（`load()` / `count()` / `nameAt()` / `pathAt()`）。
- `playback`: 再生の状態（`Playing` / `Paused` / `Stopped`）、今の曲の位置、1 曲リピート。`player::` を呼んで再生を制御し、終了通知を `update()` で処理する。LVGL のタスクからだけ呼ぶ（排他制御はしない）。

### audio

- `speaker`: スピーカーのタスクの優先度とコアを設定して起動し、音量を復元する。
- `volume`: 音量 17 段階（0〜16）と消音。段階は NVS に保存する。
- `wav`: WAV のヘッダを解析する（`parse()`）。
- `player`: 再生用のタスクを作り、ファイルを読んで `M5.Speaker.playRaw()` に渡す。終わると終了通知をキューに入れる。
- `spectrum`: イコライザーの計算。再生用のタスクから窓を受け取り、計算用のタスクで 12 帯域のレベルを求める。

### device

- `display`: ディスプレイへの転送（`flush()`）と大きさ。
- `keyboard`: TCA8418 の入力タスク。割り込みで起き、押下イベントをキューに入れる（`poll()` で取り出す）。
- `sdcard`: SD カードを SPI モードで `/sdcard` にマウントする。
- `battery`: 電池の残量。帯の `battery_icon` が読む。

### util

- `ticks.hpp`: `msToTicks()`。待ち時間は `pdMS_TO_TICKS` ではなくこれを使う（`CONFIG_FREERTOS_HZ=100` で 10ms 未満が 0 tick になるため）。

## 状態の持ち主

| 状態 | 持ち主 | 読み書きするタスク |
|------|--------|--------------------|
| 曲の一覧 | `playlist` | 起動時に書き、あとは読むだけ |
| 再生の状態・位置・リピート | `playback` | LVGL のタスクだけ |
| 再生の実行（読み出し位置など） | `player` | 再生用のタスクと、`playback` を呼ぶ LVGL のタスク |
| イコライザーのレベル | `spectrum` | 計算用のタスクが書き、LVGL のタスクが読む |
| 音量の段階・消音 | `volume` | LVGL のタスクが書き、帯の更新も同じタスクが読む |
