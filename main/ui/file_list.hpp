#pragma once

#include "lvgl.h"

// 一覧の画面。帯の下に WAV のファイル名のリストを出し、キーで選択とスクロールをする。LVGL で描く。
namespace file_list {

/**
 * @brief 一覧を作る。status_bar::create() の後、LVGL のタスクを起動する前に 1 回だけ呼ぶ
 *
 * sdcard::listWav() でファイル名を取って並べる。0 件のときは「(none)」を出す。
 * 選んだ行で Enter を押すと、now_playing::open() で再生を始めて再生中の画面に移る。
 * 取得に失敗したときも、0 件と同じ表示にする（ログにエラーが出る）。
 */
void create();

/** @brief 一覧の曲数 */
int count();

/** @brief 一覧の index 番目のファイル名。範囲外なら nullptr */
const char *nameAt(int index);

/** @brief ファイル名の一覧での位置。なければ -1 */
int indexOf(const char *name);

/** @brief 一覧の index 番目の行（LVGL の部品）。なければ nullptr。戻ったときの選択に使う */
lv_obj_t *rowAt(int index);

/** @brief 一覧の表示と非表示を切り替える。再生中の画面を出している間は隠す */
void setVisible(bool visible);

}  // namespace file_list
