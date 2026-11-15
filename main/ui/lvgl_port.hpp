#pragma once

#include <stdint.h>

// LVGL の土台。LVGL が画面の中身を RAM 上のバッファに描き、その結果を呼び出し側が渡す
// 転送の関数で画面に出す。ディスプレイの種類（M5GFX など）には依存しない。
namespace lvgl_port {

/**
 * @brief 描いた領域を画面に転送する関数。呼び出し側が用意する
 *
 * 転送は、この関数から戻るまでに終えること（戻った時点で、LVGL はバッファを再利用する）。
 *
 * @param[in] x      転送先の左上の x
 * @param[in] y      転送先の左上の y
 * @param[in] w      幅 [px]
 * @param[in] h      高さ [px]
 * @param[in] pixels RGB565（16bit、ホストのバイト順）の画素。w * h 個、行の順に並ぶ
 */
using FlushFn = void (*)(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t *pixels);

/**
 * @brief 押されたキーを 1 件取り出す関数。呼び出し側が用意する
 *
 * LVGL のタスクの中から呼ばれるので、待たずに戻ること。キーボードは「押された瞬間」だけを出せばよい。
 *
 * @param[out] key 取り出したキーの LVGL のキーコード（LV_KEY_* や文字）
 * @return 取れたら true、なければ false
 */
using PollKeyFn = bool (*)(uint32_t &key);

/**
 * @brief 画面の部品を作る関数。呼び出し側が用意する
 *
 * LVGL のタスクを起動する前に、1 回だけ呼ばれる。LVGL の部品は、この中か LVGL のタスクの中でだけ触る。
 */
using BuildUiFn = void (*)();

/**
 * @brief LVGL を初期化し、buildUi で画面を作ってから、LVGL のタスクを起動する
 *
 * 起動後に LVGL を触るのは、このタスクだけにする（LVGL はスレッドセーフではない）。
 *
 * @param[in] width   画面の幅 [px]
 * @param[in] height  画面の高さ [px]
 * @param[in] flush   描いた領域を画面に転送する関数
 * @param[in] pollKey 押されたキーを取り出す関数
 * @param[in] buildUi 画面の部品を作る関数
 * @return 成功なら true
 */
bool start(int32_t width, int32_t height, FlushFn flush, PollKeyFn pollKey, BuildUiFn buildUi);

}  // namespace lvgl_port
