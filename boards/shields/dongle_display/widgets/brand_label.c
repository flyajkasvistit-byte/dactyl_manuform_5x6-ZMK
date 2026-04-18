/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>

#include "brand_label.h"

static lv_style_t brand_style;
static bool brand_style_ready;

int zmk_widget_brand_label_init(struct zmk_widget_brand_label *widget, lv_obj_t *parent) {
    widget->obj = lv_label_create(parent);
    lv_label_set_text(widget->obj, "DactylTosh");

    if (!brand_style_ready) {
        lv_style_init(&brand_style);
        lv_style_set_text_letter_space(&brand_style, 0);
        brand_style_ready = true;
    }

    lv_obj_add_style(widget->obj, &brand_style, LV_PART_MAIN);

    return 0;
}

lv_obj_t *zmk_widget_brand_label_obj(struct zmk_widget_brand_label *widget) {
    return widget->obj;
}
