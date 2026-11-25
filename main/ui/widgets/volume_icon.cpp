#include "ui/widgets/volume_icon.hpp"

#include "audio/volume.hpp"
#include "ui/lvgl_util.hpp"

namespace volume_icon {

// アイコンの幅と、スピーカーの中心 [px]。高さは create() の引数
static constexpr int ICON_W = 18;
static constexpr int CENTER_X = 6;
static constexpr int ARC_WIDTH = 2;
static constexpr int ARC_COUNT = 3;
// 弧の外側の半径。内側から順に
static constexpr int ARC_RADIUS[ARC_COUNT] = {4, 7, 10};

// 音量の状態を見る周期 [ms]
static constexpr uint32_t POLL_MS = 50;

static lv_obj_t *s_icon = nullptr;
static lv_obj_t *s_label = nullptr;
static int s_height = 0;
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
static void draw(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target_obj(e);
    lv_layer_t *layer = lv_event_get_layer(e);
    lv_area_t area;
    lv_obj_get_coords(obj, &area);
    const int32_t ox = area.x1;
    const int32_t oy = area.y1;
    const int centerY = s_height / 2;

    const lv_color_t color = s_muted ? lv_color_make(0x60, 0x60, 0x60) : lv_color_white();

    // スピーカーの本体（四角）
    lv_draw_rect_dsc_t rect;
    lv_draw_rect_dsc_init(&rect);
    rect.bg_color = color;
    rect.bg_opa = LV_OPA_COVER;
    lv_area_t body = {ox + 0, oy + centerY - 2, ox + 2, oy + centerY + 1};
    lv_draw_rect(layer, &rect, &body);

    // スピーカーの広がる部分（台形を 2 つの三角形で描く）
    const lv_point_precise_t a = {(lv_value_precise_t)(ox + 3), (lv_value_precise_t)(oy + centerY - 2)};
    const lv_point_precise_t b = {(lv_value_precise_t)(ox + 7), (lv_value_precise_t)(oy + centerY - 6)};
    const lv_point_precise_t c = {(lv_value_precise_t)(ox + 7), (lv_value_precise_t)(oy + centerY + 6)};
    const lv_point_precise_t d = {(lv_value_precise_t)(ox + 3), (lv_value_precise_t)(oy + centerY + 2)};
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
        line.p2 = {(lv_value_precise_t)(ox + 16), (lv_value_precise_t)(oy + s_height - 2)};
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
        arc.center = {ox + CENTER_X, oy + centerY};
        arc.radius = ARC_RADIUS[i];
        arc.start_angle = 315;
        arc.end_angle = 45;
        lv_draw_arc(layer, &arc);
    }
}

/** @brief 音量の数字（n/16 の形）を更新する。消音のときは数字を残して、スピーカーと同じ暗い灰色にする */
static void updateLabel()
{
    lv_label_set_text_fmt(s_label, "%d/%d", s_level, volume::MAX_LEVEL);
    lv_obj_set_style_text_color(s_label, s_muted ? lv_color_make(0x60, 0x60, 0x60) : lv_color_white(), 0);
}

/** @brief 音量の状態を見て、変わっていたらアイコンを描き直す（LVGL のタスクで周期的に呼ばれる） */
static void poll(lv_timer_t *)
{
    const int level = volume::level();
    const bool muted = volume::muted();
    if (level == s_level && muted == s_muted) return;
    s_level = level;
    s_muted = muted;
    lv_obj_invalidate(s_icon);
    updateLabel();
}

void create(lv_obj_t *parent, int height)
{
    s_height = height;
    s_level = volume::level();
    s_muted = volume::muted();
    // 音量の数字（スピーカーの左）
    s_label = lv_label_create(parent);
    lv_obj_set_style_text_font(s_label, &lv_font_montserrat_12, 0);
    // 桁数が変わっても幅が変わらないよう、最も長い "16/16" が入る幅にして右寄せにする
    lvgl_util::setFixedWidthRight(s_label, &lv_font_montserrat_12, "16/16");
    updateLabel();

    s_icon = lvgl_util::createBox(parent, ICON_W, height);
    lv_obj_add_event_cb(s_icon, draw, LV_EVENT_DRAW_MAIN, nullptr);

    lv_timer_create(poll, POLL_MS, nullptr);
}

}  // namespace volume_icon
