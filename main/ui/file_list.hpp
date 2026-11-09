#pragma once

// 一覧の画面。帯の下に WAV のファイル名のリストを出し、キーで選択とスクロールをする。LVGL で描く。
namespace file_list {

/**
 * @brief 一覧を作る。status_bar::create() の後、LVGL のタスクを起動する前に 1 回だけ呼ぶ
 *
 * sdcard::listWav() でファイル名を取って並べる。0 件のときは「(none)」を出す。
 * 選んだ行で Enter を押すと、今は名前をログに出すだけ（再生中の画面は段階 3）。
 * 取得に失敗したときも、0 件と同じ表示にする（ログにエラーが出る）。
 */
void create();

}  // namespace file_list
