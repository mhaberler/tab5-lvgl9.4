# lv_linear_scale — plan history

All five approved plan iterations for the LinearScale.vue → LVGL port, recovered
from the planning session (the plan file `melodic-brewing-peacock.md` was
overwritten each round). Each iteration was driven by on-device photo review.
See `LINEAR_SCALE.md` for the final result.

---

## Iteration 1 — initial port (→ commit b00315a)

### Context
Montgolfiere (Vue/D3) has `LinearScale.vue`: an instrument-style scale with **piecewise-linear domain mapping** (majorTicks + per-segment weights → non-uniform tick spacing, e.g. ±1 region stretched), 3 tick classes (major/intermediate/minor), red triangle value indicator, mint confidence-range box, 0.3s ease-out animation. Goal: equivalent LVGL 9.5 widget on M5Stack Tab5 (ESP32-P4, 1280×720 landscape), two instances demoed with random data. Branch `linear-scale-lvgl` exists for this.

### Decisions (grilled)
- **Custom widget with own draw code** — built-in `lv_scale` can't do weighted segments
- **Construction idiom**: plain `lv_obj` + `LV_EVENT_DRAW_MAIN` callback + state struct in `user_data` (no full `lv_obj_class` registration)
- **Scope**: core set — value, confidenceLower/Upper, majorTicks+weights, minor/intermediate ticks, orientation (both, vertical used in demo), indicator triangle, confidence box, major-tick labels. Colors/sizes as constants matching Vue defaults; style API later
- **Layout**: two vertical scales side-by-side as columns at left edge, full 720px height (~110px wide each)
- **Animation**: 300ms `lv_anim_path_ease_out` on value + confidence bounds
- **Data feed**: `lv_timer` @500ms; per-instance random walk `value += uniform(-1.5,+1.5)` clamped [-10,10]; confidence = value ± random(0.5..2); both instances independent
- **Theme**: white rounded card per widget (Vue `bg-white rounded-xl`), Vue colors 1:1 — `#333` lines, `#ef4444` indicator, `#a7f3d0` confidence
- **Placement**: new `lib/lv_linear_scale/` (C); demo wiring in existing `lib/ui/ui.c`; keep HELLO WORLD/slider demo

### Files

#### New: `lib/lv_linear_scale/lv_linear_scale.h`

```c
lv_obj_t * lv_linear_scale_create(lv_obj_t * parent);
void lv_linear_scale_set_orientation(lv_obj_t*, bool vertical);
void lv_linear_scale_set_major_ticks(lv_obj_t*, const float* ticks, const float* weights, size_t n);  // n>=2, n-1 weights sum ~1
void lv_linear_scale_set_minor_ticks(lv_obj_t*, const float* ticks, size_t n);
void lv_linear_scale_set_intermediate_ticks(lv_obj_t*, const float* ticks, size_t n);
void lv_linear_scale_set_value(lv_obj_t*, float value, lv_anim_enable_t anim);
void lv_linear_scale_set_confidence(lv_obj_t*, float lower, float upper, lv_anim_enable_t anim);
```

Defaults = Vue defaults: majorTicks {-10,-5,-1,0,1,5,10}, weights {.1,.1,.3,.3,.1,.1}, minor ±0.1..0.9 (16), intermediate ±0.5.

