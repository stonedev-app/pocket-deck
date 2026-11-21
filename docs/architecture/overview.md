# 全体の構成

## フォルダ構成

```
main/
├── ui/                 画面
├── app/                アプリ
├── audio/              音の処理
├── device/             ハードウェア入出力
├── util/               共通処理
├── pocket-deck.cpp     app_main
├── startup.hpp         起動
└── task_config.hpp     タスク優先度・コア・スタック設定
```

## 階層構造

```mermaid
flowchart TB
    ui -->|操作| app
    ui -->|音量・イコライザーの値の取得| audio
    ui -->|画面への描画・電池残量の取得| device
    app -->|再生・音量| audio
    app -->|キー入力・曲の一覧| device
    audio -->|音を出す| device
```

| 名称 | 役割 |
|----|------|
| ui | 画面 |
| app | 曲と再生の管理 |
| audio | 音の処理 |
| device | ハードウェア入出力 |

## 各ファイル

### ui

| ファイル | 役割 |
|----------|------|
| `ui.hpp` | 画面起動 |
| `lvgl_port.hpp` | LVGL初期化と実行 |
| `lvgl_util.hpp` | LVGL補助 |
| `screens/file_list.hpp` | 曲一覧 |
| `screens/now_playing.hpp` | 再生中画面 |
| `widgets/status_bar.hpp` | ステータスバー |
| `widgets/volume_icon.hpp` | 音量表示 |
| `widgets/battery_icon.hpp` | 電池表示 |
| `widgets/eq_view.hpp` | イコライザー表示 |
| `fonts/font_jp_16.h` | 日本語フォント |
| `keys.hpp` | キー割り当て |

### app

| ファイル | 役割 |
|----------|------|
| `input.hpp` | キー入力変換 |
| `playlist.hpp` | 曲一覧 |
| `playback.hpp` | 再生制御 |

### audio

| ファイル | 役割 |
|----------|------|
| `volume.hpp` | 音量 |
| `wav.hpp` | WAV解析 |
| `player.hpp` | 再生 |
| `spectrum.hpp` | イコライザー計算 |

### device

| ファイル | 役割 |
|----------|------|
| `display.hpp` | 画面出力 |
| `keyboard.hpp` | キー入力 |
| `sdcard.hpp` | SDカード |
| `battery.hpp` | 電池残量 |
| `speaker.hpp` | スピーカー |

## その他

| 名前 | 役割 |
|------|------|
| `pocket-deck.cpp`, `startup.hpp` | 各部初期化 |
| `util/` | 共通処理 |
| `task_config.hpp` | タスク設定 |
