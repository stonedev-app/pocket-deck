#pragma once

// 電池の残量。M5.Power を包む。M5.begin() の後に使う。
namespace battery {

/**
 * @brief 電池の残量 [%] を返す
 *
 * @return 0〜100。取れないときは -1
 */
int percent();

}  // namespace battery
