#include <Arduino.h>
#include <lvgl.h>

#include "ui.h"

void set_brightness(int32_t value);

static lv_obj_t *brightness_label;

static void brightness_slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
    int32_t value = lv_slider_get_value(slider);
    set_brightness(value);
    lv_label_set_text_fmt(brightness_label, "Brillo: %d", value);
}

void ui_init(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "HELLO WORLD");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_48, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

    lv_obj_t *slider = lv_slider_create(scr);
    lv_obj_set_size(slider, 400, 50);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, 0);
    lv_slider_set_range(slider, 10, 255);
    lv_slider_set_value(slider, 200, LV_ANIM_OFF);

    lv_obj_set_style_bg_color(slider, lv_color_hex(0x404040), LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0x00BFFF), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider, lv_color_hex(0xFFFFFF), LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider, 20, LV_PART_KNOB);

    lv_obj_add_event_cb(slider, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    brightness_label = lv_label_create(scr);
    lv_label_set_text_fmt(brightness_label, "BRIGHT: %d", 200);
    lv_obj_set_style_text_color(brightness_label, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_text_font(brightness_label, &lv_font_montserrat_24, 0);
    lv_obj_align(brightness_label, LV_ALIGN_CENTER, 0, 80);
}
