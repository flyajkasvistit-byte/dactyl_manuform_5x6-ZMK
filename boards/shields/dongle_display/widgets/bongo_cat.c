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
#include <zmk/events/wpm_state_changed.h>
#include <zmk/wpm.h>

#include "bongo_cat.h"

#define SRC(array) (const void **)array, sizeof(array) / sizeof(lv_img_dsc_t *)

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

LV_IMG_DECLARE(bongo_cat_none);
LV_IMG_DECLARE(bongo_cat_left1);
LV_IMG_DECLARE(bongo_cat_left2);
LV_IMG_DECLARE(bongo_cat_right1);
LV_IMG_DECLARE(bongo_cat_right2);
LV_IMG_DECLARE(bongo_cat_both1);
LV_IMG_DECLARE(bongo_cat_both1_open);
LV_IMG_DECLARE(bongo_cat_both2);

#define ANIMATION_SPEED_IDLE 10000
const lv_img_dsc_t *idle_imgs[] = {
    &bongo_cat_both1_open,
    &bongo_cat_both1_open,
    &bongo_cat_both1_open,
    &bongo_cat_both1,
};

#define ANIMATION_SPEED_SLOW 2000
const lv_img_dsc_t *slow_imgs[] = {
    &bongo_cat_left1,
    &bongo_cat_both1,
    &bongo_cat_both1,
    &bongo_cat_right1,
    &bongo_cat_both1,
    &bongo_cat_both1,
    &bongo_cat_left1,
    &bongo_cat_both1,
    &bongo_cat_both1,
};

#define ANIMATION_SPEED_MID 500
const lv_img_dsc_t *mid_imgs[] = {
    &bongo_cat_left2,
    &bongo_cat_left1,
    &bongo_cat_none,
    &bongo_cat_right2,
    &bongo_cat_right1,
    &bongo_cat_none,
};

#define ANIMATION_SPEED_FAST 200
const lv_img_dsc_t *fast_imgs[] = {
    &bongo_cat_both2,
    &bongo_cat_both1,
    &bongo_cat_none,
    &bongo_cat_none,
};

struct bongo_cat_wpm_status_state {
    uint8_t wpm;
};

enum anim_state {
    anim_state_none,
    anim_state_idle,
    anim_state_slow,
    anim_state_mid,
    anim_state_fast
} current_anim_state;

static void create_keyboard(lv_obj_t *parent) {
    static lv_style_t keyboard_style;
    static lv_style_t key_line_style;
    static bool styles_ready;

    static const lv_point_t row_points[] = {{1, 3}, {22, 3}};
    static const lv_point_t col1_points[] = {{4, 1}, {4, 5}};
    static const lv_point_t col2_points[] = {{8, 1}, {8, 5}};
    static const lv_point_t col3_points[] = {{12, 1}, {12, 5}};
    static const lv_point_t col4_points[] = {{16, 1}, {16, 5}};
    static const lv_point_t col5_points[] = {{20, 1}, {20, 5}};

    if (!styles_ready) {
        lv_style_init(&keyboard_style);
        lv_style_set_bg_opa(&keyboard_style, LV_OPA_TRANSP);
        lv_style_set_border_width(&keyboard_style, 1);
        lv_style_set_border_color(&keyboard_style, lv_color_black());
        lv_style_set_radius(&keyboard_style, 0);
        lv_style_set_pad_all(&keyboard_style, 0);

        lv_style_init(&key_line_style);
        lv_style_set_line_width(&key_line_style, 1);
        lv_style_set_line_color(&key_line_style, lv_color_black());

        styles_ready = true;
    }

    lv_obj_t *keyboard = lv_obj_create(parent);
    lv_obj_remove_style_all(keyboard);
    lv_obj_add_style(keyboard, &keyboard_style, LV_PART_MAIN);
    lv_obj_set_size(keyboard, 24, 7);
    lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, -1, -1);

    lv_obj_t *row = lv_line_create(keyboard);
    lv_line_set_points(row, row_points, 2);
    lv_obj_add_style(row, &key_line_style, LV_PART_MAIN);

    lv_obj_t *col1 = lv_line_create(keyboard);
    lv_line_set_points(col1, col1_points, 2);
    lv_obj_add_style(col1, &key_line_style, LV_PART_MAIN);

    lv_obj_t *col2 = lv_line_create(keyboard);
    lv_line_set_points(col2, col2_points, 2);
    lv_obj_add_style(col2, &key_line_style, LV_PART_MAIN);

    lv_obj_t *col3 = lv_line_create(keyboard);
    lv_line_set_points(col3, col3_points, 2);
    lv_obj_add_style(col3, &key_line_style, LV_PART_MAIN);

    lv_obj_t *col4 = lv_line_create(keyboard);
    lv_line_set_points(col4, col4_points, 2);
    lv_obj_add_style(col4, &key_line_style, LV_PART_MAIN);

    lv_obj_t *col5 = lv_line_create(keyboard);
    lv_line_set_points(col5, col5_points, 2);
    lv_obj_add_style(col5, &key_line_style, LV_PART_MAIN);
}

