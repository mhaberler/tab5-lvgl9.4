# lv_linear_scale — LVGL port of Montgolfiere LinearScale.vue

Instrument-style scale widget with **piecewise-linear (weighted) domain mapping**
for LVGL 9.x. Port of `Montgolfiere/src/components/LinearScale.vue` (Vue 3 + D3)
to C, developed and verified on M5Stack Tab5 (ESP32-P4, 1280×720).

## Files

| File | Purpose |
|------|---------|
| `lib/lv_linear_scale/lv_linear_scale.h` | Public API |
| `lib/lv_linear_scale/lv_linear_scale.c` | Widget implementation (`#ifdef LVGL_UI`) |
| `lib/ui/ui.c` | Demo: two instances + 2 Hz random-walk driver |
| `src/lv_mem_core_psram.c` | LVGL `LV_STDLIB_CUSTOM` allocator → PSRAM (prerequisite: 256 KB LVGL heap overflowed internal DRAM) |
| `micropython/lv_linear_scale_sim.py` | MicroPython port for the [LVGL web simulator](https://sim.lvgl.io) (v9.0 MicroPython): paste file into editor, Restart. Composition-based (child rects/labels, `SYMBOL.PLAY` indicator) instead of custom draw; same piecewise math, same two-instance demo |

## Key concept: weighted segments

Major ticks partition the domain; per-segment weights set each segment's share of
the pixel length (weights sum to 1). Default: majors
`{-10,-5,-1,0,1,5,10}`, weights `{.1,.1,.3,.3,.1,.1}` → the ±1 region occupies 60%
of the scale. Default minors cover all segments (`±2..4`, `±6..9`, `±0.1..0.9`),
extending the Vue defaults which only had minors inside ±1. Mapping is per-segment linear interpolation over cumulative
weight breakpoints (port of `createCustomScale()` in the Vue source); values
outside the domain clamp to the ends.

## Rendering

Plain `lv_obj` + `LV_EVENT_DRAW_MAIN` callback, state struct in `user_data`
(freed on `LV_EVENT_DELETE`). No `lv_obj_class` registration. Draw order:

1. confidence box (under everything so minor ticks stay visible)
2. main scale line (3px, #333)
3. ticks: minor 9px/3px, intermediate 14px/3px, major 20px/3px + label
   (`montserrat_20`, `%g` formatting for fractional ticks)
4. indicator triangle (24px, apex toward line, optional caret gap)

Widget background: white rounded card (Vue `bg-white rounded-xl`).
Value/confidence updates animate 300 ms `lv_anim_path_ease_out`
(fixed-point ×100, since `lv_anim` is int32).

## API

```c
lv_obj_t * lv_linear_scale_create(lv_obj_t * parent);          /* Vue defaults */
void lv_linear_scale_set_orientation(lv_obj_t*, bool vertical); /* default true */
void lv_linear_scale_set_major_ticks(lv_obj_t*, const float * ticks,
                                     const float * weights, size_t n);
void lv_linear_scale_set_minor_ticks(lv_obj_t*, const float*, size_t);
void lv_linear_scale_set_intermediate_ticks(lv_obj_t*, const float*, size_t);
void lv_linear_scale_set_indicator_color(lv_obj_t*, lv_color_t);
void lv_linear_scale_set_confidence_color(lv_obj_t*, lv_color_t); /* border auto-darkened 20% */
void lv_linear_scale_set_confidence_cross(lv_obj_t*, int32_t px);  /* default 20 */
void lv_linear_scale_set_caret_offset_pct(lv_obj_t*, int32_t pct); /* % of 20px tick; default 0 */
void lv_linear_scale_set_confidence_opa(lv_obj_t*, lv_opa_t);      /* default LV_OPA_COVER */
void lv_linear_scale_set_scale_line_pct(lv_obj_t*, int32_t pct);   /* line pos on cross axis, default 50 */
void lv_linear_scale_set_padding(lv_obj_t*, int32_t px);           /* main-axis end padding, default 50 */
void lv_linear_scale_set_anim_duration(lv_obj_t*, uint32_t ms);    /* default 300 */
void lv_linear_scale_set_text_offset(lv_obj_t*, int32_t px);       /* label offset past tick, default 15 */
void lv_linear_scale_set_indicator_size(lv_obj_t*, int32_t px);    /* default 24 */
void lv_linear_scale_set_value(lv_obj_t*, float, lv_anim_enable_t);
void lv_linear_scale_set_confidence(lv_obj_t*, float lo, float hi, lv_anim_enable_t);
```

Setter validation mirrors Vue: majors sorted, n≥2, weights sum ≈1, else ignored.
Confidence box enforces 6px minimum length so it stays visible in compressed
outer segments.

## Demo (lib/ui/ui.c)

Two vertical 150×720 instances at left screen edge, mirroring the Android app's
vSpeed/vAccel pair:

- instance 0: default ±10 domain, caret 110%
- instance 1: ±1 domain (defaults ÷10: majors `{-1,-.5,-.1,0,.1,.5,1}`, fine minors), caret 105%
- both, matching the app's actual props: confidence green `#4ade80` cross 10px
  opa 0.8, scale line at 30%, padding 15px, anim 950 ms, indicator 20px
- `lv_timer` @500 ms: mean-reverting random walk
  `v += -0.15·v + U(-0.15r, 0.15r)`, confidence = v ± U(0.05r, 0.2r) —
  reversion keeps the needle in the stretched center region

## Build / flash

```sh
pio run -e m5stack-tab5-p4-lvgl
pio run -e m5stack-tab5-p4-lvgl -t upload --upload-port /dev/cu.usbmodemXXXX
```

Gotchas hit during development:

- **DRAM overflow**: LVGL's builtin 256 KB heap in `.bss` broke the P4 link
  (`--enable-non-contiguous-regions discards section`). Fixed by
  `LV_USE_STDLIB_MALLOC = LV_STDLIB_CUSTOM` + PSRAM allocator
  (`src/lv_mem_core_psram.c`, `include/lv_conf.h`).
- **Upload port**: default esptool port grabs a different ESP32 on this machine;
  P4 enumerates as `/dev/cu.usbmodem…` and *re-enumerates after each reboot* —
  check `ls /dev/cu.usbmodem*` before flashing.
- No bold Montserrat compiled in `lv_conf.h`; use a larger size instead.

## Iteration history (branch `linear-scale-lvgl`)

1. `b00315a` — initial port: widget + demo, verified rendering on device
2. `a347c0f` — on-device review pass vs Android reference: style setters,
   caret offset API, float label fix (`%d` → `%g`), confidence box under ticks
   (opaque box occluded minors), bolder lines, larger labels, differentiated
   demo instances, mean-reverting walk

## Not ported (Vue props out of scope)

`indicatorOpacity` (the app never overrides it), resize observer (LVGL redraws
from current size each frame — resize just works).
