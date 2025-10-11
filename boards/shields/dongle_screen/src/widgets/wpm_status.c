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
#define WPM_BAR_MAX 100
#define WPM_BAR_MIN 0

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
struct wpm_status_state
{
    int wpm;
};

struct wpm_object {
    lv_obj_t *symbol;
    lv_obj_t *label;
    lv_obj_t *bar;
} wpm_object;

static lv_color_t wpm_image_buffer[WPM_BAR_LENGTH*WPM_BAR_HEIGHT];

// Get the state from ZMK as user is typing.
static struct wpm_status_state get_state(const zmk_event_t *_eh)
{
    const struct zmk_wpm_state_changed *ev = as_zmk_wpm_state_changed(_eh);

    return (struct wpm_status_state){
        .wpm = ev ? ev->state : 0};
}

// Function to generate the progress bar at different colors for different speeds.
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
    } else if (wpm <= 70) {
        lv_canvas_fill_bg(canvas, lv_palette_main(LV_PALETTE_LIME), LV_OPA_COVER);
    } else {
        lv_canvas_fill_bg(canvas, lv_color_white(), LV_OPA_COVER);
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
    
    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.font = LV_FONT_DEFAULT;

    char buf[8];
    lv_snprintf(buf, sizeof(buf), "%d", wpm);

    lv_text_attributes_t attributes = {0};
    attributes.letter_space = label_dsc.letter_space;
    attributes.line_space = label_dsc.line_space;
    attributes.max_width = LV_COORD_MAX;
    attributes.text_flags = label_dsc.flag;

    lv_point_t txt_size;
    lv_text_get_size(&txt_size, buf, label_dsc.font, &attributes);

    lv_area_t bar_area;
    lv_obj_get_coords(obj, &bar_area);
    lv_area_t indic_area;
    lv_bar_get_indicator_coords(obj, &indic_area);

    lv_area_t txt_area;
    txt_area.x1 = 0;
    txt_area.x2 = txt_size.x - 1;
    txt_area.y1 = 0;
    txt_area.y2 = txt_size.y - 1;

    if (lv_area_get_width(&indic_area) > txt_size.x + 20) {
        lv_area_align(&indic_area, &txt_area, LV_ALIGN_RIGHT_MID, -10, 0);
        label_dsc.color = lv_color_white();
    } else {
        lv_area_align(&indic_area, &txt_area, LV_ALIGN_OUT_RIGHT_MID, 10, 0);
        label_dsc.color = lv_color_black();
    }
    label_dsc.text = buf;
    label_dsc.text_local = true;

    lv_layer_t * layer = lv_event_get_layer(e);
    lv_draw_label(layer, &label_dsc, &txt_area);
    
}

// Function to update the progress bar and the label to the current wpm state.
static void set_wpm(struct zmk_widget_wpm_status *widget, struct wpm_status_state state)
{
    lv_obj_t *symbol = wpm_object.symbol;
    lv_obj_t *label = wpm_object.label;
    lv_obj_t *bar = wpm_object.bar;
    
    draw_wpm(symbol, state.wpm);

    char wpm_text[12];
    snprintf(wpm_text, sizeof(wpm_text), "%i", state.wpm);
    lv_label_set_text(label, wpm_text);

    if (bar) {
        int val = state.wpm;
        if (val > WPM_BAR_MAX) val = WPM_BAR_MAX;
        if (val < WPM_BAR_MIN) val = WPM_BAR_MIN;
        lv_bar_set_value(bar, val, LV_ANIM_OFF);
    }

    lv_obj_clear_flag(symbol, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(symbol);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(label);

    if (bar) {
        lv_obj_clear_flag(bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(bar);
    }
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

// Take all the items that were setup and create the final product, the main object that holds the bar cnavas and label.
int zmk_widget_wpm_status_init(struct zmk_widget_wpm_status *widget, lv_obj_t *parent)
{
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 240, 40);

    lv_obj_t *image_canvas = lv_canvas_create(widget->obj);
    lv_obj_t *wpm_label = lv_label_create(widget->obj);

    lv_canvas_set_buffer(image_canvas, wpm_image_buffer, WPM_BAR_LENGTH, WPM_BAR_HEIGHT, LV_IMG_CF_TRUE_COLOR);

    lv_obj_align(image_canvas, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_align(wpm_label, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_add_flag(image_canvas, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(wpm_label, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *wpm_bar = lv_bar_create(widget->obj);
    lv_bar_set_range(wpm_bar, WPM_BAR_MIN, WPM_BAR_MAX);
    lv_obj_set_size(wpm_bar, 200, 20);
    lv_obj_align(wpm_bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_add_flag(wpm_bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(wpm_bar, wpm_status_update_cb, LV_EVENT_DRAW_MAIN_END, NULL);
        
    wpm_object = (struct wpm_object){
            .symbol = image_canvas,
            .label = wpm_label,
            .bar = wpm_bar,
    };

    sys_slist_append(&widgets, &widget->node);

    widget_wpm_status_init();
    return 0;
}

lv_obj_t *zmk_widget_wpm_status_obj(struct zmk_widget_wpm_status *widget)
{
    return widget->obj;
}