static void set_animation(lv_obj_t *animing, struct bongo_cat_wpm_status_state state) {
    if (state.wpm < 5) {
        if (current_anim_state != anim_state_idle) {
            lv_animimg_set_src(animing, SRC(idle_imgs));
            lv_animimg_set_duration(animing, ANIMATION_SPEED_IDLE);
            lv_animimg_set_repeat_count(animing, LV_ANIM_REPEAT_INFINITE);
            lv_animimg_start(animing);
            current_anim_state = anim_state_idle;
        }
    } else if (state.wpm < 30) {
        if (current_anim_state != anim_state_slow) {
            lv_animimg_set_src(animing, SRC(slow_imgs));
            lv_animimg_set_duration(animing, ANIMATION_SPEED_SLOW);
            lv_animimg_set_repeat_count(animing, LV_ANIM_REPEAT_INFINITE);
            lv_animimg_start(animing);
            current_anim_state = anim_state_slow;
        }
    } else if (state.wpm < 70) {
        if (current_anim_state != anim_state_mid) {
            lv_animimg_set_src(animing, SRC(mid_imgs));
            lv_animimg_set_duration(animing, ANIMATION_SPEED_MID);
            lv_animimg_set_repeat_count(animing, LV_ANIM_REPEAT_INFINITE);
            lv_animimg_start(animing);
            current_anim_state = anim_state_mid;
        }
    } else {
        if (current_anim_state != anim_state_fast) {
            lv_animimg_set_src(animing, SRC(fast_imgs));
            lv_animimg_set_duration(animing, ANIMATION_SPEED_FAST);
            lv_animimg_set_repeat_count(animing, LV_ANIM_REPEAT_INFINITE);
            lv_animimg_start(animing);
            current_anim_state = anim_state_fast;
        }
    }
}

struct bongo_cat_wpm_status_state bongo_cat_wpm_status_get_state(const zmk_event_t *eh) {
    struct zmk_wpm_state_changed *ev = as_zmk_wpm_state_changed(eh);
    return (struct bongo_cat_wpm_status_state) { .wpm = ev->state };
};

void bongo_cat_wpm_status_update_cb(struct bongo_cat_wpm_status_state state) {
    struct zmk_widget_bongo_cat *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_animation(widget->obj, state); }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_bongo_cat, struct bongo_cat_wpm_status_state,
                            bongo_cat_wpm_status_update_cb, bongo_cat_wpm_status_get_state)

ZMK_SUBSCRIPTION(widget_bongo_cat, zmk_wpm_state_changed);

int zmk_widget_bongo_cat_init(struct zmk_widget_bongo_cat *widget, lv_obj_t *parent) {
    widget->obj = lv_animimg_create(parent);
    lv_obj_center(widget->obj);
    create_keyboard(widget->obj);

    sys_slist_append(&widgets, &widget->node);

    widget_bongo_cat_init();

    return 0;
}

lv_obj_t *zmk_widget_bongo_cat_obj(struct zmk_widget_bongo_cat *widget) {
    return widget->obj;
}
