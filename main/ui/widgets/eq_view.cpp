#include "ui/widgets/eq_view.hpp"

#include "audio/spectrum.hpp"
#include "ui/lvgl_util.hpp"

namespace eq_view {

static constexpr int EQ_GAP = 4;  // 棒の間の隙間 [px]
// イコライザーを更新する周期 [ms]
static constexpr uint32_t EQ_POLL_MS = 40;
// 下がるときの 1 回あたりの減り幅（レベル 0〜100）。上がるときは一気に追いつく
static constexpr int EQ_FALL = 8;

// 以下は LVGL のタスクの中（と、起動前の create()）だけで読み書きする
static lv_obj_t *s_eq = nullptr;         // 棒はすべてこの 1 個の中に描く
static int s_height = 0;                 // 部品の高さ [px]
static bool s_active = false;
static int s_eqShown[spectrum::BANDS];  // 表示中のレベル（0〜100）

/** @brief レベル（0〜100）を棒の高さ [px] に直す */
static int eqBarHeight(int level)
{
    return level * s_height / 100;
}

/**
 * @brief イコライザーの棒をまとめて描く
 *
 * 棒を 1 本ずつ別の部品にすると、描き直す領域が棒の数だけに分かれ、領域ごとの手間で
 * 描画が間に合わなくなる（実機で計測、#57）。1 個の部品の中に、全部の棒を四角で描く。
 */
static void drawEq(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target_obj(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t area;
    lv_obj_get_coords(obj, &area);
    const int32_t pitch = lv_area_get_width(&area) / spectrum::BANDS;

    lv_draw_rect_dsc_t rect;
    lv_draw_rect_dsc_init(&rect);
    rect.bg_color = lv_color_make(0x40, 0xc0, 0x60);
    rect.bg_opa = LV_OPA_COVER;
    for (int b = 0; b < spectrum::BANDS; b++) {
        const int h = eqBarHeight(s_eqShown[b]);
        if (h <= 0) continue;
        const int32_t x = area.x1 + b * pitch + EQ_GAP / 2;
        lv_area_t bar = {x, area.y2 - h + 1, x + pitch - EQ_GAP - 1, area.y2};
        lv_draw_rect(layer, &rect, &bar);
    }
}

/**
 * @brief spectrum のレベルを読み、棒を更新する。上がるときは速く、下がるときは少しずつ
 *
 * 描き直すのは、高さが変わった棒を囲む範囲だけ（横は変わった棒の端から端、縦は動いた分）。
 */
static void eqPoll(lv_timer_t *)
{
    if (!s_active) return;
    uint8_t target[spectrum::BANDS];
    spectrum::levels(target);
    int first = -1, last = -1;       // 高さが変わった棒の範囲
    int hMin = s_height, hMax = 0;   // 変わった棒の、変わる前後の高さの範囲
    for (int b = 0; b < spectrum::BANDS; b++) {
        int cur = s_eqShown[b];
        if (target[b] >= cur) {
            cur = cur + (target[b] - cur + 1) * 3 / 4;  // 差の 3/4 まで一気に上げる
            if (cur > target[b]) cur = target[b];
        } else {
            cur -= EQ_FALL;
            if (cur < target[b]) cur = target[b];
        }
        const int before = eqBarHeight(s_eqShown[b]);
        const int after = eqBarHeight(cur);
        s_eqShown[b] = cur;
        if (before == after) continue;
        if (first < 0) first = b;
        last = b;
        if (before < hMin) hMin = before;
        if (after < hMin) hMin = after;
        if (before > hMax) hMax = before;
        if (after > hMax) hMax = after;
    }
    if (first < 0) return;

    lv_area_t area;
    lv_obj_get_coords(s_eq, &area);
    const int32_t pitch = lv_area_get_width(&area) / spectrum::BANDS;
    lv_area_t dirty = {area.x1 + first * pitch, area.y2 - hMax + 1, area.x1 + (last + 1) * pitch - 1,
                       area.y2 - hMin};
    lv_obj_invalidate_area(s_eq, &dirty);
}

void create(lv_obj_t *parent, int x, int y, int w, int h)
{
    s_height = h;
    // 棒は drawEq() が描く
    s_eq = lvgl_util::createBox(parent, w, h);
    lv_obj_set_pos(s_eq, x, y);
    lv_obj_add_event_cb(s_eq, drawEq, LV_EVENT_DRAW_MAIN, nullptr);

    lv_timer_create(eqPoll, EQ_POLL_MS, nullptr);
}

void reset()
{
    for (int b = 0; b < spectrum::BANDS; b++) s_eqShown[b] = 0;
    lv_obj_invalidate(s_eq);
}

void setActive(bool active)
{
    s_active = active;
}

}  // namespace eq_view