#### New: `lib/lv_linear_scale/lv_linear_scale.c` (~350 lines, `#ifdef LVGL_UI` guard like lib/ui)
- State struct (heap via `lv_malloc` → PSRAM after allocator change) in `lv_obj_set_user_data`; freed on `LV_EVENT_DELETE`
- **Scale math** ported from `createCustomScale()` in LinearScale.vue:124-225: cumulative weight → pixel breakpoints, per-segment linear interp; padding 50px each end; vertical = domain min at bottom
- **Draw** in `LV_EVENT_DRAW_MAIN` cb using `lv_draw_line_dsc_t`, `lv_draw_triangle_dsc_t`, `lv_draw_rect_dsc_t`, `lv_draw_label_dsc_t` on the layer:
  1. scale line (#333, 2px) at scaleLinePercent=50% cross-axis
  2. ticks: major 20px/2px/#333 + bold label (montserrat_14, offset 15px), intermediate 10px/1px/#666, minor 5px/1px/#666
  3. confidence box (rounded r=4, #a7f3d0, opa 60%) spanning scale(lower)..scale(upper), 20px cross
  4. indicator triangle (20px, #ef4444) at scale(value), pointing at line (from right when vertical — matches Vue)
- Widget bg: white, radius ~12, via local styles in create()
- **Animation**: `lv_anim` (300ms, ease_out) tweening stored display-value/bounds floats (fixed-point ×100 int since lv_anim is int32), each frame `lv_obj_invalidate`
- Setters clamp/validate like Vue validator (sorted ticks, weights sum ≈1)

#### Modified: `lib/ui/ui.c`
- Create 2 vertical instances: x=10 and x=130, y=0, size ~110×720; defaults unchanged
- `lv_timer_create(demo_tick, 500, NULL)`: per-instance random walk (esp_random or rand), set value + confidence with `LV_ANIM_ON`
- Existing title/slider/label untouched (screen right side)

### Verify
1. `pio run -e m5stack-tab5-p4-lvgl` → clean link (watch RAM stays ~9%, heap in PSRAM)
2. Flash `pio run -e m5stack-tab5-p4-lvgl -t upload`; observe: two white vertical scale cards left edge; non-uniform spacing (−1..0..1 occupies middle 60% of length); red triangles glide smoothly 2×/s; mint confidence boxes track; labels −10,−5,−1,0,1,5,10 bold
3. Touch slider still works (regression check)

---

## Iteration 2 — visibility + demo tuning (photo review 1)

### Context
Iteration 1 (commit b00315a) works on device: two vertical piecewise-linear scales render with ticks, labels, animated red indicator. Photo review found: (1) mint confidence box nearly invisible, (2) random walk lingers at ±10 clamp so indicator sits in compressed outer segments, (3) general polish wanted. All changes in `lib/lv_linear_scale/lv_linear_scale.c` + `lib/ui/ui.c`.

### Changes

#### lib/lv_linear_scale/lv_linear_scale.c
1. **Confidence box visibility**
   - `LS_OPA_CONFIDENCE` → `LV_OPA_COVER` (was 60%)
   - Add 1px border: `dsc.border_color = lv_color_hex(0x34d399); dsc.border_width = 1; dsc.border_opa = LV_OPA_COVER`
   - Enforce minimum main-axis length ~6px: if `(b - a) < 6`, expand symmetrically around center
2. **Indicator**: `LS_INDICATOR_SIZE` 20 → 24
3. **Labels**: font `lv_font_montserrat_14` → `lv_font_montserrat_16`; widen label area boxes accordingly (y ±10 vertical, height 20)

#### lib/ui/ui.c
4. **Random walk mean reversion**: `demo_values[i] += -0.15f * demo_values[i] + frand(-1.5f, 1.5f)` (keep clamp). Indicator now dwells in stretched ±1 region where nonlinearity is visible.
5. **Card padding**: set 8 in widget create(); verify ticks/labels stay inside 110px width; widen card to 120px if labels clip.

### Verify
1. `pio run -e m5stack-tab5-p4-lvgl` clean
2. Flash `pio run -e m5stack-tab5-p4-lvgl -t upload --upload-port /dev/cu.usbmodem213401` (Tab5 port; default port grabs wrong ESP32)
3. Observe: confidence box clearly visible solid mint w/ border even at extremes; indicator oscillates around center dwelling in ±1 region; larger labels/triangle; no clipping at card edges
4. Commit on `linear-scale-lvgl`

---

## Iteration 3 — match Android reference (side-by-side review)

### Context
Side-by-side vs Montgolfiere Android app shows LVGL port structurally correct but: confidence uses pale mint default instead of app's saturated green (app passes props our widget lacks), triangle apex hardcoded at line (Android has caret gap), demo shows two identical ±10 scales while Android pairs vSpeed(±10) with vAccel(±1). Iteration 2 changes are uncommitted+already flashed; commit together with iteration 3.

### Changes

#### lib/lv_linear_scale/lv_linear_scale.h + .c — new API

```c
void lv_linear_scale_set_indicator_color(lv_obj_t*, lv_color_t);
void lv_linear_scale_set_confidence_color(lv_obj_t*, lv_color_t);   /* border derived: darkened ~20% */
void lv_linear_scale_set_confidence_cross(lv_obj_t*, int32_t px);   /* default 20 */
void lv_linear_scale_set_caret_offset_pct(lv_obj_t*, int32_t pct);  /* % of major tick len (20px); default 0 = apex at line */
```

- Move current constants into state struct fields, init to existing defaults in create()
- Caret offset (port of Vue caretDistancePercent): shift whole triangle away from line by `pct/100 * LS_MAJOR_TICK_LEN` (vertical: leftward; horizontal: upward)
- **Label float fix**: `%d` breaks for vAccel ticks (0.5 → "0"). Use stdio `snprintf`: integral values `%d`, else `%g`

#### lib/ui/ui.c — demo differentiation
- Instance 0 (vSpeed-like): defaults, confidence green `0x4ade80`, cross 12, caret 100
- Instance 1 (vAccel-like, Vue defaults ÷10): majors {-1,-.5,-.1,0,.1,.5,1} same weights {.1,.1,.3,.3,.1,.1}, minors ±{.01..09 minus .05}, intermediate ±0.05; same colors; random walk scaled ÷10 (step ±0.15, clamp ±1, spread 0.05..0.2)
- Walk state per instance needs scale factor array

### Verify
1. `pio run -e m5stack-tab5-p4-lvgl` clean
2. Flash: port may re-enumerate — `ls /dev/cu.usbmodem*`, pick P4; kill stale monitors first
3. Observe: left scale ±10, right scale ±1 with fractional labels (0.5, 0.1 rendered correctly); green confidence bars slim (12px); triangles float with gap off line; both animate independently
4. Commit iterations 2+3 on `linear-scale-lvgl`

---

## Iteration 4 — readability (photo review 2)

### Context
Iteration 3 on device: ±10 + ±1 scales, green confidence, caret gap all working. Feedback: lines too thin, labels too small, minor ticks invisible. Root cause of "missing" minors: opaque confidence box drawn after ticks and straddling the line occludes the 5px minors entirely. All changes in `lib/lv_linear_scale/lv_linear_scale.c`.

### Changes (single file)
1. **Z-order**: move confidence-box block before tick drawing in `ls_draw_cb` (box under ticks/line; indicator stays last on top). Minors become visible over green.
2. **Line weights**: main scale line width 2→3; major ticks 2→3; intermediate+minor ticks 1→2 (in `ls_draw_cb` / `ls_draw_tick` call sites).
3. **Caret closer**: demo `lv_linear_scale_set_caret_offset_pct` 100 → 30 (gap 20px → 6px; apex may touch green band edge, box half-cross = 6px) in `lib/ui/ui.c`.
4. **Labels**: `lv_font_montserrat_16` → `lv_font_montserrat_20` (no bold Montserrat compiled; larger size reads bolder). Bump label area: vertical y ±12/height 24; widen cards 130→150 and spacing 140→160 in `lib/ui/ui.c` preemptively (label right-edge fit).

### Verify
1. `pio run -e m5stack-tab5-p4-lvgl` clean; flash (P4 port = /dev/cu.usbmodem*, kill stale monitors first)
2. Observe: minor ticks visible incl. over green bar; thicker lines; larger labels not clipped
3. Commit iterations 2+3+4 together on `linear-scale-lvgl` (→ commit a347c0f)

---

## Iteration 5 — port remaining app-used props (→ commit 5e1e308)

### Context
Review of real usage in Montgolfiere `Tab1Page.vue:103-142` shows the app overrides 5 of the 6 props previously deemed out of scope: scaleLinePercent=30, scalePadding=15, transitionDuration=0.95s, majorTickTextOffset, confidenceOpacity=0.8 (+ indicatorSize=20). Port those as setters; skip indicatorOpacity (app never sets it). Update demo to app's real values. Also commit LINEAR_SCALE.md (untracked) with doc updates.

### Changes

#### lib/lv_linear_scale/lv_linear_scale.h/.c — 6 setters

```c
void lv_linear_scale_set_scale_line_pct(lv_obj_t*, int32_t pct);   /* cross-axis line pos %, default 50 */
void lv_linear_scale_set_padding(lv_obj_t*, int32_t px);           /* main-axis end padding, default 50 */
void lv_linear_scale_set_anim_duration(lv_obj_t*, uint32_t ms);    /* default 300 */
void lv_linear_scale_set_text_offset(lv_obj_t*, int32_t px);       /* label offset past tick, default 15 */
void lv_linear_scale_set_confidence_opa(lv_obj_t*, lv_opa_t);      /* default LV_OPA_COVER */
void lv_linear_scale_set_indicator_size(lv_obj_t*, int32_t px);    /* default 24 */
```

Mechanics: constants `LS_LINE_PCT/LS_PADDING/LS_ANIM_MS/LS_TEXT_OFFSET/LS_OPA_CONFIDENCE/LS_INDICATOR_SIZE` become state-struct fields initialized to current defaults; `ls_scale()` takes padding from state; `ls_draw_cb`/`ls_draw_tick` use fields; `ls_animate` uses `s->anim_ms`. Each setter invalidates.

#### lib/ui/ui.c — demo uses app's real values
line_pct 30, padding 15, anim 950 ms, caret 110 (inst 0) / 105 (inst 1), confidence opa 204 (0.8), cross 10, indicator size 20. Geometry check: line@30% of 150 = 45px; labels start 45+20+15=80, +42px label ≤ 150 — fits.

#### Docs
Update LINEAR_SCALE.md: API list + remove ported items from "Not ported" (leaves only indicatorOpacity + resize note). Commit doc + code together.

### Verify
1. `pio run -e m5stack-tab5-p4-lvgl` clean
2. Flash (check `ls /dev/cu.usbmodem*` — port re-enumerates; no serial monitor)
3. Observe: scale line shifted left (30%), longer scale (15px padding), slower 950ms glide, labels/caret per app
4. Commit on `linear-scale-lvgl`

---

## Post-plan feedback round (photo review 3, part of 5e1e308)

Not a formal plan — direct fixes from photo:
- Minor ticks absent in outer segments (1–5, 5–10 and ±0.1–1): Vue defaults only define minors inside ±1; extended default minors to `±2..4, ±6..9` and the ±1 demo instance to `±0.2..0.9`
- Minor ticks lengthened 5→9px, intermediate 10→14px, both emboldened 2→3px
