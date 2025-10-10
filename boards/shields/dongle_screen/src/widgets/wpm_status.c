/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/wpm_state_changed.h>

#include "wpm_status.h"
#include <fonts.h>

#define WPM_BAR_LENGTH 102
#define WPM_BAR_HEIGHT 20

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
struct wpm_status_state
{
    int wpm;
};

struct wpm_object {
    lv_obj_t *symbol;
    lv_obj_t *label;
} wpm_object;

static lv_color_t wpm_image_buffer[WPM_BAR_LENGTH*WPM_BAR_HEIGHT];

static struct wpm_status_state get_state(const zmk_event_t *_eh)
{
    const struct zmk_wpm_state_changed *ev = as_zmk_wpm_state_changed(_eh);

    return (struct wpm_status_state){
        .wpm = ev ? ev->state : 0};
}

static void draw_wpm(lv_obj_t *canvas, uint8_t wpm) {

    if (wpm > 100) {
        wpm = 100;
    } 
    
    if (wpm < 1)
    {
        lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_COVER);
    } else if (wpm <= 10) {
        lv_canvas_fill_bg(canvas, lv_palette_main(LV_PALETTE_PURPLE), LV_OPA_COVER);
    } else if (wpm <= 30) {
        lv_canvas_fill_bg(canvas, lv_palette_main(LV_PALETTE_CYAN), LV_OPA_COVER);
    } else if (wpm <= 50) {
        lv_canvas_fill_bg(canvas, lv_palette_main(LV_PALETTE_GREEN), LV_OPA_COVER);
    } else if (wpm <= 90) {
        lv_canvas_fill_bg(canvas, lv_palette_main(LV_PALETTE_LIME), LV_OPA_COVER);
    } else {
        lv_canvas_fill_bg(canvas, lv_color_black(), LV_OPA_COVER);
    }

    lv_draw_rect_dsc_t rect_fill_dsc;
    lv_draw_rect_dsc_init(&rect_fill_dsc);
    rect_fill_dsc.bg_color = lv_color_black();

    lv_canvas_set_px(canvas, 0, 0, lv_color_black());
    lv_canvas_set_px(canvas, 0, WPM_BAR_HEIGHT - 1, lv_color_black());
    lv_canvas_set_px(canvas, WPM_BAR_LENGTH - 1, 0, lv_color_black());
    lv_canvas_set_px(canvas, WPM_BAR_LENGTH - 1, WPM_BAR_HEIGHT - 1, lv_color_black());

    if (wpm <= 99 && wpm > 0)
    {
        lv_canvas_draw_rect(canvas, wpm, 1, WPM_BAR_LENGTH - 2 - wpm, WPM_BAR_HEIGHT-2, &rect_fill_dsc);
        for (int i = 1; i < WPM_BAR_HEIGHT; i++) {
            lv_canvas_set_px(canvas, WPM_BAR_LENGTH-2, i, lv_color_black());
        }   
    }  
}

static void set_wpm(struct zmk_widget_wpm_status *widget, struct wpm_status_state state)
{
    lv_obj_t *symbol = wpm_object.symbol;
    lv_obj_t *label = wpm_object.label;
    
    draw_wpm(symbol, state.wpm);

    char wpm_text[12];
    snprintf(wpm_text, sizeof(wpm_text), "%i", state.wpm);
    lv_label_set_text(label, wpm_text);

    lv_obj_clear_flag(symbol, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(symbol);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(label);
}

static void wpm_status_update_cb(struct wpm_status_state state)
{
    struct zmk_widget_wpm_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node)
    {
        set_wpm(widget, state);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_wpm_status, struct wpm_status_state,
                            wpm_status_update_cb, get_state)
ZMK_SUBSCRIPTION(widget_wpm_status, zmk_wpm_state_changed);

// output_status.c
int zmk_widget_wpm_status_init(struct zmk_widget_wpm_status *widget, lv_obj_t *parent)
{
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 240, 40);

    lv_obj_t *image_canvas = lv_canvas_create(widget->obj);
    lv_obj_t *wpm_label = lv_label_create(widget->obj);

    lv_canvas_set_buffer(image_canvas, wpm_image_buffer, WPM_BAR_LENGTH, WPM_BAR_HEIGHT, LV_IMG_CF_TRUE_COLOR);

    lv_obj_align(image_canvas, LV_ALIGN_LEFT_TOP, 0, 0);
    lv_obj_align(wpm_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_add_flag(image_canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(wpm_label, LV_OBJ_FLAG_HIDDEN);
        
    wpm_object = (struct wpm_object){
            .symbol = image_canvas,
            .label = wpm_label,
    };

    sys_slist_append(&widgets, &widget->node);

    widget_wpm_status_init();
    return 0;
}

lv_obj_t *zmk_widget_wpm_status_obj(struct zmk_widget_wpm_status *widget)
{
    return widget->obj;
}
