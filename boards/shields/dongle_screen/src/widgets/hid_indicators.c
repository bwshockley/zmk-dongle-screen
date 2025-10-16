/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/services/bas.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/hid_indicators_changed.h>

#include <fonts.h>

#include "hid_indicators.h"

#define LED_NLCK 0x01
#define LED_CLCK 0x02
#define LED_SLCK 0x04

#define LOCK "\xEF\x80\xA3"
#define UNLOCK "\xEF\x8F\x81"

// Add LVGL color includes
#include <lvgl.h>

struct hid_indicators_state {
    uint8_t hid_indicators;
};

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

// Define colors
static lv_color_t inactive_color = LV_COLOR_MAKE(0x40, 0x40, 0x40); // dark grey
static lv_color_t active_color = LV_COLOR_MAKE(0x00, 0x00, 0xFF); // blue

static void set_hid_indicators(struct zmk_widget_hid_indicators *widget, struct hid_indicators_state state) {
    bool caps = state.hid_indicators & LED_CLCK;
    bool num = state.hid_indicators & LED_NLCK;
    bool scroll = state.hid_indicators & LED_SLCK;

    lv_color_t caps_color = caps ? active_color : inactive_color;
    lv_color_t num_color = num ? active_color : inactive_color;
    lv_color_t scroll_color = scroll ? active_color : inactive_color;

    // Set label colors and text
    lv_obj_set_style_text_font(widget->caps_icon, &icons_lvgl, 0);
    lv_obj_set_style_text_color(widget->caps_icon, caps_color, 0);
    lv_label_set_text(widget->caps_icon, LOCK);
    
    lv_obj_set_style_text_color(widget->caps_label, caps_color, 0);
    lv_label_set_text(widget->caps_label, "CAPS");

    lv_label_set_text(widget->num_label, "NUM");
    lv_obj_set_style_text_color(widget->num_label, num_color, 0);

    lv_label_set_text(widget->scroll_label, "SCROLL");
    lv_obj_set_style_text_color(widget->scroll_label, scroll_color, 0);
}

void hid_indicators_update_cb(struct hid_indicators_state state) {
    struct zmk_widget_hid_indicators *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_hid_indicators(widget, state);
    }
}

static struct hid_indicators_state hid_indicators_get_state(const zmk_event_t *eh) {
    struct zmk_hid_indicators_changed *ev = as_zmk_hid_indicators_changed(eh);
    return (struct hid_indicators_state){
        .hid_indicators = ev->indicators,
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_hid_indicators, struct hid_indicators_state,
                            hid_indicators_update_cb, hid_indicators_get_state)

ZMK_SUBSCRIPTION(widget_hid_indicators, zmk_hid_indicators_changed);

int zmk_widget_hid_indicators_init(struct zmk_widget_hid_indicators *widget, lv_obj_t *parent) {
    // Create horizontal container
    widget->cont = lv_obj_create(parent);
    lv_obj_set_size(widget->cont, 240, 40);
    lv_obj_set_style_border_width(widget->cont, 0, 0);
    lv_obj_set_style_pad_all(widget->cont, 0, 0);
    lv_obj_set_style_bg_opa(widget->cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_font(widget->cont, &lv_font_montserrat_12, 0);

    // Add labels
    widget->caps_label = lv_label_create(widget->cont);
    widget->caps_icon = lv_label_create(widget->cont);
    lv_obj_align(widget->caps_icon, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_align(widget->caps_label, LV_ALIGN_LEFT_MID, 15, 0);
    
    widget->num_label = lv_label_create(widget->cont);
    lv_obj_align(widget->num_label, LV_ALIGN_LEFT_MID, 50, 0);
    widget->scroll_label = lv_label_create(widget->cont);
    lv_obj_align(widget->scroll_label, LV_ALIGN_LEFT_MID, 95, 0);

    // Optional: add some spacing between labels
    // lv_obj_set_style_pad_gap(widget->cont, 8, 0);

    sys_slist_append(&widgets, &widget->node);

    widget_hid_indicators_init();

    // Initialize with all inactive
    struct hid_indicators_state initial = {0};
    set_hid_indicators(widget, initial);

    return 0;
}

lv_obj_t *zmk_widget_hid_indicators_obj(struct zmk_widget_hid_indicators *widget) {
    return widget->cont;
}
