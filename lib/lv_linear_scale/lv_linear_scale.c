/**
 * @file lv_linear_scale.c
 * Instrument-style linear scale with piecewise-linear (weighted) domain mapping.
 * Port of Montgolfiere LinearScale.vue (D3) to LVGL 9.
 */
#ifdef LVGL_UI

#include "lv_linear_scale.h"
#include <stdio.h>
#include <string.h>

/*********************
 *      DEFINES
 *********************/
#define LS_MAX_MAJOR 16
#define LS_MAX_TICKS 32

/* Geometry constants (Vue defaults) */
#define LS_PADDING            50  /* scalePadding */
#define LS_MAJOR_TICK_LEN     20  /* MAJOR_TICK_LENGTH */
#define LS_INTER_TICK_LEN     10  /* INTERMEDIATE_TICK_LENGTH */
#define LS_MINOR_TICK_LEN      5  /* MINOR_TICK_LENGTH */
#define LS_INDICATOR_SIZE     24  /* indicatorSize */
#define LS_CONF_CROSS         20  /* confidenceBoxCrossDimension */
#define LS_TEXT_OFFSET        15  /* majorTickTextOffset */
#define LS_LINE_PCT           50  /* scaleLinePercent */
#define LS_ANIM_MS           300  /* transitionDuration 0.3s */

/* Colors (Vue defaults) */
#define LS_COLOR_LINE       lv_color_hex(0x333333)
#define LS_COLOR_TICK       lv_color_hex(0x666666)
#define LS_COLOR_TEXT       lv_color_hex(0x333333)
#define LS_COLOR_INDICATOR  lv_color_hex(0xef4444)
#define LS_COLOR_CONFIDENCE lv_color_hex(0xa7f3d0)
#define LS_OPA_CONFIDENCE   LV_OPA_COVER
#define LS_COLOR_CONF_BORDER lv_color_hex(0x34d399)
#define LS_CONF_MIN_LEN       6  /* min main-axis px so box stays visible */

/**********************
 *      TYPEDEFS
 **********************/
typedef struct {
    bool vertical;

    float major[LS_MAX_MAJOR];
    float weights[LS_MAX_MAJOR - 1];
    size_t n_major;

    float minor[LS_MAX_TICKS];
    size_t n_minor;

    float inter[LS_MAX_TICKS];
    size_t n_inter;

    /* displayed (possibly mid-animation) state, fixed point x100 */
    int32_t value_x100;
    int32_t conf_lo_x100;
    int32_t conf_hi_x100;

    /* style */
    lv_color_t indicator_color;
    lv_color_t conf_color;
    lv_color_t conf_border_color;
    int32_t conf_cross;      /* confidence box cross-axis px */
    int32_t caret_pct;       /* triangle gap from line, % of major tick len */

    lv_obj_t * obj;
} lv_linear_scale_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void ls_draw_cb(lv_event_t * e);
static void ls_delete_cb(lv_event_t * e);

/**********************
 *  DEFAULT CONFIG (Vue defaults)
 **********************/
static const float def_major[]   = {-10, -5, -1, 0, 1, 5, 10};
static const float def_weights[] = {0.1f, 0.1f, 0.3f, 0.3f, 0.1f, 0.1f};
static const float def_minor[]   = {-0.9f, -0.8f, -0.7f, -0.6f, -0.4f, -0.3f, -0.2f, -0.1f,
                                    0.1f,  0.2f,  0.3f,  0.4f,  0.6f,  0.7f,  0.8f,  0.9f};
static const float def_inter[]   = {-0.5f, 0.5f};

/**********************
 *  SCALE MATH (port of createCustomScale, LinearScale.vue)
 **********************/

/* Map domain value -> pixel offset along main axis (0 .. length-1 local coords).
 * Vertical: domain min at bottom (large px), max at top (small px). */
