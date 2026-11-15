#pragma once

// 一覧の画面。帯の下に WAV のファイル名のリストを出し、キーで選択とスクロールをする。LVGL で描く。
// データは app/playlist が持つ。この画面は表示と選択だけを行う。
namespace file_list {

/** @brief 行が選ばれたとき（Enter）に呼ばれる関数。行の位置（0 始まり）を渡す */
using SelectFn = void (*)(int index);

/**
 * @brief 一覧を作る。status_bar::create() の後、LVGL のタスクを起動する前に 1 回だけ呼ぶ
 *
 * playlist の曲を並べる。0 件のときは「(none)」を出す。
 * 選んだ行で Enter を押すと、onSelect を呼ぶ。
 *
 * @param[in] onSelect 行が選ばれたときの関数
 */
void create(SelectFn onSelect);

/** @brief 一覧を出し、index 番目の行を選ぶ。行がなければ（範囲外、0 件）選ぶのは省く */
void show(int index);

/** @brief 一覧を隠す。再生中の画面を出している間は隠す */
void hide();

}  // namespace file_list
