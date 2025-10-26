#include "layer_roller.h"

#include <ctype.h>
#include <zmk/display.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/event_manager.h>
#include <zmk/keymap.h>

#include <fonts.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

static char layer_names_buffer[256] = {0}; // Buffer for concatenated layer names
static int layer_select_id[ZMK_KEYMAP_LAYERS_LEN] = {0};   // Maps layer index to display position
static int layer_display_order[ZMK_KEYMAP_LAYERS_LEN] = {0}; // Maps display position to layer index
static int total_layers = 0;

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

struct layer_roller_state {
    uint8_t index;
};

static void layer_roller_set_sel(lv_obj_t *roller, struct layer_roller_state state) {
    if (state.index >= ZMK_KEYMAP_LAYERS_LEN || layer_select_id[state.index] == -1) {
        return;
    }

    int display_pos = layer_select_id[state.index];

    // Set color based on layer index
    if (state.index == 1) {
        lv_obj_set_style_text_color(roller, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_SELECTED);
    } else if (state.index == 4) {
        lv_obj_set_style_text_color(roller, lv_palette_main(LV_PALETTE_GREEN), LV_PART_SELECTED);
    } else {
        lv_obj_set_style_text_color(roller, lv_color_white(), LV_PART_SELECTED);
    }
    lv_roller_set_selected(roller, display_pos, LV_ANIM_ON);
}

static void layer_roller_update_cb(struct layer_roller_state state) {
    struct zmk_widget_layer_roller *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        layer_roller_set_sel(widget->obj, state);
    }
}

static struct layer_roller_state layer_roller_get_state(const zmk_event_t *eh) {
    uint8_t index = zmk_keymap_highest_layer_active();
    return (struct layer_roller_state){
        .index = index,
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_roller, struct layer_roller_state, layer_roller_update_cb,
                            layer_roller_get_state)
ZMK_SUBSCRIPTION(widget_layer_roller, zmk_layer_state_changed);

static void mask_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);

    static int16_t mask_top_id = -1;
    static int16_t mask_bottom_id = -1;

    if(code == LV_EVENT_COVER_CHECK) {
        lv_event_set_cover_res(e, LV_COVER_RES_MASKED);

    }
    else if(code == LV_EVENT_DRAW_MAIN_BEGIN) {
        /* add mask */
        const lv_font_t * font = lv_obj_get_style_text_font(obj, LV_PART_SELECTED);
        lv_coord_t line_space = lv_obj_get_style_text_line_space(obj, LV_PART_MAIN);
        lv_coord_t font_h = lv_font_get_line_height(font);

        lv_area_t roller_coords;
        lv_obj_get_coords(obj, &roller_coords);

        lv_area_t rect_area;
        rect_area.x1 = roller_coords.x1;
        rect_area.x2 = roller_coords.x2;
        rect_area.y1 = roller_coords.y1;
        // rect_area.y2 = roller_coords.y1 + (lv_obj_get_height(obj) - font_h - line_space) / 2;
        rect_area.y2 = roller_coords.y1 + (lv_obj_get_height(obj) - font_h) / 2;

        lv_draw_mask_fade_param_t * fade_mask_top = lv_mem_buf_get(sizeof(lv_draw_mask_fade_param_t));
        lv_draw_mask_fade_init(fade_mask_top, &rect_area, LV_OPA_TRANSP, rect_area.y1, LV_OPA_COVER, rect_area.y2);
        mask_top_id = lv_draw_mask_add(fade_mask_top, NULL);

        rect_area.y1 = rect_area.y2 + font_h + line_space - 1;
        rect_area.y2 = roller_coords.y2;

        lv_draw_mask_fade_param_t * fade_mask_bottom = lv_mem_buf_get(sizeof(lv_draw_mask_fade_param_t));
        lv_draw_mask_fade_init(fade_mask_bottom, &rect_area, LV_OPA_COVER, rect_area.y1, LV_OPA_TRANSP, rect_area.y2);
        mask_bottom_id = lv_draw_mask_add(fade_mask_bottom, NULL);

    }
    else if(code == LV_EVENT_DRAW_POST_END) {
        lv_draw_mask_fade_param_t * fade_mask_top = lv_draw_mask_remove_id(mask_top_id);
        lv_draw_mask_fade_param_t * fade_mask_bottom = lv_draw_mask_remove_id(mask_bottom_id);
        lv_draw_mask_free_param(fade_mask_top);
        lv_draw_mask_free_param(fade_mask_bottom);
        lv_mem_buf_release(fade_mask_top);
        lv_mem_buf_release(fade_mask_bottom);
        mask_top_id = -1;
        mask_bottom_id = -1;
    }
}

