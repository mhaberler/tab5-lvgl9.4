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

void lv_linear_scale_set_value(lv_obj_t * obj, float value, lv_anim_enable_t anim);
void lv_linear_scale_set_confidence(lv_obj_t * obj, float lower, float upper,
                                    lv_anim_enable_t anim);

#ifdef __cplusplus
}
#endif

#endif /* LV_LINEAR_SCALE_H */
