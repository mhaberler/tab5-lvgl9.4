# lv_linear_scale_sim.py — MicroPython/LVGL 9 port of lib/lv_linear_scale
# Paste into the LVGL online simulator: https://sim.lvgl.io (v9.0 MicroPython).
#
# Instrument-style scale with piecewise-linear (weighted) domain mapping.
# Composition-based (child objects), not custom draw: ticks are thin rects,
# labels are lv.label, indicator is the SYMBOL.PLAY triangle glyph.
# Reference C implementation: lib/lv_linear_scale/lv_linear_scale.c

import display_driver  # noqa: F401  (sim.lvgl.io display init)
import lvgl as lv

try:
    import random

    def _rand():  # 0.0 .. 1.0
        return random.getrandbits(16) / 65535.0
except ImportError:  # minimal LCG fallback
    _seed = [12345]

    def _rand():
        _seed[0] = (1103515245 * _seed[0] + 12345) & 0x7FFFFFFF
        return _seed[0] / 0x7FFFFFFF


def _frand(lo, hi):
    return lo + (hi - lo) * _rand()


def _fmt(v):
    return str(int(v)) if v == int(v) else ("%g" % v)


def _darken(rgb, factor=0.8):
    r = int(((rgb >> 16) & 0xFF) * factor)
    g = int(((rgb >> 8) & 0xFF) * factor)
    b = int((rgb & 0xFF) * factor)
    return (r << 16) | (g << 8) | b


# full-range default minors, as in the C widget
_DEF_MINORS = tuple([-9, -8, -7, -6, -4, -3, -2] +
                    [x / 10 for x in range(-9, 0) if x != -5] +
                    [x / 10 for x in range(1, 10) if x != 5] +
                    [2, 3, 4, 6, 7, 8, 9])

_MAJOR_LEN, _INTER_LEN, _MINOR_LEN = 20, 14, 9
_TICK_W = 3
_CONF_MIN_LEN = 6


