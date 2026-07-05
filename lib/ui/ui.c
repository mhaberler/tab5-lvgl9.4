#ifdef LVGL_UI

#include <Arduino.h>
#include <lvgl.h>
#include <stdlib.h>

#include "ui.h"
#include "lv_linear_scale.h"

void set_brightness(int32_t value);
void report_brightness(int32_t value);

static lv_obj_t *brightness_label;

/* ---- linear scale demo: 2 instances, random walk @ 2 Hz ---- */

#define LS_DEMO_COUNT 2

static lv_obj_t *demo_scales[LS_DEMO_COUNT];
static float demo_values[LS_DEMO_COUNT];

static float frand(float lo, float hi)
{
    return lo + (hi - lo) * ((float)rand() / (float)RAND_MAX);
}

static void demo_tick(lv_timer_t *t)
{
    LV_UNUSED(t);
    for (int i = 0; i < LS_DEMO_COUNT; i++)
    {
        /* mean-reverting walk: dwells in stretched +-1 region */
        demo_values[i] += -0.15f * demo_values[i] + frand(-1.5f, 1.5f);
        if (demo_values[i] < -10.0f) demo_values[i] = -10.0f;
        if (demo_values[i] > 10.0f) demo_values[i] = 10.0f;

        float spread = frand(0.5f, 2.0f);
        lv_linear_scale_set_value(demo_scales[i], demo_values[i], LV_ANIM_ON);
        lv_linear_scale_set_confidence(demo_scales[i],
                                       demo_values[i] - spread,
                                       demo_values[i] + spread, LV_ANIM_ON);
    }
}

static void brightness_slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = (lv_obj_t *)lv_event_get_target(e);
    int32_t value = lv_slider_get_value(slider);
    set_brightness(value);
    report_brightness(value);
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

    /* two vertical linear scales, left edge */
    for (int i = 0; i < LS_DEMO_COUNT; i++)
    {
        demo_scales[i] = lv_linear_scale_create(scr);
        lv_obj_set_size(demo_scales[i], 130, 720);
        lv_obj_set_pos(demo_scales[i], 10 + i * 140, 0);
        demo_values[i] = 0.0f;
    }
    lv_timer_create(demo_tick, 500, NULL);
}
#endif