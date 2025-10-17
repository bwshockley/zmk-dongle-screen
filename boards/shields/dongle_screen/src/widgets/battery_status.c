/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/services/bas.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/split/central.h>
#include <zmk/display.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/usb.h>

#include "battery_status.h"
#include "../brightness.h"

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY)
    #define SOURCE_OFFSET 1
#else
    #define SOURCE_OFFSET 0
#endif

#define BATT_BAR_LENGTH 100
#define BATT_BAR_HEIGHT 14

#define BATT_BAR_MAX 100
#define BATT_BAR_MIN 0

// Stylize the bar here.
static lv_color_t dark_grey_color = LV_COLOR_MAKE(0x40, 0x40, 0x40); // dark grey
static lv_color_t pure_blue_color = LV_COLOR_MAKE(0x00, 0x00, 0xFF); // dark grey

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct battery_state {
    uint8_t source;
    uint8_t level;
    bool usb_present;
};

struct battery_object {
    lv_obj_t * bar;
} battery_objects[ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT + SOURCE_OFFSET];

static lv_style_t style_bg;
static lv_style_t style_indic;

// Peripheral reconnection tracking
// ZMK sends battery events with level < 1 when peripherals disconnect
static int8_t last_battery_levels[ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT + SOURCE_OFFSET];

static void init_peripheral_tracking(void) {
    for (int i = 0; i < (ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT + SOURCE_OFFSET); i++) {
        last_battery_levels[i] = -1; // -1 indicates never seen before
    }
}

static bool is_peripheral_reconnecting(uint8_t source, uint8_t new_level) {
    if (source >= (ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT + SOURCE_OFFSET)) {
        return false;
    }
    
    int8_t previous_level = last_battery_levels[source];
    
    // Reconnection detected if:
    // 1. Previous level was < 1 (disconnected/unknown) AND
    // 2. New level is >= 1 (valid battery level)
    bool reconnecting = (previous_level < 1) && (new_level >= 1);
    
    if (reconnecting) {
        LOG_INF("Peripheral %d reconnection: %d%% -> %d%% (was %s)", 
                source, previous_level, new_level, 
                previous_level == -1 ? "never seen" : "disconnected");
    }
    
    return reconnecting;
}

static void event_cb(lv_event_t * e)
{
    lv_obj_draw_part_dsc_t * dsc = lv_event_get_draw_part_dsc(e);
    if(dsc->part != LV_PART_INDICATOR) return;

    lv_obj_t * obj = lv_event_get_target(e);

    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.font = LV_FONT_DEFAULT;

    char buf[8];
    lv_snprintf(buf, sizeof(buf), "%d", (int)lv_bar_get_value(obj));

    lv_point_t txt_size;
    lv_txt_get_size(&txt_size, buf, label_dsc.font, label_dsc.letter_space, label_dsc.line_space, LV_COORD_MAX, label_dsc.flag);

    lv_area_t txt_area;
    /*If the indicator is long enough put the text inside on the right*/
    if(lv_area_get_width(dsc->draw_area) > txt_size.x + 20) {
        txt_area.x2 = dsc->draw_area->x2 - 5;
        txt_area.x1 = txt_area.x2 - txt_size.x + 1;
        label_dsc.color = lv_color_white();
    }
    /*If the indicator is still short put the text out of it on the right*/
    else {
        txt_area.x1 = dsc->draw_area->x2 + 5;
        txt_area.x2 = txt_area.x1 + txt_size.x - 1;
        label_dsc.color = lv_color_black();
    }

    txt_area.y1 = dsc->draw_area->y1 + (lv_area_get_height(dsc->draw_area) - txt_size.y) / 2;
    txt_area.y2 = txt_area.y1 + txt_size.y - 1;

    lv_draw_label(dsc->draw_ctx, &label_dsc, &txt_area, buf, NULL);
}