static void init_layer_arrays(void) {
    static bool initialized = false;
    if (initialized) {
        return; // Already initialized
    }
    initialized = true;

    // Count available layers
    total_layers = 0;
    for (int i = 0; i < ZMK_KEYMAP_LAYERS_LEN; i++) {
        if (zmk_keymap_layer_name(i) != NULL) {
            total_layers++;
        }
    }

    // Initialize arrays with invalid values
    for (int i = 0; i < ZMK_KEYMAP_LAYERS_LEN; i++) {
        layer_select_id[i] = -1;
        layer_display_order[i] = -1;
    }

    // Initialize arrays with layer 0 in the center
    int center_pos = total_layers / 2;
    
    // Place layer 0 in the center
    layer_display_order[center_pos] = 0;
    layer_select_id[0] = center_pos;
    
    // Fill positions before center with odd numbers
    int odd = 1;
    for (int i = center_pos - 1; i >= 0; i--) {
        if (odd < total_layers) {
            layer_display_order[i] = odd;
            layer_select_id[odd] = i;
            odd += 2;
        }
    }
    
    // Fill positions after center with even numbers
    int even = 2;
    for (int i = center_pos + 1; i < total_layers; i++) {
        if (even < total_layers) {
            layer_display_order[i] = even;
            layer_select_id[even] = i;
            even += 2;
        }
    }

    LOG_DBG("Layer display order:");
    for (int i = 0; i < total_layers; i++) {
        LOG_DBG("Position %d: Layer %d", i, layer_display_order[i]);
    }
}

int zmk_widget_layer_roller_init(struct zmk_widget_layer_roller *widget, lv_obj_t *parent) {
    init_layer_arrays();

    widget->obj = lv_roller_create(parent);
    lv_obj_set_size(widget->obj, 240, 80);

    static lv_style_t style;
    lv_style_init(&style);
    lv_style_set_bg_color(&style, lv_color_black());
    lv_style_set_text_color(&style, lv_color_white());
    lv_style_set_text_line_space(&style, 0);
    //lv_style_set_border_width(&style, 1);
    //lv_style_set_border_color(&style, lv_palette_main(LV_PALETTE_LIGHT_BLUE));
    lv_style_set_pad_all(&style, 0);
    lv_obj_add_style(widget->obj, &style, 0);

    // Set the background opacity, text size, and color for the selected layer.
    lv_obj_set_style_text_align(widget->obj, LV_ALIGN_LEFT_MID, LV_PART_SELECTED);
    lv_obj_set_style_bg_opa(widget->obj, LV_OPA_TRANSP, LV_PART_SELECTED);
    lv_obj_set_style_text_font(widget->obj, &lv_font_montserrat_40, LV_PART_SELECTED);   
    lv_obj_set_style_text_color(widget->obj, lv_color_white(), LV_PART_SELECTED);

    // Set the text size and color of the non-selected layers.
    lv_obj_set_style_text_font(widget->obj, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_set_style_text_color(widget->obj, lv_palette_darken(LV_PALETTE_GREY,4), LV_PART_MAIN);

    layer_names_buffer[0] = '\0';
    char *ptr = layer_names_buffer;

    for (int i = 0; i < total_layers; i++) {
        const char *layer_name = zmk_keymap_layer_name(layer_display_order[i]);
        if (layer_name) {

            // For each layer name after the first layer name and a newline.
            if (i > 0) {
                strcat(ptr, "\n");
                ptr += strlen(ptr);
            }

            if (layer_name && *layer_name) { //is both valid and points to a non-empty string
                // If the layer names should be all caps, modify them to be so.
                #if IS_ENABLED(CONFIG_LAYER_ROLLER_ALL_CAPS)
                while (*layer_name) {
                    *ptr = toupper((unsigned char)*layer_name);
                    ptr++;
                    layer_name++;
                }
                *ptr = '\0';
                // Otherwise, just add the layer name directly to the buffer.
                #else
                strcat(ptr, layer_name);
                ptr += strlen(layer_name);
            #endif
            // If a layer doesn't have a name, just use the layer number.  Supports up to 99 layers.
            } else {
                char index_str[2];
                snprintf(index_str, sizeof(index_str), "%d", i);
                strcat(ptr, index_str);
                ptr += strlen(index_str);
            }
        }
    }

    lv_roller_set_options(widget->obj, layer_names_buffer, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(widget->obj, 3);
    
    //lv_obj_add_event_cb(widget->obj, mask_event_cb, LV_EVENT_ALL, NULL);
    
    lv_obj_set_style_anim_time(widget->obj, 400, 0);
    
    sys_slist_append(&widgets, &widget->node);
    
    widget_layer_roller_init();
    return 0;
}

lv_obj_t *zmk_widget_layer_roller_obj(struct zmk_widget_layer_roller *widget) {
    return widget->obj;
}