static float ls_scale(const lv_linear_scale_t * s, float v, int32_t length)
{
    float start_px, dir;
    float effective = (float)(length - 2 * LS_PADDING);
    if(s->vertical) {
        start_px = (float)(length - LS_PADDING); /* bottom */
        dir = -1.0f;
    }
    else {
        start_px = (float)LS_PADDING;
        dir = 1.0f;
    }

    /* cumulative breakpoints */
    float bp[LS_MAX_MAJOR];
    bp[0] = start_px;
    for(size_t i = 0; i + 1 < s->n_major; i++) {
        bp[i + 1] = bp[i] + dir * s->weights[i] * effective;
    }

    /* clamp to domain ends */
    if(v <= s->major[0]) return bp[0];
    if(v >= s->major[s->n_major - 1]) return bp[s->n_major - 1];

    /* per-segment linear interpolation */
    for(size_t i = 0; i + 1 < s->n_major; i++) {
        if(v >= s->major[i] && v <= s->major[i + 1]) {
            float t = (v - s->major[i]) / (s->major[i + 1] - s->major[i]);
            return bp[i] + t * (bp[i + 1] - bp[i]);
        }
    }
    return bp[0]; /* unreachable with sorted ticks */
}

/**********************
 *  DRAWING
 **********************/

static void ls_draw_tick(lv_layer_t * layer, const lv_linear_scale_t * s,
                         lv_area_t * coords, float v, int32_t tick_len,
                         lv_color_t color, int32_t width, bool label)
{
    int32_t w = lv_area_get_width(coords);
    int32_t h = lv_area_get_height(coords);
    int32_t length = s->vertical ? h : w;
    int32_t cross = s->vertical ? w : h;
    float pos = ls_scale(s, v, length);
    float line_pos = (float)cross * LS_LINE_PCT / 100.0f;

    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = color;
    line_dsc.width = width;
    line_dsc.opa = LV_OPA_COVER;

    if(s->vertical) {
        line_dsc.p1.x = (lv_value_precise_t)(coords->x1 + line_pos);
        line_dsc.p1.y = (lv_value_precise_t)(coords->y1 + pos);
        line_dsc.p2.x = (lv_value_precise_t)(coords->x1 + line_pos + tick_len);
        line_dsc.p2.y = line_dsc.p1.y;
    }
    else {
        line_dsc.p1.x = (lv_value_precise_t)(coords->x1 + pos);
        line_dsc.p1.y = (lv_value_precise_t)(coords->y1 + line_pos);
        line_dsc.p2.x = line_dsc.p1.x;
        line_dsc.p2.y = (lv_value_precise_t)(coords->y1 + line_pos + tick_len);
    }
    lv_draw_line(layer, &line_dsc);

    if(!label) return;

    char buf[16];
    if(v == (float)(int)v) snprintf(buf, sizeof(buf), "%d", (int)v);
    else snprintf(buf, sizeof(buf), "%g", (double)v);

    lv_draw_label_dsc_t lbl_dsc;
    lv_draw_label_dsc_init(&lbl_dsc);
    lbl_dsc.text = buf;
    lbl_dsc.text_local = 1; /* copy: buf is stack local */
    lbl_dsc.color = LS_COLOR_TEXT;
    lbl_dsc.font = &lv_font_montserrat_20;

    lv_area_t txt_area;
    if(s->vertical) {
        /* text right of tick, vertically centered on tick */
        txt_area.x1 = coords->x1 + (int32_t)line_pos + tick_len + LS_TEXT_OFFSET;
        txt_area.x2 = txt_area.x1 + 60;
        txt_area.y1 = coords->y1 + (int32_t)pos - 12;
        txt_area.y2 = txt_area.y1 + 24;
        lbl_dsc.align = LV_TEXT_ALIGN_LEFT;
    }
    else {
        /* text below tick, horizontally centered */
        txt_area.x1 = coords->x1 + (int32_t)pos - 25;
        txt_area.x2 = txt_area.x1 + 50;
        txt_area.y1 = coords->y1 + (int32_t)line_pos + tick_len + LS_TEXT_OFFSET;
        txt_area.y2 = txt_area.y1 + 24;
        lbl_dsc.align = LV_TEXT_ALIGN_CENTER;
    }
    lv_draw_label(layer, &lbl_dsc, &txt_area);
}

