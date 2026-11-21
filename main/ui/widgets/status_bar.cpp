#include "ui/widgets/status_bar.hpp"

#include "lvgl.h" // IWYU pragma: keep
#include "ui/lvgl_util.hpp"
#include "ui/widgets/battery_icon.hpp"
#include "ui/widgets/volume_icon.hpp"

namespace status_bar {

void create()
{
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    // 帯。画面の幅いっぱいで、中身は右寄せに並べる
    lv_obj_t *bar = lvgl_util::createBox(screen, LV_PCT(100), HEIGHT);
    lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(bar, 6, 0);
    lv_obj_set_style_pad_right(bar, 4, 0);

    volume_icon::create(bar, HEIGHT);
    battery_icon::create(bar);
}

}  // namespace status_bar
