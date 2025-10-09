/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */
 
 #pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_hid_indicators {
    lv_obj_t *cont;
    lv_obj_t *caps_label;
    lv_obj_t *num_label;
    lv_obj_t *scroll_label;
    sys_snode_t node;
};

int zmk_widget_hid_indicators_init(struct zmk_widget_hid_indicators *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_hid_indicators_obj(struct zmk_widget_hid_indicators *widget);
