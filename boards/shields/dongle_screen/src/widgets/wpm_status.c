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

static lv_color_t dark_grey_color = LV_COLOR_MAKE(0x40, 0x40, 0x40); // dark grey

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
struct wpm_status_state
{
    int wpm;
};

struct wpm_object {
    lv_obj_t *bar;
} wpm_object;

static lv_style_t style_bg;
static lv_style_t style_indic;

static struct wpm_status_state get_state(const zmk_event_t *_eh)
{
    const struct zmk_wpm_state_changed *ev = as_zmk_wpm_state_changed(_eh);
    return (struct wpm_status_state){
        .wpm = ev ? ev->state : 0};
}

static void set_wpm(struct zmk_widget_wpm_status *widget, struct wpm_status_state state)
{
    // Create the three objects associated with the wpm_object so we can manipulate them
    lv_obj_t *bar = wpm_object.bar;

    lv_style_init(&style_bg);
    lv_style_set_border_color(&style_bg, dark_grey_color);
    lv_style_set_border_width(&style_bg, 1);
    lv_style_set_radius(&style_bg, 10);

    lv_style_init(&style_indic);
    lv_style_set_bg_opa(&style_indic, LV_OPA_COVER);
    lv_style_set_bg_color(&style_indic, lv_palette_main(LV_PALETTE_BLUE));
    lv_style_set_bg_grad_color(&style_indic, lv_palette_main(LV_PALETTE_YELLOW));
    lv_style_set_bg_grad_dir(&style_indic, LV_GRAD_DIR_HOR);
    lv_style_set_radius(&style_indic, 8);

    lv_obj_remove_style_all(bar);  /*To have a clean start*/
    lv_obj_add_style(bar, &style_bg, 0);
    lv_obj_add_style(bar, &style_indic, LV_PART_INDICATOR);

    lv_obj_set_size(bar, 120, 20);
    lv_bar_set_range(bar, 0, 160);
    lv_bar_set_value(bar, state.wpm, LV_ANIM_ON);

    lv_obj_clear_flag(bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(bar);
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
    // Create the widget and set to parent.
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 240, 100);

    // Create theobjects and assign each to the widget.
    lv_obj_t * bar = lv_bar_create(widget->obj);
    lv_obj_t * wpm_label = lv_label_create(widget->obj);

    lv_label_set_text(wpm_label, "words per min");
    lv_obj_set_style_text_font(wpm_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(wpm_label, dark_grey_color, 0);
    
    // Align all the objects within the newly created widget.
    lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_align(wpm_label, LV_ALIGN_TOP_LEFT, 0, 25);

    // Temporarily hide them until we we ready to work on them.
    lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);    

    // Create the wmp_object assigning the objects created in this function to the wpm_object.
    wpm_object = (struct wpm_object){
            .bar = bar,
    };

    sys_slist_append(&widgets, &widget->node);

    widget_wpm_status_init();
    
    return 0;
}

lv_obj_t *zmk_widget_wpm_status_obj(struct zmk_widget_wpm_status *widget)
{
    return widget->obj;
}
