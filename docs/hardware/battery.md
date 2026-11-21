# 電池

## 部品とつなぎ方

- 電池の残量は、M5Unifiedの`M5.Power`から読む。ピンやアドレスの設定は、M5Unifiedが行う。
- `M5.Power.getBatteryLevel()`が、残量を`%`で返す。

## このプロジェクトでの使い方

ソースは`main/device/battery.hpp` / `main/device/battery.cpp`。

- `battery::percent()`が、`M5.Power.getBatteryLevel()`の値を返す。0〜100の範囲に入らない値のときは、`-1`（取れない）を返す。`M5.begin()`の後に使う。
- 画面の電池アイコン（`main/ui/widgets/battery_icon.cpp`）が、LVGLのタイマーで周期的に`battery::percent()`を呼ぶ。
  - 値が`-1`のときは、アイコンの中身を隠す（取れないことを示す）。
  - ADCのぶれでちらつかないよう、前回の表示から`BATTERY_HYSTERESIS`以上離れたときだけ更新する。ただし、0と100は、すぐに反映する。
  - 残量が`LOW_PERCENT`（15）以下のときは、赤で表示する。

## 注意点

- **充電状態は取れない**。Cardputer-Advでは、`M5.Power.isCharging()`が常に`charge_unknown`を返す（実機で確認）。そのため、充電中の表示はしていない。
- 読むたびに値が少しぶれることがあるので、表示に使うときは、ヒステリシスなどで、ちらつきを抑える（`battery_icon.cpp`は`BATTERY_HYSTERESIS` = 3で抑えている）。
