#pragma once

#include "lvgl.h"

// 日本語フォント（Noto Sans JP Regular、16 px）。JIS 第1・第2水準の漢字まで持つ。
// 実体は font_jp_16.c で、tools/font のスクリプトが生成する（tools/font/README.md を参照）。

#ifdef __cplusplus
extern "C" {
#endif

LV_FONT_DECLARE(font_jp_16);

#ifdef __cplusplus
}
#endif
