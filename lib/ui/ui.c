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
/* instance 0: vSpeed-like +-10 domain; instance 1: vAccel-like +-1 domain */
static const float demo_range[LS_DEMO_COUNT] = {10.0f, 1.0f};

static float frand(float lo, float hi)
{
    return lo + (hi - lo) * ((float)rand() / (float)RAND_MAX);
}

static void demo_tick(lv_timer_t *t)
{
    LV_UNUSED(t);
    for (int i = 0; i < LS_DEMO_COUNT; i++)
    {
        float r = demo_range[i];
        /* mean-reverting walk: dwells in stretched center region */
        demo_values[i] += -0.15f * demo_values[i] + frand(-0.15f * r, 0.15f * r);
        if (demo_values[i] < -r) demo_values[i] = -r;
        if (demo_values[i] > r) demo_values[i] = r;

        float spread = frand(0.05f * r, 0.2f * r);
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
        lv_obj_set_size(demo_scales[i], 150, 720);
        lv_obj_set_pos(demo_scales[i], 10 + i * 160, 0);
        /* values as passed by Montgolfiere Tab1Page.vue */
        lv_linear_scale_set_confidence_color(demo_scales[i], lv_color_hex(0x4ade80));
        lv_linear_scale_set_confidence_cross(demo_scales[i], 10);
        lv_linear_scale_set_confidence_opa(demo_scales[i], (lv_opa_t)(0.8f * LV_OPA_COVER));
        lv_linear_scale_set_caret_offset_pct(demo_scales[i], i == 0 ? 110 : 105);
        lv_linear_scale_set_scale_line_pct(demo_scales[i], 30);
        lv_linear_scale_set_padding(demo_scales[i], 15);
        lv_linear_scale_set_anim_duration(demo_scales[i], 950);
        lv_linear_scale_set_indicator_size(demo_scales[i], 20);
        demo_values[i] = 0.0f;
    }

    /* instance 1: vAccel-like +-1 domain (Vue defaults / 10) */
    {
        static const float majors[] = {-1, -0.5f, -0.1f, 0, 0.1f, 0.5f, 1};
        static const float weights[] = {0.1f, 0.1f, 0.3f, 0.3f, 0.1f, 0.1f};
        static const float minors[] = {-0.9f, -0.8f, -0.7f, -0.6f, -0.4f, -0.3f, -0.2f,
                                       -0.09f, -0.08f, -0.07f, -0.06f, -0.04f, -0.03f, -0.02f, -0.01f,
                                       0.01f,  0.02f,  0.03f,  0.04f,  0.06f,  0.07f,  0.08f,  0.09f,
                                       0.2f, 0.3f, 0.4f, 0.6f, 0.7f, 0.8f, 0.9f};
        static const float inters[] = {-0.05f, 0.05f};
        lv_linear_scale_set_major_ticks(demo_scales[1], majors, weights, 7);
        lv_linear_scale_set_minor_ticks(demo_scales[1], minors, 30);
        lv_linear_scale_set_intermediate_ticks(demo_scales[1], inters, 2);
    }
    lv_timer_create(demo_tick, 500, NULL);
}
#endif