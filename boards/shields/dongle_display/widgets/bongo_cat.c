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

#define CAT_WIDTH 60
#define CAT_HEIGHT 26
#define CAT_WIDGET_HEIGHT 38

LV_IMG_DECLARE(bongo_cat_none);
LV_IMG_DECLARE(bongo_cat_left1);
LV_IMG_DECLARE(bongo_cat_left2);
LV_IMG_DECLARE(bongo_cat_right1);
LV_IMG_DECLARE(bongo_cat_right2);
LV_IMG_DECLARE(bongo_cat_both1);
LV_IMG_DECLARE(bongo_cat_both1_open);
LV_IMG_DECLARE(bongo_cat_both2);
LV_IMG_DECLARE(bongo_blast_1);
LV_IMG_DECLARE(bongo_blast_2);
LV_IMG_DECLARE(bongo_blast_3);

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

#define ANIMATION_SPEED_BLAST 4500
const lv_img_dsc_t *blast_imgs[] = {
    &bongo_blast_1,
    &bongo_blast_2,
    &bongo_blast_3,
};

struct bongo_cat_wpm_status_state {
    uint8_t wpm;
};

enum anim_state {
    anim_state_none,
    anim_state_idle,
    anim_state_slow,
    anim_state_mid,
    anim_state_fast,
    anim_state_overdrive
} current_anim_state;

static lv_obj_t *create_keyboard_half(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, bool right_half) {
    static lv_style_t keyboard_style;
    static lv_style_t key_line_style;
    static bool styles_ready;

    static const lv_point_t left_top_points[] = {{1, 2}, {17, 0}};
    static const lv_point_t left_bottom_points[] = {{0, 9}, {16, 7}};
    static const lv_point_t left_left_points[] = {{1, 2}, {0, 9}};
    static const lv_point_t left_right_points[] = {{17, 0}, {16, 7}};
    static const lv_point_t left_row_points[] = {{0, 6}, {16, 4}};
    static const lv_point_t left_col1_points[] = {{6, 1}, {5, 8}};
    static const lv_point_t left_col2_points[] = {{11, 1}, {10, 8}};

    static const lv_point_t right_top_points[] = {{1, 0}, {17, 2}};
    static const lv_point_t right_bottom_points[] = {{2, 7}, {18, 9}};
    static const lv_point_t right_left_points[] = {{1, 0}, {2, 7}};
    static const lv_point_t right_right_points[] = {{17, 2}, {18, 9}};
    static const lv_point_t right_row_points[] = {{2, 4}, {18, 6}};
    static const lv_point_t right_col1_points[] = {{8, 1}, {9, 8}};
    static const lv_point_t right_col2_points[] = {{13, 1}, {14, 8}};

    if (!styles_ready) {
        lv_style_init(&keyboard_style);
        lv_style_set_bg_opa(&keyboard_style, LV_OPA_TRANSP);
        lv_style_set_border_width(&keyboard_style, 0);
        lv_style_set_radius(&keyboard_style, 0);
        lv_style_set_pad_all(&keyboard_style, 0);

        lv_style_init(&key_line_style);
        lv_style_set_line_width(&key_line_style, 1);
        lv_style_set_line_color(&key_line_style, lv_color_black());

        styles_ready = true;
    }

    lv_obj_t *half = lv_obj_create(parent);
    lv_obj_remove_style_all(half);
    lv_obj_add_style(half, &keyboard_style, LV_PART_MAIN);
    lv_obj_set_size(half, 19, 10);
    lv_obj_set_pos(half, x, y);

    lv_obj_t *top = lv_line_create(half);
    lv_line_set_points(top, right_half ? right_top_points : left_top_points, 2);
    lv_obj_add_style(top, &key_line_style, LV_PART_MAIN);

    lv_obj_t *bottom = lv_line_create(half);
    lv_line_set_points(bottom, right_half ? right_bottom_points : left_bottom_points, 2);
    lv_obj_add_style(bottom, &key_line_style, LV_PART_MAIN);

    lv_obj_t *left = lv_line_create(half);
    lv_line_set_points(left, right_half ? right_left_points : left_left_points, 2);
    lv_obj_add_style(left, &key_line_style, LV_PART_MAIN);

    lv_obj_t *right = lv_line_create(half);
    lv_line_set_points(right, right_half ? right_right_points : left_right_points, 2);
    lv_obj_add_style(right, &key_line_style, LV_PART_MAIN);

    lv_obj_t *row = lv_line_create(half);
    lv_line_set_points(row, right_half ? right_row_points : left_row_points, 2);
    lv_obj_add_style(row, &key_line_style, LV_PART_MAIN);

    lv_obj_t *col1 = lv_line_create(half);
    lv_line_set_points(col1, right_half ? right_col1_points : left_col1_points, 2);
    lv_obj_add_style(col1, &key_line_style, LV_PART_MAIN);

    lv_obj_t *col2 = lv_line_create(half);
    lv_line_set_points(col2, right_half ? right_col2_points : left_col2_points, 2);
    lv_obj_add_style(col2, &key_line_style, LV_PART_MAIN);

    return half;
}

static lv_obj_t *create_keyboard(lv_obj_t *parent) {
    lv_obj_t *keyboard = lv_obj_create(parent);
    lv_obj_remove_style_all(keyboard);
    lv_obj_set_size(keyboard, 50, 12);
    lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, -4);

    create_keyboard_half(keyboard, 0, 1, false);
    create_keyboard_half(keyboard, 31, 1, true);

    return keyboard;
}

