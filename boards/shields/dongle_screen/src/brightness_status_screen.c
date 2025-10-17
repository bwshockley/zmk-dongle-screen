/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include "brightness_status_screen.h"

#include "widgets/brightness_status.h"
struct zmk_widget_brightness_status brightness_status_widget;

lv_style_t global_style;

lv_obj_t *zmk_brightness_status_screen()
{
  // Create the screen
  lv_obj_t *screen;
  screen = lv_obj_create(NULL);

  // Setup the screen background
  lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(screen, 255, LV_PART_MAIN);

  // Setup basic styling, mainly just text color.
  lv_style_init(&global_style);
  lv_style_set_text_color(&global_style, lv_color_white());
  lv_obj_add_style(screen, &global_style, LV_PART_MAIN);

  zmk_widget_brightness_status_init(&brightness_status_widget, screen);
  lv_obj_align(zmk_widget_brightness_status_obj(&brightness_status_widget), LV_ALIGN_CENTER, 0, 0);

  return screen;
}