static void ls_draw_cb(lv_event_t * e)
{
    lv_obj_t * obj = (lv_obj_t *)lv_event_get_target(e);
    lv_linear_scale_t * s = (lv_linear_scale_t *)lv_obj_get_user_data(obj);
    lv_layer_t * layer = lv_event_get_layer(e);
    if(s == NULL || layer == NULL) return;

    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);
    int32_t w = lv_area_get_width(&coords);
    int32_t h = lv_area_get_height(&coords);
    int32_t length = s->vertical ? h : w;
    int32_t cross = s->vertical ? w : h;
    float line_pos = (float)cross * LS_LINE_PCT / 100.0f;

    /* 1. main scale line */
    {
        float p_start = ls_scale(s, s->major[0], length);
        float p_end = ls_scale(s, s->major[s->n_major - 1], length);
        lv_draw_line_dsc_t dsc;
        lv_draw_line_dsc_init(&dsc);
        dsc.color = LS_COLOR_LINE;
        dsc.width = 3;
        dsc.opa = LV_OPA_COVER;
        if(s->vertical) {
            dsc.p1.x = (lv_value_precise_t)(coords.x1 + line_pos);
            dsc.p1.y = (lv_value_precise_t)(coords.y1 + p_start);
            dsc.p2.x = dsc.p1.x;
            dsc.p2.y = (lv_value_precise_t)(coords.y1 + p_end);
        }
        else {
            dsc.p1.x = (lv_value_precise_t)(coords.x1 + p_start);
            dsc.p1.y = (lv_value_precise_t)(coords.y1 + line_pos);
            dsc.p2.x = (lv_value_precise_t)(coords.x1 + p_end);
            dsc.p2.y = dsc.p1.y;
        }
        lv_draw_line(layer, &dsc);
    }

    /* 2. confidence box (under ticks so minors stay visible) */
    {
        float lo = ls_scale(s, (float)s->conf_lo_x100 / 100.0f, length);
        float hi = ls_scale(s, (float)s->conf_hi_x100 / 100.0f, length);
        float a = LV_MIN(lo, hi), b = LV_MAX(lo, hi);
        if(b - a < LS_CONF_MIN_LEN) { /* keep visible in compressed segments */
            float c = (a + b) / 2.0f;
            a = c - LS_CONF_MIN_LEN / 2.0f;
            b = c + LS_CONF_MIN_LEN / 2.0f;
        }

        lv_draw_rect_dsc_t dsc;
        lv_draw_rect_dsc_init(&dsc);
        dsc.bg_color = s->conf_color;
        dsc.bg_opa = LS_OPA_CONFIDENCE;
        dsc.radius = 4;
        dsc.border_color = s->conf_border_color;
        dsc.border_width = 1;
        dsc.border_opa = LV_OPA_COVER;

        lv_area_t box;
        if(s->vertical) {
            box.x1 = coords.x1 + (int32_t)(line_pos - s->conf_cross / 2);
            box.x2 = box.x1 + s->conf_cross;
            box.y1 = coords.y1 + (int32_t)a;
            box.y2 = coords.y1 + (int32_t)b;
        }
        else {
            box.x1 = coords.x1 + (int32_t)a;
            box.x2 = coords.x1 + (int32_t)b;
            box.y1 = coords.y1 + (int32_t)(line_pos - s->conf_cross / 2);
            box.y2 = box.y1 + s->conf_cross;
        }
        lv_draw_rect(layer, &dsc, &box);
    }

    /* 3. ticks over the box: minor, intermediate, major (major last: thicker + label) */
    for(size_t i = 0; i < s->n_minor; i++)
        ls_draw_tick(layer, s, &coords, s->minor[i], LS_MINOR_TICK_LEN, LS_COLOR_TICK, 2, false);
    for(size_t i = 0; i < s->n_inter; i++)
        ls_draw_tick(layer, s, &coords, s->inter[i], LS_INTER_TICK_LEN, LS_COLOR_TICK, 2, false);
    for(size_t i = 0; i < s->n_major; i++)
        ls_draw_tick(layer, s, &coords, s->major[i], LS_MAJOR_TICK_LEN, LS_COLOR_LINE, 3, true);

    /* 4. indicator triangle, apex offset from the scale line */
    {
        float pos = ls_scale(s, (float)s->value_x100 / 100.0f, length);
        float caret_gap = (float)s->caret_pct / 100.0f * LS_MAJOR_TICK_LEN;

        lv_draw_triangle_dsc_t dsc;
        lv_draw_triangle_dsc_init(&dsc);
        dsc.color = s->indicator_color;
        dsc.opa = LV_OPA_COVER;

        if(s->vertical) {
            /* base left of the line, apex pointing right, gap before line */
            float apex_x = coords.x1 + line_pos - caret_gap;
            float base_x = apex_x - LS_INDICATOR_SIZE;
            float cy = coords.y1 + pos;
            dsc.p[0].x = (lv_value_precise_t)apex_x;
            dsc.p[0].y = (lv_value_precise_t)cy;
            dsc.p[1].x = (lv_value_precise_t)base_x;
            dsc.p[1].y = (lv_value_precise_t)(cy - LS_INDICATOR_SIZE / 2);
            dsc.p[2].x = (lv_value_precise_t)base_x;
            dsc.p[2].y = (lv_value_precise_t)(cy + LS_INDICATOR_SIZE / 2);
        }
        else {
            /* base above the line, apex pointing down, gap before line */
            float apex_y = coords.y1 + line_pos - caret_gap;
            float base_y = apex_y - LS_INDICATOR_SIZE;
            float cx = coords.x1 + pos;
            dsc.p[0].x = (lv_value_precise_t)cx;
            dsc.p[0].y = (lv_value_precise_t)apex_y;
            dsc.p[1].x = (lv_value_precise_t)(cx - LS_INDICATOR_SIZE / 2);
            dsc.p[1].y = (lv_value_precise_t)base_y;
            dsc.p[2].x = (lv_value_precise_t)(cx + LS_INDICATOR_SIZE / 2);
            dsc.p[2].y = (lv_value_precise_t)base_y;
        }
        lv_draw_triangle(layer, &dsc);
    }
}

