/*
 *
 * Copyright (c) 2024 The ZMK Contributors
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <lvgl.h>
#include "widgets/brightness_status.h"

extern struct zmk_widget_brightness_status brightness_status_widget;

lv_obj_t *zmk_display_status_screen();