static lv_obj_t *create_blast(lv_obj_t *parent, lv_obj_t *keyboard) {
    lv_obj_t *blast = lv_animimg_create(parent);
    lv_animimg_set_src(blast, SRC(blast_imgs));
    lv_animimg_set_duration(blast, ANIMATION_SPEED_BLAST);
    lv_animimg_set_repeat_count(blast, LV_ANIM_REPEAT_INFINITE);
    lv_obj_align_to(blast, keyboard, LV_ALIGN_OUT_TOP_MID, 0, 0);
    lv_animimg_start(blast);
    lv_obj_add_flag(blast, LV_OBJ_FLAG_HIDDEN);

    return blast;
}

static void set_blast_visible(struct zmk_widget_bongo_cat *widget, bool visible) {
    if (visible) {
        lv_obj_clear_flag(widget->blast_obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(widget->blast_obj, LV_OBJ_FLAG_HIDDEN);
    }
}

static void set_animation(struct zmk_widget_bongo_cat *widget, struct bongo_cat_wpm_status_state state) {
    lv_obj_t *animing = widget->anim_obj;

    if (state.wpm < 5) {
        if (current_anim_state != anim_state_idle) {
            lv_animimg_set_src(animing, SRC(idle_imgs));
            lv_animimg_set_duration(animing, ANIMATION_SPEED_IDLE);
            lv_animimg_set_repeat_count(animing, LV_ANIM_REPEAT_INFINITE);
            lv_animimg_start(animing);
            set_blast_visible(widget, false);
            current_anim_state = anim_state_idle;
        }
    } else if (state.wpm < 30) {
        if (current_anim_state != anim_state_slow) {
            lv_animimg_set_src(animing, SRC(slow_imgs));
            lv_animimg_set_duration(animing, ANIMATION_SPEED_SLOW);
            lv_animimg_set_repeat_count(animing, LV_ANIM_REPEAT_INFINITE);
            lv_animimg_start(animing);
            set_blast_visible(widget, false);
            current_anim_state = anim_state_slow;
        }
    } else if (state.wpm < 70) {
        if (current_anim_state != anim_state_mid) {
            lv_animimg_set_src(animing, SRC(mid_imgs));
            lv_animimg_set_duration(animing, ANIMATION_SPEED_MID);
            lv_animimg_set_repeat_count(animing, LV_ANIM_REPEAT_INFINITE);
            lv_animimg_start(animing);
            set_blast_visible(widget, false);
            current_anim_state = anim_state_mid;
        }
    } else if (state.wpm < 100) {
        if (current_anim_state != anim_state_fast) {
            lv_animimg_set_src(animing, SRC(fast_imgs));
            lv_animimg_set_duration(animing, ANIMATION_SPEED_FAST);
            lv_animimg_set_repeat_count(animing, LV_ANIM_REPEAT_INFINITE);
            lv_animimg_start(animing);
            set_blast_visible(widget, false);
            current_anim_state = anim_state_fast;
        }
    } else {
        if (current_anim_state != anim_state_overdrive) {
            lv_animimg_set_src(animing, SRC(fast_imgs));
            lv_animimg_set_duration(animing, 90);
            lv_animimg_set_repeat_count(animing, LV_ANIM_REPEAT_INFINITE);
            lv_animimg_start(animing);
            set_blast_visible(widget, true);
            current_anim_state = anim_state_overdrive;
        }
    }
}

struct bongo_cat_wpm_status_state bongo_cat_wpm_status_get_state(const zmk_event_t *eh) {
    struct zmk_wpm_state_changed *ev = as_zmk_wpm_state_changed(eh);
    return (struct bongo_cat_wpm_status_state) { .wpm = ev->state };
};

void bongo_cat_wpm_status_update_cb(struct bongo_cat_wpm_status_state state) {
    struct zmk_widget_bongo_cat *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_animation(widget, state); }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_bongo_cat, struct bongo_cat_wpm_status_state,
                            bongo_cat_wpm_status_update_cb, bongo_cat_wpm_status_get_state)

ZMK_SUBSCRIPTION(widget_bongo_cat, zmk_wpm_state_changed);

int zmk_widget_bongo_cat_init(struct zmk_widget_bongo_cat *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_remove_style_all(widget->obj);
    lv_obj_set_size(widget->obj, CAT_WIDTH, CAT_WIDGET_HEIGHT);
    lv_obj_center(widget->obj);

    widget->anim_obj = lv_animimg_create(widget->obj);
    lv_obj_set_size(widget->anim_obj, CAT_WIDTH, CAT_HEIGHT);
    lv_obj_align(widget->anim_obj, LV_ALIGN_TOP_LEFT, 0, 0);

    widget->keyboard_obj = create_keyboard(widget->obj);
    widget->blast_obj = create_blast(widget->obj, widget->keyboard_obj);

    sys_slist_append(&widgets, &widget->node);

    widget_bongo_cat_init();
    set_animation(widget, (struct bongo_cat_wpm_status_state){.wpm = 0});

    return 0;
}

lv_obj_t *zmk_widget_bongo_cat_obj(struct zmk_widget_bongo_cat *widget) {
    return widget->obj;
}