class LinearScale:
    def __init__(self, parent, vertical=True,
                 major_ticks=(-10, -5, -1, 0, 1, 5, 10),
                 weights=(.1, .1, .3, .3, .1, .1),
                 minor_ticks=_DEF_MINORS,
                 inter_ticks=(-0.5, 0.5),
                 indicator_color=0xEF4444,
                 confidence_color=0xA7F3D0,
                 confidence_cross=20, confidence_opa=255,
                 caret_pct=0, line_pct=50, padding=50, text_offset=15,
                 indicator_size=20,  # parity kwarg; glyph size fixed by font
                 anim_ms=300, width=110, height=300, x=0, y=0):
        assert len(major_ticks) >= 2 and len(weights) == len(major_ticks) - 1
        self.vertical = vertical
        self.major = sorted(major_ticks)
        self.weights = weights
        self.caret_pct = caret_pct
        self.line_pct = line_pct
        self.padding = padding
        self.conf_cross = confidence_cross
        self.anim_ms = anim_ms
        self.w, self.h = width, height

        self._value = 0.0
        self._lo, self._hi = -1.0, 1.0

        # white rounded card
        card = lv.obj(parent)
        card.set_size(width, height)
        card.set_pos(x, y)
        card.set_style_bg_color(lv.color_hex(0xFFFFFF), 0)
        card.set_style_radius(12, 0)
        card.set_style_border_width(0, 0)
        card.set_style_pad_all(0, 0)
        card.remove_flag(lv.obj.FLAG.SCROLLABLE)
        self.card = card

        length = height if vertical else width
        cross = width if vertical else height
        self._length = length
        self._line_pos = cross * line_pct // 100

        # confidence box first: below ticks in z-order
        conf = lv.obj(card)
        conf.set_style_bg_color(lv.color_hex(confidence_color), 0)
        conf.set_style_bg_opa(confidence_opa, 0)
        conf.set_style_radius(4, 0)
        conf.set_style_border_width(1, 0)
        conf.set_style_border_color(lv.color_hex(_darken(confidence_color)), 0)
        conf.remove_flag(lv.obj.FLAG.SCROLLABLE)
        self.conf = conf

        # main scale line
        p0, p1 = self._scale(self.major[0]), self._scale(self.major[-1])
        a, b = min(p0, p1), max(p0, p1)
        line = self._rect(0x333333)
        if vertical:
            line.set_pos(self._line_pos - 1, a)
            line.set_size(3, b - a + 1)
        else:
            line.set_pos(a, self._line_pos - 1)
            line.set_size(b - a + 1, 3)

        # ticks + labels
        for v in minor_ticks:
            self._tick(v, _MINOR_LEN, 0x666666)
        for v in inter_ticks:
            self._tick(v, _INTER_LEN, 0x666666)
        for v in self.major:
            self._tick(v, _MAJOR_LEN, 0x333333)
            lbl = lv.label(card)
            lbl.set_text(_fmt(v))
            lbl.set_style_text_color(lv.color_hex(0x333333), 0)
            pos = self._scale(v)
            if vertical:
                lbl.set_pos(self._line_pos + _MAJOR_LEN + text_offset, pos - 8)
            else:
                lbl.set_pos(pos - 10, self._line_pos + _MAJOR_LEN + text_offset)

        # indicator: right/down-pointing triangle glyph, apex toward line
        ind = lv.label(card)
        ind.set_text(lv.SYMBOL.PLAY)
        ind.set_style_text_color(lv.color_hex(indicator_color), 0)
        self.ind = ind
        self._glyph = 16  # approx symbol extent at default font

        self._update()

    # --- piecewise scale: port of ls_scale() ---
    def _scale(self, v):
        eff = self._length - 2 * self.padding
        if self.vertical:
            start, dirn = self._length - self.padding, -1.0
        else:
            start, dirn = self.padding, 1.0
        bp = [start]
        for wgt in self.weights:
            bp.append(bp[-1] + dirn * wgt * eff)
        if v <= self.major[0]:
            return int(bp[0])
        if v >= self.major[-1]:
            return int(bp[-1])
        for i in range(len(self.major) - 1):
            m0, m1 = self.major[i], self.major[i + 1]
            if m0 <= v <= m1:
                t = (v - m0) / (m1 - m0)
                return int(bp[i] + t * (bp[i + 1] - bp[i]))
        return int(bp[0])

    def _rect(self, rgb):
        r = lv.obj(self.card)
        r.set_style_bg_color(lv.color_hex(rgb), 0)
        r.set_style_radius(0, 0)
        r.set_style_border_width(0, 0)
        r.remove_flag(lv.obj.FLAG.SCROLLABLE)
        return r

    def _tick(self, v, tick_len, rgb):
        t = self._rect(rgb)
        pos = self._scale(v)
        if self.vertical:
            t.set_pos(self._line_pos, pos - _TICK_W // 2)
            t.set_size(tick_len, _TICK_W)
        else:
            t.set_pos(pos - _TICK_W // 2, self._line_pos)
            t.set_size(_TICK_W, tick_len)

    # --- dynamic parts ---
    def _update(self):
        pos = self._scale(self._value)
        gap = self.caret_pct * _MAJOR_LEN // 100
        if self.vertical:
            self.ind.set_pos(self._line_pos - gap - self._glyph, pos - self._glyph // 2)
        else:
            self.ind.set_pos(pos - self._glyph // 2, self._line_pos - gap - self._glyph)

        a = self._scale(self._lo)
        b = self._scale(self._hi)
        a, b = min(a, b), max(a, b)
        if b - a < _CONF_MIN_LEN:
            c = (a + b) // 2
            a, b = c - _CONF_MIN_LEN // 2, c + _CONF_MIN_LEN // 2
        if self.vertical:
            self.conf.set_pos(self._line_pos - self.conf_cross // 2, a)
            self.conf.set_size(self.conf_cross, b - a)
        else:
            self.conf.set_pos(a, self._line_pos - self.conf_cross // 2)
            self.conf.set_size(b - a, self.conf_cross)

    def _animate(self, get, put, target):
        a = lv.anim_t()
        a.init()
        a.set_var(self.card)
        a.set_values(int(get() * 100), int(target * 100))
        a.set_duration(self.anim_ms)
        a.set_path_cb(lv.anim_t.path_ease_out)

        def cb(_a, val):
            put(val / 100.0)
            self._update()
        a.set_custom_exec_cb(cb)
        lv.anim_t.start(a)

    def set_value(self, v, anim=True):
        if anim:
            self._animate(lambda: self._value,
                          lambda x: setattr(self, '_value', x), v)
        else:
            self._value = v
            self._update()

    def set_confidence(self, lo, hi, anim=True):
        if anim:
            self._animate(lambda: self._lo,
                          lambda x: setattr(self, '_lo', x), lo)
            self._animate(lambda: self._hi,
                          lambda x: setattr(self, '_hi', x), hi)
        else:
            self._lo, self._hi = lo, hi
            self._update()


# ---------------- demo: two instances + 2 Hz mean-reverting random walk ----
disp = lv.display_get_default()
H = disp.get_vertical_resolution()
scr = lv.screen_active()
scr.set_style_bg_color(lv.color_hex(0x000000), 0)

common = dict(vertical=True, line_pct=30, padding=12, caret_pct=105,
              confidence_color=0x4ADE80, confidence_cross=10,
              confidence_opa=204, anim_ms=950,
              width=110, height=H - 10, y=5)

vspeed = LinearScale(scr, x=5, **common)
vaccel = LinearScale(scr, x=125,
                     major_ticks=(-1, -0.5, -0.1, 0, 0.1, 0.5, 1),
                     weights=(.1, .1, .3, .3, .1, .1),
                     minor_ticks=tuple([x / 10 for x in _DEF_MINORS]),
                     inter_ticks=(-0.05, 0.05),
                     **common)

_state = [(vspeed, 10.0, [0.0]), (vaccel, 1.0, [0.0])]


def demo_tick(t):
    for scale, rng, val in _state:
        val[0] += -0.15 * val[0] + _frand(-0.15 * rng, 0.15 * rng)
        val[0] = max(-rng, min(rng, val[0]))
        spread = _frand(0.05 * rng, 0.2 * rng)
        scale.set_value(val[0])
        scale.set_confidence(val[0] - spread, val[0] + spread)


lv.timer_create(demo_tick, 500, None)
