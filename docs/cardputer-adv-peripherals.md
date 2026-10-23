# Cardputer-Adv 周辺機器の使い方

M5Stack Cardputer-Adv（ESP32-S3）で、ディスプレイ・キーボード・IMU・スピーカー・マイク・赤外線送信・SD カードを使うコードを書くための情報をまとめる。

## 0. このドキュメントの前提と信頼度

- 情報源は M5Stack 公式の出荷時デモ [`m5stack/M5Cardputer-UserDemo`](https://github.com/m5stack/M5Cardputer-UserDemo) の **`CardputerADV` ブランチ**と、そこが使う M5Unified 0.2.10 のソースを直接読んだ内容。main ブランチは旧 Cardputer 用なので参照しない。
- 「確認済み」はソースで確認した事実。「未確認」は推測または知識ベースで、実機やデータシートでの検証が必要。
- 本ドキュメントのコード例は、上記ソースの使い方を元に書いたもので、**このリポジトリではまだビルド・実機確認をしていない**。
- 本プロジェクトの現状は C 言語（`main/pocket-deck.c`）、ESP-IDF v5.5.6、ターゲット esp32s3。UserDemo は ESP-IDF v5.4.2 で、バージョン差の影響は未確認。

## 1. 参照元の構成

| 項目 | 内容 |
|---|---|
| 言語 | C++（M5Unified / M5GFX が C++ のため） |
| 主な依存 | M5GFX 0.2.15、M5Unified 0.2.10（`fetch_repos.py` で `components/` に git clone） |
| IDF Component Manager | `espressif/esp-now`、`espressif/esp_tinyusb`、`jgromes/radiolib`（周辺機器の基本機能には不要） |
| 初期化の順序 | `M5.begin()` → `M5.Display.setBrightness(0)` → `M5.Speaker.begin()` → ディスプレイ用スプライト作成 → キーボード初期化 |
| 主ループ | `M5.update()` と `keyboard.update()` を毎周回呼ぶ |

本プロジェクトで使うときの注意:

- M5Unified / M5GFX を使うには、`main/pocket-deck.c` を C++（`.cpp`）にし、`app_main` を `extern "C"` で宣言する必要がある。
- M5Unified / M5GFX を IDF Component Manager 経由で入れられるかは未確認。UserDemo は `components/` に git clone している。

## 2. ハード構成の早見表

| 機能 | デバイス / 接続 | 確認 |
|---|---|---|
| ディスプレイ | ST7789V2、1.14 インチ、240×135 ピクセル。M5GFX が自動設定（ピン設定は不要） | 自動設定はソースで確認済み。パネル仕様はソース未確認で、[スイッチサイエンスの商品ページ](https://www.switch-science.com/products/10737)の記載 |
| キーボード | TCA8418（I2C アドレス 0x34）。割り込みピン GPIO11 | 確認済み |
| 内部 I2C | SCL=GPIO9、SDA=GPIO8 | 確認済み（M5Unified のピン表） |
| IMU | M5Unified の `M5.Imu` | 自動検出の仕組みは確認済み（詳細は 5 章）。搭載チップは未確認 |
| スピーカー | I2S（BCK=GPIO41、WS=GPIO43、DATA_OUT=GPIO42）、コーデック ES8311 | 確認済み |
| マイク | I2S（BCK=GPIO41、WS=GPIO43、DATA_IN=GPIO46）、コーデック ES8311 | 確認済み |
| 赤外線送信 | GPIO44（RMT、38kHz キャリア） | 確認済み。受信ハードの有無は未確認 |
| ボタン | GPIO0（`M5.BtnA` 相当） | 確認済み（M5Unified のソース） |
| SD カード / SPI | SCLK=40、MOSI=14、MISO=39、CS=12 | 確認済み（9 章） |
| メモリ | ESP32-S3FN8（フラッシュ 8MB、内蔵 SRAM 512KB）。PSRAM なし | 構成は[公式の製品ページ](https://docs.m5stack.com/en/products/sku/K132-Adv)の記載。PSRAM なしは、UserDemo の `sdkconfig.defaults` にフラッシュ 8MB の設定だけがあり SPIRAM の設定がないことと、製品ページに PSRAM の記載がないことからの判断で、データシートは未確認 |

ピン番号の出典は UserDemo の `main/hal/hal_config.h` と M5Unified の `src/M5Unified.cpp`。

## 3. ディスプレイ

`M5.begin()` の後は `M5.Display`（`M5GFX` 型）がそのまま使える。ピン設定は M5Unified がボードを自動判定して行う。

```cpp
#include <M5Unified.hpp>

extern "C" void app_main(void)
{
    M5.begin();
    M5.Display.setBrightness(128);   // 0〜255
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(0, 0);
    M5.Display.println("hello");
}
```

ちらつきを避けたいときは、スプライトに描いてから転送する。UserDemo はこの方式を使っている。

```cpp
LGFX_Sprite canvas(&M5.Display);
canvas.createSprite(M5.Display.width(), M5.Display.height());
canvas.fillScreen(TFT_BLACK);
canvas.drawString("hello", 0, 0);
canvas.pushSprite(0, 0);
```

- `M5.begin()` 直後の既定の向きは、`width()` = 240、`height()` = 135（横長）。実機で確認済み。
- `M5.Display.println`、`setCursor`、`printf` による文字表示は、実機で動作を確認した。
- UserDemo の起動時は、`setBrightness(0)` で一度消灯してから描画を準備している。
- 日本語表示（フォント選択）は未確認。M5GFX の内蔵フォントに日本語があるはずだが、調べていない。

## 4. キーボード（TCA8418）

Cardputer-Adv のキーボードは **TCA8418（I2C のキーマトリクスコントローラ）** 経由で読む。M5Unified にはキーボード機能がなく、UserDemo は自前のドライバ（`main/hal/keyboard/` と `main/hal/utils/adafruit_tca8418/`）を持っている。`M5Cardputer` ライブラリが Adv のキーボードに対応しているかは未確認。

### 4.1 初期化の流れ

1. `Adafruit_TCA8418::begin()` で全 GPIO を入力・イベント対象にする（アドレス 0x34、400kHz）。
2. `matrix(7, 8)` で 7 行 × 8 列のマトリクスに設定し、`flush()` でイベントバッファを空にする。
3. GPIO11 を入力にして `ANYEDGE` の割り込みを設定し、ISR でフラグを立てる。
4. `enableInterrupts()` を呼ぶ。

### 4.2 イベントの読み方

メインループから `update()` を呼び、フラグが立っていれば `getEvent()` で 1 イベント（1 バイト）を読む。

- 最上位ビット（0x80）が 1 なら押下、0 なら離上。
- 下位 7 ビットから 1 を引いた値を `v` とすると、`row = v / 10`、`col = v % 10`。
- 読んだ後、`INT_STAT` レジスタへ 1 を書いて割り込みフラグを消す。まだイベントが残っていれば、次の周回で続きを読む。

### 4.3 TCA8418 の座標から物理配列への変換

4.2 で得た生の `(raw_row, raw_col)` を、4 行 × 14 列の物理配列の座標 `(row, col)` へ変換する。

```cpp
col = raw_row * 2;
if (raw_col > 3) col++;
row = (raw_col + 4) % 4;
```

変換後の `(row, col)` で、UserDemo の `_key_value_map[4][14]`（`keyboard.cpp`）を引く。各キーに「通常」「Shift 時」「Fn 時」の 3 種類のコードと名前が入っている。内容は次の 4.4 を参照。

### 4.4 キー配列（変換後の座標）

各行を列 0〜13 の順に並べた。`A → B` は、A が通常時、B が Shift 時の名前。

| row | 列 0〜13 |
|---|---|
| 0 | `` ` `` → `~`、`1` → `!`、`2` → `@`、`3` → `#`、`4` → `$`、`5` → `%`、`6` → `^`、`7` → `&`、`8` → `*`、`9` → `(`、`0` → `)`、`-` → `_`、`=` → `+`、`del` |
| 1 | `tab`、`q` `w` `e` `r` `t` `y` `u` `i` `o` `p`（Shift で大文字）、`[` → `{`、`]` → `}`、`\` → `\|` |
| 2 | `fn`、`shift`、`a` `s` `d` `f` `g` `h` `j` `k` `l`（Shift で大文字）、`;` → `:`、`'` → `"`、`enter` |
| 3 | `ctrl`、`opt`、`alt`、`z` `x` `c` `v` `b` `n` `m`（Shift で大文字）、`,` → `<`、`.` → `>`、`/` → `?`、`space` |

修飾キーの位置は次のとおり。

| キー | (row, col) |
|---|---|
| Fn | (2, 0) |
| Shift（Aa） | (2, 1) |
| Ctrl | (3, 0) |
| Opt（Meta） | (3, 1) |
| Alt | (3, 2) |

### 4.5 Fn 層とキー名の決め方

Fn を押している間は、次のキーが別のコードになる。

| Fn + | キー名 | キーコード |
|---|---|---|
| `;` | up | `KEY_UP` |
| `,` | left | `KEY_LEFT` |
| `.` | down | `KEY_DOWN` |
| `/` | right | `KEY_RIGHT` |
| `` ` `` | esc | `KEY_ESC` |
| `del` | del | `KEY_DELETE`（Fn なしは `KEY_BACKSPACE`） |
| 英字 | 大文字（`Q` など） | 英字のコードのまま。`extraModifiers` に `KEY_MOD_LSHIFT` が入る |

`convertToKeyEvent()` のキー名とコードの決め方は次のとおり。

```text
Fn キー自体 (2,0)           → keyCode=KEY_NONE、keyName="fn"、isModifier=true
Fn 押下中 かつ Fn 列に定義あり → Fn 列のコードと名前（上の表）
Shift 押下中                 → Shift 時の名前とコード
それ以外                     → 通常の名前とコード
```

- Shift 時の記号（`!` `@` など）は、キーコードが通常時と同じ（例: `KEY_1`）で、**名前だけが変わる**。Shift が押されている情報は `getModifierMask()` の `KEY_MOD_LSHIFT` に入る。文字を入力するときは `keyName` を使い、HID として送るときは `keyCode` と修飾マスクを使う。
- 英字は Shift 時も `keyCode` が同じで、名前だけが大文字になる。
- Fn 列に定義がないキーは、Fn を押していても通常のキーとして扱われる。

### 4.6 アプリ側からのキー取得

`Keyboard::update()` を毎周回呼んだ後、次のどちらかで押されたキーを受け取る。

- `getLatestKeyEvent()`: 今回の `update()` で起きた 1 イベント（`state`、`keyCode`、`keyName`、`isModifier`、`extraModifiers`）。`update()` の冒頭で毎回クリアされ、イベントがなければ `keyCode == KEY_NONE` になる。
- `onKeyEvent` / `onKeyEventRaw`: イベント発生時に呼ばれるシグナル（`mooncake_log` ライブラリの `mclog::Signal`）。

`state` は押下で true、離上で false。修飾キーの状態は `getModifierMask()` で取れる。

### 4.7 キーコード

`keymap.h` の `KeScanCode_t` は USB HID のキーコードで、`KeModifierMask_t` は HID の修飾ビットと同じ。そのため、USB / BLE キーボードとして送る用途にも流用できる。

このキー配列で使われる定数名は次のとおり（`keymap.h` で確認）。

| 種類 | 定数名 |
|---|---|
| 英字 | `KEY_A` 〜 `KEY_Z` |
| 数字 | `KEY_1` 〜 `KEY_9`、`KEY_0` |
| 記号 | `KEY_GRAVE`（`` ` ``）、`KEY_MINUS`、`KEY_EQUAL`、`KEY_LEFTBRACE`、`KEY_RIGHTBRACE`、`KEY_BACKSLASH`、`KEY_SEMICOLON`、`KEY_APOSTROPHE`、`KEY_COMMA`、`KEY_DOT`、`KEY_SLASH` |
| 編集・制御 | `KEY_ENTER`、`KEY_ESC`、`KEY_BACKSPACE`、`KEY_DELETE`、`KEY_TAB`、`KEY_SPACE` |
| 方向 | `KEY_UP`、`KEY_DOWN`、`KEY_LEFT`、`KEY_RIGHT` |
| 修飾キー | `KEY_LEFTSHIFT`、`KEY_LEFTCTRL`、`KEY_LEFTALT`、`KEY_LEFTMETA` |
| 修飾ビット | `KEY_MOD_LCTRL`、`KEY_MOD_LSHIFT`、`KEY_MOD_LALT`、`KEY_MOD_LMETA`（右側版もある） |
| なし | `KEY_NONE`（イベントなし、または Fn キー自体） |

### 4.8 ライセンスの注意

`keymap.h` の冒頭には GPLv3 の著作権表記（esp32s3-keyboard 由来）がある。UserDemo 本体は MIT のため、`keymap.h` をそのままコピーして使うときはライセンスを確認する。

## 5. IMU

M5Unified の `M5.Imu` を使う。UserDemo の IMU アプリ（`main/apps/app_imu/app_imu.cpp`）の使い方は次のとおり。

```cpp
// M5.begin() が IMU を自動検出して初期化済み。通常は begin() の呼び出しは不要。

// 毎周回
if (M5.Imu.update()) {             // 新しいデータがあれば非ゼロ（sensor_mask_t）
    auto d = M5.Imu.getImuData();
    // d.accel.x / y / z   加速度
    // d.gyro.x / y / z    角速度
}
```

- `M5.begin()` は、`internal_imu` が有効で内部 I2C が使えるとき `M5.Imu.begin(&M5.In_I2C, board)` を呼ぶ（Adv で無効になる記述は見当たらない）。UserDemo はその後にもう一度 `M5.Imu.begin()` を呼んでいる。`begin()` は呼ぶたびに検出をやり直す（ソースで確認）ので害はないが、呼ぶ理由は UserDemo に書かれていない。本プロジェクトでは、まず `begin()` なしで `update()` が非ゼロを返すかを実機で確かめ、返さなければ `begin()` を呼ぶ。
- 検出は MPU6886 系 → BMI270 の順に試す。Adv に載っているチップは **未確認** で、`M5.Imu.getType()`（`imu_t`）をログに出せば分かる。
- `imu_data_t` は `accel`・`gyro`・`mag`（各 x/y/z の float）と `usec` を持つ。単位と座標軸の向きは **未確認**（ヘッダに記載なし）。画面向きに合わせた補正が必要かもしれない。軸の入れ替えは `setAxisOrder()` で指定できる。

## 6. スピーカー

M5Unified の `M5.Speaker`（`m5::Speaker_Class`）を使う。コーデック ES8311 の有効化（I2C でのレジスタ書き込み）は M5Unified が行う。

```cpp
M5.Speaker.begin();                // UserDemo は起動時に一度呼ぶ（コーデックの初期化に時間がかかる）
M5.Speaker.setVolume(128);         // 0〜255

M5.Speaker.tone(1000, 100);                  // 周波数[Hz], 長さ[ms]
M5.Speaker.playWav(wav_data, wav_size);      // 内蔵 WAV データを再生
M5.Speaker.playRaw(samples, count, 44100);   // PCM（int16_t）を再生
while (M5.Speaker.isPlaying()) { M5.delay(1); }
```

- UserDemo で使われているのは `begin` / `setVolume` / `playWav` / `playRaw` / `isPlaying` / `isEnabled` / `end` / `config().sample_rate`。`tone` は M5Unified のヘッダで存在を確認した（UserDemo では未使用）。
- `playRaw(raw_data, array_len, sample_rate = 44100, stereo = false, repeat = 1, channel = -1, stop_current_sound = false)` で、データは `int8_t` / `uint8_t` / `int16_t` の配列に対応する。`array_len` は**要素数**（バイト数ではない）。UserDemo は録音データの再生に `playRaw(data, count, 16000, false)` を使っている。
- `playRaw` に渡した配列は、再生が終わるまで書き換えず、解放もしない。ヘッダに明記はなく、再生を待たずに戻る設計であることからの推測。UserDemo は `playRaw` の後に `isPlaying()` が false になるまで待ち、その後でバッファを扱っている（`app_record.cpp`）。
- `stop_current_sound` は「true なら、現在の音の終了を待たずに新しい出力を始める」（ヘッダの説明）。実行時に生成したデータを再生するときは、バッファを 2〜3 個用意して交互に使う（ヘッダの注意書き）。
- `tone(frequency, duration = UINT32_MAX, channel = -1, stop_current_sound = true)` は、`duration` を省略すると鳴り続ける。止めるには `M5.Speaker.stop()`（全チャンネル）または `stop(channel)` を使う。どちらもヘッダで存在を確認した。
- **マイクとスピーカーは同時に使えない**（次の 7 章を参照）。

### 6.1 長い音声のチャンク再生

SD カード上の WAV のように、メモリに収まらない音声は、小さなバッファに分けて `playRaw` で順に渡す。PSRAM がないので（2 章）、バッファは内部 SRAM に取る。以下は M5Unified のヘッダの記述（`Speaker_Class.hpp`）を元に書いた例で、**ビルドも実機確認もしていない**。

```cpp
static int16_t buf[3][2048];                 // 要素数。3 個を順番に使う
int i = 0;
size_t n;
while ((n = fread(buf[i], sizeof(int16_t), 2048, fp)) > 0) {
    M5.Speaker.playRaw(buf[i], n, sample_rate, false, 1, 0);   // channel=0 を明示
    i = (i + 1) % 3;
}
while (M5.Speaker.isPlaying(0)) { M5.delay(1); }
```

- バッファの回し方はヘッダの `@attention` に従う。コールバックなしなら 3 個を順番に使う。`setBufferReleaseCallback()` で「再生側が読み終えた」通知を受けるなら、通知後に次を埋める前提で 2 個の交互利用でよい。
- チャンネルを明示すると、キューに空きがないとき、通常のタスクは空きが出るまで待つ（ヘッダの `@return` の説明）。上の例はこれに頼ってループを組んだ。この待ちの挙動は未確認。
- `isPlaying(uint8_t channel)` は 0〜7 のチャンネル番号を明示する形で、戻り値は 0=再生していない、1=再生中でキューに空きあり、2=再生中でキューに空きなし。チャンネル省略（自動選択）では使えないので、`isPlaying()` で全体の再生状態を見るか、チャンネルを明示する。
- WAV のヘッダ（`fmt ` チャンクのサンプルレート・チャンネル数・ビット幅、`data` チャンクの位置）は、自前で読む必要がある。M5Unified に、ファイルから読むための WAV パーサは見当たらない（`playWav` はメモリ上の全データを渡す方式）。ステレオは `playRaw` の `stereo` 引数で指定するが、左右のサンプルの並び方の仕様は未確認。
- 適切なバッファ長と個数は未確認。音切れが出たら、チャンクを大きくするか個数を増やす。

## 7. マイク

M5Unified の `M5.Mic`（`m5::Mic_Class`）を使う。スピーカーと同じ I2S ピンとコーデックを共有するため、**使う前にスピーカーを止める**。UserDemo の録音アプリ（`main/apps/app_record/app_record.cpp`）は次の順に切り替えている。

```cpp
// 録音開始
M5.Speaker.end();
auto cfg = M5.Mic.config();
cfg.magnification      = 128;   // 入力ゲイン
cfg.noise_filter_level = 2;
M5.Mic.config(cfg);
M5.Mic.begin();

// 録音（int16_t のバッファに溜める）。len は要素数
M5.Mic.record(buf, len, sample_rate);
while (M5.Mic.isRecording()) { M5.delay(1); }   // 録音が終わるまで待つ

// 録音終了、再生に戻す
M5.Mic.end();
M5.Speaker.begin();
M5.Speaker.setVolume(128);
```

- `record(int16_t* rec_data, size_t array_len, uint32_t sample_rate, bool stereo = false)` は `bool` を返す。`array_len` は要素数。`uint8_t*` 版もあり、サンプルレートを省略すると `config().sample_rate` を使う。
- `isRecording()` は `size_t` を返す（0=録音していない、1=録音中でキューに空きあり、2=録音中でキューに空きなし）。ヘッダの説明から、`record()` は録音要求をキューに積んで戻る非ブロッキングの設計に見える（挙動そのものは未確認）。
- UserDemo は 16000 Hz・モノラルで録音し、一定長のチャンクごとに `record()` を繰り返して配列を埋めている。再生も `playRaw(data, count, 16000, false)` で同じレートを指定している。録音と再生でサンプルレートを揃えること。

## 8. 赤外線送信

M5Unified ではなく、ESP-IDF の RMT ドライバ（`driver/rmt_tx.h`）を使う。必要なコードは、すべて UserDemo の `main/hal/utils/ir_nec/` にある。

| ファイル | 内容 |
|---|---|
| `ir_helper.c/h` | UserDemo 自前のラッパー。`ir_helper_init()` と `ir_helper_send()` |
| `ir_nec_encoder.c/h` | RMT 用の NEC エンコーダ。IDF の `ir_nec_transceiver` サンプルが元 |

- 送信ピンは GPIO44（UserDemo では `HAL_PIN_IR_TX`）。
- キャリアは 38kHz、デューティ 0.33、RMT の分解能は 1MHz（1 tick = 1µs）。
- プロトコルは NEC 形式。

```c
ir_helper_init(GPIO_NUM_44);       // 一度だけ。RMT の送信チャンネルとエンコーダを作る
ir_helper_send(addr, cmd);         // NEC フレームを 1 回送る（ループなし）
```

- `addr` と `cmd` は `uint8_t`。**反転値の付加は `ir_helper_send()` が行う**（`address = addr | (~addr << 8)`、`command` も同様）ので、呼び出し側は 8 ビットの値をそのまま渡す。
- UserDemo のアプリ（`AppRemote`、`AppStringIRToolKit`）は、`Hal::irInit()`（初期化済みかを確認して `ir_helper_init` を呼ぶ）と `Hal::irSend(addr, cmd)` 経由で送っている。
- `ir_helper_send()` は送信キューに積んで戻る（`rmt_transmit` を呼ぶだけ）。連続して送るときの間隔は UserDemo のアプリ側（`app_remote.cpp`）を見て決める。
- NEC 以外のプロトコル（SONY、家電メーカー独自形式など）は未確認。
- 受信用のハードが搭載されているかは未確認。
- GPIO44 は ESP32-S3 の UART0 の RX としても使われることがある。本プロジェクトのコンソール設定（`sdkconfig.defaults`）と競合しないかは未確認。

## 9. SD カード

M5Unified に SD カードの機能はなく、UserDemo は ESP-IDF の API（SPI モードの SD ホストと FAT）を直接使っている。出典は `main/hal/hal.cpp` の `Hal::spi_init()` と `Hal::sd_card_init()`で、コメントによれば ESP-IDF v5.3.3 の `storage/sd_card/sdspi` サンプルが元。

```cpp
#include <driver/spi_master.h>
#include <driver/sdspi_host.h>
#include <sdmmc_cmd.h>
#include <esp_vfs_fat.h>

sdmmc_host_t host = SDSPI_HOST_DEFAULT();

// 1. SPI バスの初期化（一度だけ）
spi_bus_config_t bus_cfg = {
    .mosi_io_num   = GPIO_NUM_14,
    .miso_io_num   = GPIO_NUM_39,
    .sclk_io_num   = GPIO_NUM_40,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 4000,
};
spi_bus_initialize((spi_host_device_t)host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);

// 2. SD カードのマウント
esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false, .max_files = 5, .allocation_unit_size = 16 * 1024,
    .disk_status_check_enable = false, .use_one_fat = false};
sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
slot_config.gpio_cs  = GPIO_NUM_12;
slot_config.host_id  = (spi_host_device_t)host.slot;

sdmmc_card_t* card = nullptr;
esp_err_t ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &card);
```

- マウント後は `fopen("/sdcard/...")` などの標準のファイル API で読み書きできる。
- 失敗したとき、`ESP_FAIL` はファイルシステムのマウント失敗、それ以外はカードの初期化失敗（UserDemo のログには「SD カードの線にプルアップ抵抗があるか確認」とある）。UserDemo は失敗時もバスを解放せず、再試行に備えて残している。
- UserDemo はバスの初期化を `Hal::init()` の最後（`M5.begin()` やディスプレイ・キーボードの初期化の後）で行い、マウントは SD が必要になったときに行う。`format_if_mount_failed=false` なので、未フォーマットのカードは自動では初期化されない。FAT 以外のフォーマットは未確認。
- ここまでの内容はソースを読んだだけで、**このリポジトリではビルドも実機確認もしていない**。ESP-IDF v5.5.6 でこの API が同じように使えるかは未確認。

## 10. 実装前に確認が必要な項目

1. IMU のチップ型番（`M5.Imu.getType()` をログに出す）と、加速度・角速度の単位と軸の向き。
2. M5Unified / M5GFX を IDF v5.5.6 のプロジェクトにどう組み込むか（git clone、サブモジュール、または Component Manager）。UserDemo は IDF v5.4.2 で、v5.5.6 でビルドできるかも未確認。
3. `record()` が非ブロッキングかどうかの実機確認。
4. キーボード用の TCA8418 ドライバを、UserDemo からコピーするか、M5Unified の I2C ラッパー（`m5::I2C_Device`）を使って自作するか。UserDemo のドライバは `M5.In_I2C` 上に作られている。`M5Cardputer` ライブラリの Adv 対応も未確認。
5. 日本語フォントの扱い。
6. 赤外線の受信ハードの有無と、GPIO44 とコンソール（UART0）の競合。
7. SD の SPI ホスト（`SDSPI_HOST_DEFAULT()` の `slot`）と、M5GFX がディスプレイに使う SPI ホストが衝突しないか。UserDemo は同じ起動順（`M5.begin()` の後に `spi_bus_initialize`）で動いているが、各ホスト番号は未確認。
8. SD の読み出し速度と、`playRaw` のチャンク再生で音切れが出ないバッファの長さ・個数。
9. 対応する音声形式。WAV の PCM は 6.1 の方式で扱えるが、MP3 などはデコーダ（minimp3 など）の組み込みが別に必要で、未調査。
10. PSRAM がないことの最終確認（Stamp-S3A のデータシート）。

## 11. 参照先

- UserDemo（Adv 用）: https://github.com/m5stack/M5Cardputer-UserDemo/tree/CardputerADV
  - `main/hal/hal.cpp`: 起動順序、IR 初期化、SPI バスと SD カードのマウント
  - `main/hal/hal_config.h`: ピン定義
  - `main/hal/keyboard/`: キーボード（`keyboard.cpp`、`keymap.h`）
  - `main/hal/utils/adafruit_tca8418/`: TCA8418 ドライバ
  - `main/hal/utils/ir_nec/`: 赤外線送信
  - `main/apps/app_imu/`、`main/apps/app_record/`: IMU とマイク・スピーカーの使用例
- M5Unified: https://github.com/m5stack/M5Unified （`src/M5Unified.cpp` にボードごとのピンとコーデック設定）
- M5GFX: https://github.com/m5stack/M5GFX
- 製品ページ: https://docs.m5stack.com/en/products/sku/K132-Adv
