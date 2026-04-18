/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/wpm_state_changed.h>

#include "typing_stats.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);
struct typing_stats_state {
    uint8_t wpm;
};

static void set_stats(lv_obj_t *label, struct typing_stats_state state) {
    lv_label_set_text_fmt(label, "%2u", state.wpm);
}

static void typing_stats_update_cb(struct typing_stats_state state) {
    struct zmk_widget_typing_stats *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_stats(widget->obj, state); }
}

static struct typing_stats_state typing_stats_get_state(const zmk_event_t *eh) {
    const struct zmk_wpm_state_changed *ev = as_zmk_wpm_state_changed(eh);

    return (struct typing_stats_state){
        .wpm = ev->state,
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_typing_stats, struct typing_stats_state, typing_stats_update_cb,
                            typing_stats_get_state)

ZMK_SUBSCRIPTION(widget_typing_stats, zmk_wpm_state_changed);

int zmk_widget_typing_stats_init(struct zmk_widget_typing_stats *widget, lv_obj_t *parent) {
    widget->obj = lv_label_create(parent);
    lv_label_set_text(widget->obj, " 0");

    sys_slist_append(&widgets, &widget->node);

    widget_typing_stats_init();

    return 0;
}

lv_obj_t *zmk_widget_typing_stats_obj(struct zmk_widget_typing_stats *widget) {
    return widget->obj;
}
