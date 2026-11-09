#include "ui/status_bar.hpp"

#include "audio/volume.hpp"
#include "lvgl.h"

namespace status_bar {

// 音量アイコンの大きさと、スピーカーの中心 [px]
static constexpr int ICON_W = 18;
static constexpr int ICON_H = HEIGHT;
static constexpr int CENTER_X = 6;
static constexpr int CENTER_Y = ICON_H / 2;
static constexpr int ARC_WIDTH = 2;
static constexpr int ARC_COUNT = 3;
// 弧の外側の半径。内側から順に
static constexpr int ARC_RADIUS[ARC_COUNT] = {4, 7, 10};

// 電池（仮表示）。枠の大きさと、中身の割合 [%]
static constexpr int BATTERY_W = 20;
static constexpr int BATTERY_H = 10;
static constexpr int BATTERY_DUMMY_PERCENT = 70;

// 音量の状態を見る周期 [ms]
static constexpr uint32_t POLL_MS = 50;

static lv_obj_t *s_volumeIcon = nullptr;
// 描画に使う音量の状態。LVGL のタスクの中だけで読み書きする
static int s_level = 0;
static bool s_muted = false;

/**
 * @brief 段階（0〜16）を弧の本数に直す
 *
 * 0 は 0 本、1〜5 は 1 本、6〜10 は 2 本、11〜16 は 3 本。
 */
static int arcCount(int level)
{
    if (level <= 0) return 0;
    if (level <= 5) return 1;
    if (level <= 10) return 2;
    return 3;
}

/** @brief 音量アイコン（スピーカー + 弧、消音のときは暗いスピーカー + 斜線）を描く */
static void drawVolume(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target_obj(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t area;
    lv_obj_get_coords(obj, &area);
    const int32_t ox = area.x1;
    const int32_t oy = area.y1;

    const lv_color_t color = s_muted ? lv_color_make(0x60, 0x60, 0x60) : lv_color_white();

    // スピーカーの本体（四角）
    lv_draw_rect_dsc_t rect;
    lv_draw_rect_dsc_init(&rect);
    rect.bg_color = color;
    rect.bg_opa = LV_OPA_COVER;
    lv_area_t body = {ox + 0, oy + CENTER_Y - 2, ox + 2, oy + CENTER_Y + 1};
    lv_draw_rect(layer, &rect, &body);

    // スピーカーの広がる部分（台形を 2 つの三角形で描く）
    const lv_point_precise_t a = {(lv_value_precise_t)(ox + 3), (lv_value_precise_t)(oy + CENTER_Y - 2)};
    const lv_point_precise_t b = {(lv_value_precise_t)(ox + 7), (lv_value_precise_t)(oy + CENTER_Y - 6)};
    const lv_point_precise_t c = {(lv_value_precise_t)(ox + 7), (lv_value_precise_t)(oy + CENTER_Y + 6)};
    const lv_point_precise_t d = {(lv_value_precise_t)(ox + 3), (lv_value_precise_t)(oy + CENTER_Y + 2)};
    lv_draw_triangle_dsc_t tri;
    lv_draw_triangle_dsc_init(&tri);
    tri.color = color;
    tri.opa = LV_OPA_COVER;
    tri.p[0] = a; tri.p[1] = b; tri.p[2] = c;
    lv_draw_triangle(layer, &tri);
    tri.p[0] = a; tri.p[1] = c; tri.p[2] = d;
    lv_draw_triangle(layer, &tri);

    if (s_muted) {
        // 消音: 斜線を重ねる
        lv_draw_line_dsc_t line;
        lv_draw_line_dsc_init(&line);
        line.color = lv_color_make(0xff, 0x40, 0x40);
        line.opa = LV_OPA_COVER;
        line.width = 2;
        line.p1 = {(lv_value_precise_t)(ox + 1), (lv_value_precise_t)(oy + 1)};
        line.p2 = {(lv_value_precise_t)(ox + 16), (lv_value_precise_t)(oy + ICON_H - 2)};
        lv_draw_line(layer, &line);
        return;
    }

    // 音量: 右向きの弧（-45°〜45°）を、段階に応じた本数だけ描く
    const int n = arcCount(s_level);
    for (int i = 0; i < n; i++) {
        lv_draw_arc_dsc_t arc;
        lv_draw_arc_dsc_init(&arc);
        arc.color = color;
        arc.opa = LV_OPA_COVER;
        arc.width = ARC_WIDTH;
        arc.center = {ox + CENTER_X, oy + CENTER_Y};
        arc.radius = ARC_RADIUS[i];
        arc.start_angle = 315;
        arc.end_angle = 45;
        lv_draw_arc(layer, &arc);
    }
}

/** @brief 音量の状態を見て、変わっていたらアイコンを描き直す（LVGL のタスクで周期的に呼ばれる） */
static void pollVolume(lv_timer_t *)
{
    const int level = volume::level();
    const bool muted = volume::muted();
    if (level == s_level && muted == s_muted) return;
    s_level = level;
    s_muted = muted;
    lv_obj_invalidate(s_volumeIcon);
}

/** @brief 飾りのない箱を作る（既定のテーマの枠・余白・スクロールを外す） */
static lv_obj_t *createBox(lv_obj_t *parent, int w, int h)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_size(o, w, h);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    return o;
}

/** @brief 電池の絵（横長の枠、中身、右の突起）を作る。残量は仮の固定値 */
static void createBattery(lv_obj_t *parent)
{
    // 枠 + 突起をまとめる入れ物
    lv_obj_t *box = createBox(parent, BATTERY_W + 2, BATTERY_H);

    lv_obj_t *frame = createBox(box, BATTERY_W, BATTERY_H);
    lv_obj_set_pos(frame, 0, 0);
    lv_obj_set_style_border_width(frame, 1, 0);
    lv_obj_set_style_border_color(frame, lv_color_white(), 0);

    // 中身。枠の内側（1px の余白）に収める
    const int inner = BATTERY_W - 4;
    lv_obj_t *fill = createBox(frame, inner * BATTERY_DUMMY_PERCENT / 100, BATTERY_H - 4);
    lv_obj_set_pos(fill, 1, 1);
    lv_obj_set_style_bg_color(fill, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);

    lv_obj_t *nub = createBox(box, 2, 4);
    lv_obj_set_pos(nub, BATTERY_W, (BATTERY_H - 4) / 2);
    lv_obj_set_style_bg_color(nub, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(nub, LV_OPA_COVER, 0);
}

void create()
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    // 帯。画面の幅いっぱいで、中身は右寄せに並べる
    lv_obj_t *bar = createBox(screen, LV_PCT(100), HEIGHT);
    lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(bar, 6, 0);
    lv_obj_set_style_pad_right(bar, 4, 0);

    s_level = volume::level();
    s_muted = volume::muted();
    s_volumeIcon = createBox(bar, ICON_W, ICON_H);
    lv_obj_add_event_cb(s_volumeIcon, drawVolume, LV_EVENT_DRAW_MAIN, nullptr);

    createBattery(bar);

    lv_timer_create(pollVolume, POLL_MS, nullptr);
}

}  // namespace status_bar
