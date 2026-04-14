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

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct battery_state {
    uint8_t source;
    uint8_t level;
    bool usb_present;
};

struct battery_object {
    lv_obj_t *bar;
} battery_objects[ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT + SOURCE_OFFSET];

/* Styles (initialized once) */
static lv_style_t style_bg;
static lv_style_t style_indic;
static bool styles_initialized = false;

/* Peripheral tracking */
static int8_t last_battery_levels[ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT + SOURCE_OFFSET];

static void init_peripheral_tracking(void) {
    for (int i = 0; i < (ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT + SOURCE_OFFSET); i++) {
        last_battery_levels[i] = -1;
    }
}

static bool is_peripheral_reconnecting(uint8_t source, uint8_t new_level) {
    if (source >= (ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT + SOURCE_OFFSET)) {
        return false;
    }

    int8_t previous_level = last_battery_levels[source];
    bool reconnecting = (previous_level < 1) && (new_level >= 1);

    if (reconnecting) {
        LOG_INF("Peripheral %d reconnection: %d%% -> %d%%",
                source, previous_level, new_level);
    }

    return reconnecting;
}

/* LVGL v9 draw event */
static void event_cb(lv_event_t *e) {
    lv_draw_task_t *draw_task = lv_event_get_draw_task(e);
    if (!draw_task) return;

    lv_draw_dsc_base_t *base_dsc = lv_draw_task_get_draw_dsc(draw_task);
    if (base_dsc->part != LV_PART_INDICATOR) return;

    lv_obj_t *obj = lv_event_get_target(e);

    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.font = LV_FONT_DEFAULT;

    char buf[8];
    lv_snprintf(buf, sizeof(buf), "%d", (int)lv_bar_get_value(obj));

    lv_point_t txt_size;
    lv_text_get_size(&txt_size, buf, label_dsc.font,
                     label_dsc.letter_space,
                     label_dsc.line_space,
                     LV_COORD_MAX,
                     label_dsc.flag);

    lv_area_t txt_area;
    lv_area_t *draw_area = lv_draw_task_get_draw_dsc(task);

    if (lv_area_get_width(draw_area) > txt_size.x + 20) {
        txt_area.x2 = draw_area->x2 - 5;
        txt_area.x1 = txt_area.x2 - txt_size.x + 1;
        label_dsc.color = lv_color_white();
    } else {
        txt_area.x1 = draw_area->x2 + 5;
        txt_area.x2 = txt_area.x1 + txt_size.x - 1;
        label_dsc.color = lv_color_black();
    }

    txt_area.y1 = draw_area->y1 +
                  (lv_area_get_height(draw_area) - txt_size.y) / 2;
    txt_area.y2 = txt_area.y1 + txt_size.y - 1;

    lv_draw_label(draw_task->draw_ctx, &label_dsc, &txt_area, buf, NULL);
}

/* Helper for battery color */
static lv_color_t get_battery_color(uint8_t level) {
    if (level <= 10) return lv_palette_main(LV_PALETTE_RED);
    if (level <= 20) return lv_palette_main(LV_PALETTE_ORANGE);
    if (level <= 30) return lv_palette_main(LV_PALETTE_YELLOW);
    if (level <= 90) return lv_palette_main(LV_PALETTE_GREEN);
    return lv_palette_main(LV_PALETTE_INDIGO);
}

static void set_battery_symbol(lv_obj_t *widget, struct battery_state state) {
    if (state.source >= ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT + SOURCE_OFFSET) {
        return;
    }

    bool reconnecting = is_peripheral_reconnecting(state.source, state.level);
    last_battery_levels[state.source] = state.level;

    if (reconnecting) {
#if CONFIG_DONGLE_SCREEN_IDLE_TIMEOUT_S > 0
        brightness_wake_screen_on_reconnect();
#endif
    }

    lv_obj_t *bar = battery_objects[state.source].bar;
    if (!bar) return;

    lv_bar_set_value(bar, state.level, LV_ANIM_ON);

    lv_color_t color = get_battery_color(state.level);

    lv_obj_set_style_border_color(bar, color, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR);

    lv_obj_clear_flag(bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(bar);
}

void battery_status_update_cb(struct battery_state state) {
    struct zmk_widget_dongle_battery_status *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        set_battery_symbol(widget->obj, state);
    }
}

/* Event → state conversion */
static struct battery_state peripheral_battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *ev =
        as_zmk_peripheral_battery_state_changed(eh);

    return (struct battery_state){
        .source = ev->source + SOURCE_OFFSET,
        .level = ev->state_of_charge,
    };
}

static struct battery_state central_battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev =
        as_zmk_battery_state_changed(eh);

    return (struct battery_state){
        .source = 0,
        .level = ev ? ev->state_of_charge : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif
    };
}

static struct battery_state battery_status_get_state(const zmk_event_t *eh) {
    if (as_zmk_peripheral_battery_state_changed(eh)) {
        return peripheral_battery_status_get_state(eh);
    }
    return central_battery_status_get_state(eh);
}

/* ZMK wiring */
ZMK_DISPLAY_WIDGET_LISTENER(widget_dongle_battery_status,
                            struct battery_state,
                            battery_status_update_cb,
                            battery_status_get_state)

ZMK_SUBSCRIPTION(widget_dongle_battery_status,
                 zmk_peripheral_battery_state_changed);

#if IS_ENABLED(CONFIG_ZMK_DONGLE_DISPLAY_DONGLE_BATTERY)
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
ZMK_SUBSCRIPTION(widget_dongle_battery_status,
                 zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_dongle_battery_status,
                 zmk_usb_conn_state_changed);
#endif
#endif
#endif

int zmk_widget_dongle_battery_status_init(
    struct zmk_widget_dongle_battery_status *widget,
    lv_obj_t *parent) {

    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 240, 40);

    /* Init styles once */
    if (!styles_initialized) {
        styles_initialized = true;

        lv_style_init(&style_bg);
        lv_style_set_border_color(&style_bg, lv_color_white());
        lv_style_set_border_width(&style_bg, 1);
        lv_style_set_radius(&style_bg, 7);

        lv_style_init(&style_indic);
        lv_style_set_bg_opa(&style_indic, LV_OPA_COVER);
        lv_style_set_bg_color(&style_indic, lv_color_white());
        lv_style_set_radius(&style_indic, 5);
    }

    for (int i = 0; i < ZMK_SPLIT_CENTRAL_PERIPHERAL_COUNT + SOURCE_OFFSET; i++) {
        lv_obj_t *bar = lv_bar_create(widget->obj);

        lv_obj_remove_style_all(bar);
        lv_obj_add_style(bar, &style_bg, LV_PART_MAIN);
        lv_obj_add_style(bar, &style_indIc, LV_PART_INDICATOR);

        lv_obj_set_size(bar, BATT_BAR_LENGTH, BATT_BAR_HEIGHT);
        lv_bar_set_range(bar, BATT_BAR_MIN, BATT_BAR_MAX);

        lv_obj_add_flag(bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, -60 + (i * 120), -10);

        /* LVGL v9 draw hook */
        lv_obj_add_event_cb(bar, event_cb, LV_EVENT_DRAW_TASK_ADDED, NULL);

        battery_objects[i].bar = bar;
    }

    sys_slist_append(&widgets, &widget->node);

    init_peripheral_tracking();
    widget_dongle_battery_status_init();

    return 0;
}

lv_obj_t *zmk_widget_dongle_battery_status_obj(
    struct zmk_widget_dongle_battery_status *widget) {
    return widget ? widget->obj : NULL;
}
