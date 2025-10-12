#include <zephyr/kernel.h>
#include <lvgl.h>
#include "brightness_status.h"

static void update_brightness_status(struct zmk_widget_brightness_status *widget, uint8_t brightness)
{
    char brightness_text[8] = {};
    snprintf(brightness_text, sizeof(brightness_text), "%i %", brightness);
    lv_label_set_text(widget->label, brightness_text);

    //lv_anim_t a;
    //lv_anim_init(&a);
    //lv_anim_set_var(&a, widget);
    //lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    //lv_anim_set_time(&a, 300);
    //lv_anim_set_exec_cb(&a, (lv_anim_exec_cb_t)lv_obj_set_opa_scale);
    //lv_anim_start(&a);
}

int zmk_widget_brightness_status_init(struct zmk_widget_brightness_status *widget, lv_obj_t *parent)
{
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 240, 280);

    static lv_style_t bg_style;
    lv_style_init(&bg_style);
    lv_style_set_bg_color(&bg_style, lv_color_black());
    lv_style_set_bg_opa(&bg_style, LV_OPA_60);
    lv_obj_add_style(widget->obj, &bg_style, LV_PART_MAIN);

    widget->label = lv_label_create(widget->obj);
    lv_obj_align(widget->label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(widget->label, "50%");
    lv_obj_set_style_text_font(widget->label, &NerdFonts_Regular_40, 0);
    lv_obj_add_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);  

    return 0;
}

lv_obj_t *zmk_widget_brightness_status_obj(struct zmk_widget_brightness_status *widget)
{
    return widget->obj;
}
