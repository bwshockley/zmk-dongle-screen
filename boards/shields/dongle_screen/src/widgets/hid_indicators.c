#include <zephyr/kernel.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/hid_indicators_changed.h>
#include <lvgl.h>

struct hid_indicators_widget {
    lv_obj_t *container;
    lv_obj_t *caps_label;
    lv_obj_t *num_label;
    lv_obj_t *scroll_label;
};

static struct hid_indicators_widget indicators_widget;

static void update_indicators(bool caps, bool num, bool scroll) {
    lv_color_t active_color = lv_palette_main(LV_PALETTE_GREEN);
    lv_color_t inactive_color = lv_palette_main(LV_PALETTE_GREY);

    lv_label_set_text(indicators_widget.caps_label, "CAPS");
    lv_label_set_text(indicators_widget.num_label, "NUM");
    lv_label_set_text(indicators_widget.scroll_label, "SCRL");

    lv_obj_set_style_text_color(
        indicators_widget.caps_label,
        caps ? active_color : inactive_color,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_color(
        indicators_widget.num_label,
        num ? active_color : inactive_color,
        LV_PART_MAIN
    );

    lv_obj_set_style_text_color(
        indicators_widget.scroll_label,
        scroll ? active_color : inactive_color,
        LV_PART_MAIN
    );
}

static void hid_indicators_changed_listener(const struct zmk_event_header *eh) {
    const struct zmk_hid_indicators_changed *ev = cast_zmk_hid_indicators_changed(eh);
    update_indicators(ev->caps_lock, ev->num_lock, ev->scroll_lock);
}

ZMK_LISTENER(hid_indicators_widget, hid_indicators_changed_listener);
ZMK_SUBSCRIPTION(hid_indicators_widget, zmk_hid_indicators_changed);

lv_obj_t *hid_indicators_widget_create(lv_obj_t *parent) {
    indicators_widget.container = lv_obj_create(parent);
    lv_obj_set_layout(indicators_widget.container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(indicators_widget.container, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(indicators_widget.container, 4, LV_PART_MAIN);

    indicators_widget.caps_label = lv_label_create(indicators_widget.container);
    indicators_widget.num_label = lv_label_create(indicators_widget.container);
    indicators_widget.scroll_label = lv_label_create(indicators_widget.container);

    // Initial state: all off
    update_indicators(false, false, false);

    return indicators_widget.container;
}
