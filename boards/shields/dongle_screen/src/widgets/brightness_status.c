#include <zephyr/kernel.h>
#include <lvgl.h>
#include "brightness_status.h"

#define BRIGHTNESS_STATUS_HIDE_DELAY_MS 300

static void brightness_status_fade_cb(void * obj, int32_t value)
{
    lv_obj_set_style_opa((lv_obj_t *)obj, value, LV_PART_MAIN);
}

static void brightness_status_timer_cb(lv_timer_t *timer)
{
    struct zmk_widget_brightness_status *widget = (struct zmk_widget_brightness_status *)timer->user_data;
    if (widget && widget->obj) {
        // Setup fade-out animation
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, widget->obj);
        lv_anim_set_exec_cb(&a, brightness_status_fade_cb);
        lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
        lv_anim_set_time(&a, BRIGHTNESS_FADE_TIME_MS);
        lv_anim_start(&a);
    }
    lv_timer_del(timer); // Clean up the timer after use
}

int zmk_widget_update_brightness_status(struct zmk_widget_brightness_status *widget, uint8_t brightness)
{
    char brightness_text[8] = {};
    snprintf(brightness_text, sizeof(brightness_text), "%i%%", brightness);
    lv_label_set_text(widget->label, brightness_text);

    // Unhide the widget
    //lv_obj_clear_flag(widget->obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(widget->obj, LV_OPA_COVER, LV_PART_MAIN);

    // Start a one-shot timer to hide the widget after 300ms
    lv_timer_t *timer = lv_timer_create(brightness_status_timer_cb, BRIGHTNESS_STATUS_HIDE_DELAY_MS, widget);
    lv_timer_set_repeat_count(timer, 1);

    return 0;
}

int zmk_widget_brightness_status_init(struct zmk_widget_brightness_status *widget, lv_obj_t *parent)
{
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 240, 280);
    lv_obj_set_style_bg_color(widget->obj, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_60, 0);

    widget->label = lv_label_create(widget->obj);
    lv_obj_align(widget->label, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(widget->label, "50%");
    lv_obj_set_style_text_font(widget->label, &lv_font_montserrat_48, 0);
    
    lv_obj_set_style_opa(widget->obj, LV_OPA_TRANSP, LV_PART_MAIN);
    return 0;
}

lv_obj_t *zmk_widget_brightness_status_obj(struct zmk_widget_brightness_status *widget)
{
    return widget->obj;
}
