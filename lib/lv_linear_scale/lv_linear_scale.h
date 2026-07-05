/**
 * @file lv_linear_scale.h
 * Instrument-style linear scale with piecewise-linear (weighted) domain mapping.
 * Port of Montgolfiere LinearScale.vue (D3) to LVGL 9.
 */
#ifndef LV_LINEAR_SCALE_H
#define LV_LINEAR_SCALE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <lvgl.h>
#include <stddef.h>
#include <stdbool.h>

/* Create a linear scale widget with Vue-default config:
 * majorTicks {-10,-5,-1,0,1,5,10}, weights {.1,.1,.3,.3,.1,.1},
 * minor ticks +-0.1..0.9, intermediate +-0.5, vertical orientation. */
lv_obj_t * lv_linear_scale_create(lv_obj_t * parent);

void lv_linear_scale_set_orientation(lv_obj_t * obj, bool vertical);

/* n >= 2 major ticks (sorted ascending), n-1 weights summing to ~1 */
void lv_linear_scale_set_major_ticks(lv_obj_t * obj, const float * ticks,
                                     const float * weights, size_t n);
void lv_linear_scale_set_minor_ticks(lv_obj_t * obj, const float * ticks, size_t n);
void lv_linear_scale_set_intermediate_ticks(lv_obj_t * obj, const float * ticks, size_t n);

void lv_linear_scale_set_indicator_color(lv_obj_t * obj, lv_color_t color);
/* border derived automatically: color darkened ~20% */
void lv_linear_scale_set_confidence_color(lv_obj_t * obj, lv_color_t color);
void lv_linear_scale_set_confidence_cross(lv_obj_t * obj, int32_t px);   /* default 20 */
/* triangle gap from scale line, % of major tick length (20px); default 0 = apex at line */
void lv_linear_scale_set_caret_offset_pct(lv_obj_t * obj, int32_t pct);
void lv_linear_scale_set_scale_line_pct(lv_obj_t * obj, int32_t pct);   /* cross-axis line pos %, default 50 */
void lv_linear_scale_set_padding(lv_obj_t * obj, int32_t px);           /* main-axis end padding, default 50 */
void lv_linear_scale_set_anim_duration(lv_obj_t * obj, uint32_t ms);    /* default 300 */
void lv_linear_scale_set_text_offset(lv_obj_t * obj, int32_t px);       /* label offset past tick, default 15 */
void lv_linear_scale_set_confidence_opa(lv_obj_t * obj, lv_opa_t opa);  /* default LV_OPA_COVER */
void lv_linear_scale_set_indicator_size(lv_obj_t * obj, int32_t px);    /* default 24 */

void lv_linear_scale_set_value(lv_obj_t * obj, float value, lv_anim_enable_t anim);
void lv_linear_scale_set_confidence(lv_obj_t * obj, float lower, float upper,
                                    lv_anim_enable_t anim);

#ifdef __cplusplus
}
#endif

#endif /* LV_LINEAR_SCALE_H */