/**********************
 *  ANIMATION
 **********************/

static void ls_anim_value_cb(void * var, int32_t v)
{
    lv_linear_scale_t * s = (lv_linear_scale_t *)var;
    s->value_x100 = v;
    lv_obj_invalidate(s->obj);
}

static void ls_anim_lo_cb(void * var, int32_t v)
{
    lv_linear_scale_t * s = (lv_linear_scale_t *)var;
    s->conf_lo_x100 = v;
    lv_obj_invalidate(s->obj);
}

static void ls_anim_hi_cb(void * var, int32_t v)
{
    lv_linear_scale_t * s = (lv_linear_scale_t *)var;
    s->conf_hi_x100 = v;
    lv_obj_invalidate(s->obj);
}

static void ls_animate(lv_linear_scale_t * s, lv_anim_exec_xcb_t exec_cb,
                       int32_t from, int32_t to)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s);
    lv_anim_set_exec_cb(&a, exec_cb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, LS_ANIM_MS);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a); /* replaces running anim with same var+exec_cb */
}

/**********************
 *  API
 **********************/

lv_obj_t * lv_linear_scale_create(lv_obj_t * parent)
{
    lv_obj_t * obj = lv_obj_create(parent);

    lv_linear_scale_t * s = (lv_linear_scale_t *)lv_malloc(sizeof(lv_linear_scale_t));
    LV_ASSERT_MALLOC(s);
    lv_memzero(s, sizeof(*s));

    s->obj = obj;
    s->vertical = true;
    memcpy(s->major, def_major, sizeof(def_major));
    memcpy(s->weights, def_weights, sizeof(def_weights));
    s->n_major = sizeof(def_major) / sizeof(def_major[0]);
    memcpy(s->minor, def_minor, sizeof(def_minor));
    s->n_minor = sizeof(def_minor) / sizeof(def_minor[0]);
    memcpy(s->inter, def_inter, sizeof(def_inter));
    s->n_inter = sizeof(def_inter) / sizeof(def_inter[0]);
    s->value_x100 = 0;
    s->conf_lo_x100 = -100;
    s->conf_hi_x100 = 100;
    s->indicator_color = LS_COLOR_INDICATOR;
    s->conf_color = LS_COLOR_CONFIDENCE;
    s->conf_border_color = LS_COLOR_CONF_BORDER;
    s->conf_cross = LS_CONF_CROSS;
    s->caret_pct = 0;

    lv_obj_set_user_data(obj, s);

    /* white rounded card (Vue: bg-white rounded-xl) */
    lv_obj_set_style_bg_color(obj, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(obj, 12, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 8, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_add_event_cb(obj, ls_draw_cb, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_event_cb(obj, ls_delete_cb, LV_EVENT_DELETE, NULL);

    return obj;
}

static void ls_delete_cb(lv_event_t * e)
{
    lv_obj_t * obj = (lv_obj_t *)lv_event_get_target(e);
    lv_linear_scale_t * s = (lv_linear_scale_t *)lv_obj_get_user_data(obj);
    if(s) {
        lv_anim_delete(s, NULL);
        lv_free(s);
        lv_obj_set_user_data(obj, NULL);
    }
}

void lv_linear_scale_set_orientation(lv_obj_t * obj, bool vertical)
{
    lv_linear_scale_t * s = (lv_linear_scale_t *)lv_obj_get_user_data(obj);
    if(!s) return;
    s->vertical = vertical;
    lv_obj_invalidate(obj);
}

void lv_linear_scale_set_major_ticks(lv_obj_t * obj, const float * ticks,
                                     const float * weights, size_t n)
{
    lv_linear_scale_t * s = (lv_linear_scale_t *)lv_obj_get_user_data(obj);
    if(!s || n < 2 || n > LS_MAX_MAJOR) return;

    float sum = 0;
    for(size_t i = 0; i + 1 < n; i++) sum += weights[i];
    for(size_t i = 0; i + 1 < n; i++) {
        if(ticks[i] > ticks[i + 1]) return; /* must be sorted */
    }
    if(sum < 0.999f || sum > 1.001f) return; /* weights must sum to ~1 */

    memcpy(s->major, ticks, n * sizeof(float));
    memcpy(s->weights, weights, (n - 1) * sizeof(float));
    s->n_major = n;
    lv_obj_invalidate(obj);
}

void lv_linear_scale_set_minor_ticks(lv_obj_t * obj, const float * ticks, size_t n)
{
    lv_linear_scale_t * s = (lv_linear_scale_t *)lv_obj_get_user_data(obj);
    if(!s || n > LS_MAX_TICKS) return;
    memcpy(s->minor, ticks, n * sizeof(float));
    s->n_minor = n;
    lv_obj_invalidate(obj);
}

void lv_linear_scale_set_intermediate_ticks(lv_obj_t * obj, const float * ticks, size_t n)
{
    lv_linear_scale_t * s = (lv_linear_scale_t *)lv_obj_get_user_data(obj);
    if(!s || n > LS_MAX_TICKS) return;
    memcpy(s->inter, ticks, n * sizeof(float));
    s->n_inter = n;
    lv_obj_invalidate(obj);
}

void lv_linear_scale_set_indicator_color(lv_obj_t * obj, lv_color_t color)
{
    lv_linear_scale_t * s = (lv_linear_scale_t *)lv_obj_get_user_data(obj);
    if(!s) return;
    s->indicator_color = color;
    lv_obj_invalidate(obj);
}

void lv_linear_scale_set_confidence_color(lv_obj_t * obj, lv_color_t color)
{
    lv_linear_scale_t * s = (lv_linear_scale_t *)lv_obj_get_user_data(obj);
    if(!s) return;
    s->conf_color = color;
    s->conf_border_color = lv_color_darken(color, LV_OPA_20);
    lv_obj_invalidate(obj);
}

void lv_linear_scale_set_confidence_cross(lv_obj_t * obj, int32_t px)
{
    lv_linear_scale_t * s = (lv_linear_scale_t *)lv_obj_get_user_data(obj);
    if(!s) return;
    s->conf_cross = px;
    lv_obj_invalidate(obj);
}

void lv_linear_scale_set_caret_offset_pct(lv_obj_t * obj, int32_t pct)
{
    lv_linear_scale_t * s = (lv_linear_scale_t *)lv_obj_get_user_data(obj);
    if(!s) return;
    s->caret_pct = pct;
    lv_obj_invalidate(obj);
}

void lv_linear_scale_set_value(lv_obj_t * obj, float value, lv_anim_enable_t anim)
{
    lv_linear_scale_t * s = (lv_linear_scale_t *)lv_obj_get_user_data(obj);
    if(!s) return;
    int32_t target = (int32_t)(value * 100.0f);
    if(anim == LV_ANIM_ON) {
        ls_animate(s, ls_anim_value_cb, s->value_x100, target);
    }
    else {
        s->value_x100 = target;
        lv_obj_invalidate(obj);
    }
}

void lv_linear_scale_set_confidence(lv_obj_t * obj, float lower, float upper,
                                    lv_anim_enable_t anim)
{
    lv_linear_scale_t * s = (lv_linear_scale_t *)lv_obj_get_user_data(obj);
    if(!s) return;
    int32_t lo = (int32_t)(lower * 100.0f);
    int32_t hi = (int32_t)(upper * 100.0f);
    if(anim == LV_ANIM_ON) {
        ls_animate(s, ls_anim_lo_cb, s->conf_lo_x100, lo);
        ls_animate(s, ls_anim_hi_cb, s->conf_hi_x100, hi);
    }
    else {
        s->conf_lo_x100 = lo;
        s->conf_hi_x100 = hi;
        lv_obj_invalidate(obj);
    }
}

#endif /* LVGL_UI */