static void set_battery_symbol(lv_obj_t *widget, struct battery_state state) {
    if (state.source >= ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT + SOURCE_OFFSET) {
        return;
    }
    
    // Check for reconnection using the existing battery level mechanism
    bool reconnecting = is_peripheral_reconnecting(state.source, state.level);
    
    // Update our tracking
    last_battery_levels[state.source] = state.level;

    // Wake screen on reconnection
    if (reconnecting) {
        #if CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0    
        LOG_INF("Peripheral %d reconnected (battery: %d%%), requesting screen wake", 
                state.source, state.level);
        brightness_wake_screen_on_reconnect();
        #else 
        LOG_INF("Peripheral %d reconnected (battery: %d%%)", 
                state.source, state.level);
        #endif
    }


    LOG_DBG("source: %d, level: %d, usb: %d", state.source, state.level, state.usb_present);

    // Retreive the bar objet from the passed list of objects.
    lv_obj_t * bar = battery_objects[state.source].bar;
    lv_obj_t * value_label = battery_objects[state.source].bar->label;

    lv_label_set_text_fmt(value_label, "%d", lv_bar_get_value(bar));

    // Get the bar's content area coordinates
    lv_coord_t bar_width = lv_obj_get_width(bar);
    lv_coord_t bar_value = lv_bar_get_value(bar);
    lv_coord_t bar_range = lv_bar_get_max_value(bar) - lv_bar_get_min_value(bar);

    // Calculate x-position based on bar value
    lv_coord_t label_x = (lv_coord_t)((float)(bar_value - lv_bar_get_min_value(bar)) / bar_range * bar_width) - lv_obj_get_width(value_label) - 5;

    // Set the label's position
    lv_obj_set_pos(value_label, label_x, lv_obj_get_height(bar) / 2 - lv_obj_get_height(value_label) / 2);

    lv_bar_set_value(bar, state.level, LV_ANIM_ON);

    // Style the bar indicator and border to the various states.
    if (state.level <= 10) {
        lv_obj_set_style_border_color(bar, lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_set_style_bg_color(bar, lv_palette_main(LV_PALETTE_RED), LV_PART_INDICATOR); 
    } else if (state.level <= 20) {
        lv_obj_set_style_border_color(bar, lv_palette_main(LV_PALETTE_ORANGE), 0);
        lv_obj_set_style_bg_color(bar, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_INDICATOR); 
    } else if (state.level <= 30) {
        lv_obj_set_style_border_color(bar, lv_palette_main(LV_PALETTE_YELLOW), 0);
        lv_obj_set_style_bg_color(bar, lv_palette_main(LV_PALETTE_YELLOW), LV_PART_INDICATOR); 
    } else if (state.level <= 90) {
        lv_obj_set_style_border_color(bar, lv_palette_main(LV_PALETTE_GREEN), 0);
        lv_obj_set_style_bg_color(bar, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR); 
    } else {
        lv_obj_set_style_border_color(bar, lv_palette_main(LV_PALETTE_INDIGO), 0);
        lv_obj_set_style_bg_color(bar, lv_palette_main(LV_PALETTE_INDIGO), LV_PART_INDICATOR); 
    }

    lv_obj_clear_flag(bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(bar);
}

void battery_status_update_cb(struct battery_state state) {
    struct zmk_widget_dongle_battery_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_battery_symbol(widget->obj, state); }
}

static struct battery_state peripheral_battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *ev = as_zmk_peripheral_battery_state_changed(eh);
    return (struct battery_state){
        .source = ev->source + SOURCE_OFFSET,
        .level = ev->state_of_charge,
    };
}

static struct battery_state central_battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);
    return (struct battery_state) {
        .source = 0,
        .level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
        #if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
        #endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
    };
}

static struct battery_state battery_status_get_state(const zmk_event_t *eh) { 
    if (as_zmk_peripheral_battery_state_changed(eh) != NULL) {
        return peripheral_battery_status_get_state(eh);
    } else {
        return central_battery_status_get_state(eh);
    }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_dongle_battery_status, struct battery_state,
                            battery_status_update_cb, battery_status_get_state)

ZMK_SUBSCRIPTION(widget_dongle_battery_status, zmk_peripheral_battery_state_changed);

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY)
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

ZMK_SUBSCRIPTION(widget_dongle_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_dongle_battery_status, zmk_usb_conn_state_changed);
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
#endif /* !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) */
#endif /* IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY) */

int zmk_widget_dongle_battery_status_init(struct zmk_widget_dongle_battery_status *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 240, 40);
    
    for (int i = 0; i < ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT + SOURCE_OFFSET; i++) {
        lv_obj_t * bar = lv_bar_create(widget->obj);

        // Initial style of background of the bar.
        lv_style_init(&style_bg);
        lv_style_set_border_color(&style_bg, lv_color_white());
        lv_style_set_border_width(&style_bg, 1);
        lv_style_set_radius(&style_bg, 7);

        // Initial style of moving indicator of the bar.
        lv_style_init(&style_indic);
        lv_style_set_bg_opa(&style_indic, LV_OPA_COVER);
        lv_style_set_bg_color(&style_indic, lv_color_white());
        lv_style_set_radius(&style_indic, 5);

        // Remove all pre-defined styling
        lv_obj_remove_style_all(bar);
        lv_obj_add_style(bar, &style_bg, 0);
        lv_obj_add_style(bar, &style_indic, LV_PART_INDICATOR);
        
        // Setup the size, range, and location of the bar.
        // These items do not change, so we can set them here.
        lv_obj_set_size(bar, BATT_BAR_LENGTH, BATT_BAR_HEIGHT);
        lv_bar_set_range(bar, BATT_BAR_MIN, BATT_BAR_MAX);
        lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, -60 +(i * 120), -10);
        lv_obj_add_event_cb(bar, event_cb, LV_EVENT_DRAW_PART_END, NULL);

        lv_obj_t * label = lv_bar_create(widget->bar);

        // Finally, pakage the objects into the collector.
        battery_objects[i] = (struct battery_object){
            .bar = bar,
        };
    }

    sys_slist_append(&widgets, &widget->node);

    // Initialize peripheral tracking
    init_peripheral_tracking();

    widget_dongle_battery_status_init();

    return 0;
}

lv_obj_t *zmk_widget_dongle_battery_status_obj(struct zmk_widget_dongle_battery_status *widget) {
    return widget->obj;
}
