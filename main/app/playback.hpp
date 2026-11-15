#pragma once

#include <stdint.h>

// 再生の制御。曲の位置、再生の状態、リピート、終了通知の処理を持つ。LVGL には依存しない。
// LVGL のタスクからだけ呼ぶ（状態を排他制御していない）。
namespace playback {

enum class State { Playing, Paused, Stopped };

/**
 * @brief 一覧の index 番目の曲を再生する。再生中・一時停止中なら、その曲を止めて切り替える
 *
 * 解析か開始に失敗したときは、ログに出して false を返す（今の再生は続ける。開始の失敗で止まった場合は Stopped）。
 */
bool play(int index);

/** @brief 再生中は一時停止、一時停止中は続きから、停止中は最初から */
void togglePause();

/** @brief 次の曲を再生する。一覧の最後では何もしない */
void next();

/** @brief 前の曲を再生する。一覧の先頭では何もしない */
void prev();

/** @brief 1 曲リピートの入と切を切り替える */
void toggleRepeat();

/** @brief 再生を止める。止めたときの終了通知は捨てる */
void stop();

/** @brief 終了通知を見て状態を更新する。リピートがオンで最後まで再生したときは、同じ曲を最初から始める */
void update();

/** @brief 再生の状態 */
State state();

/** @brief 1 曲リピートがオンか */
bool repeat();

/** @brief 今の曲の、一覧での位置。まだ再生していなければ -1 */
int index();

/** @brief 再生位置 [ms]。最後まで再生して終わったときは長さと同じ。長さを超えない */
uint32_t positionMs();

/** @brief 今の曲の長さ [ms] */
uint32_t durationMs();

}  // namespace playback
