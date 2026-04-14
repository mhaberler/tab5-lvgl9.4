
// RMT-based RGB LED driver for WS2812 and SK6812 LEDs
// Uses native ESP32 RMT peripheral

#if defined(COLOR_LED_PIN)
    #include <Arduino.h>

    // RMT bit timing for WS2812/SK6812 @ 10MHz clock (100ns per tick)
    // Bit "1": 0.8us high (8 ticks), 0.4us low (4 ticks)
    // Bit "0": 0.4us high (4 ticks), 0.85us low (8 ticks)

    static rmt_data_t led_data[NUM_LEDS * 24];  // 24 bits per LED (RGB)
    static const uint8_t LED_BRIGHTNESS = 50;   // 0-255

    struct RGB {
        uint8_t r, g, b;
    };

    static void setLedBit(int bitIndex, bool value) {
        if (value) {
            led_data[bitIndex].level0 = 1;
            led_data[bitIndex].duration0 = 8;   // 0.8us high
            led_data[bitIndex].level1 = 0;
            led_data[bitIndex].duration1 = 4;   // 0.4us low
        } else {
            led_data[bitIndex].level0 = 1;
            led_data[bitIndex].duration0 = 4;   // 0.4us high
            led_data[bitIndex].level1 = 0;
            led_data[bitIndex].duration1 = 8;   // 0.85us low
        }
    }

    static RGB hsvToRgb(uint8_t hue, uint8_t sat, uint8_t val) {
        // Simple HSV to RGB conversion
        float h = hue / 255.0f * 6.0f;
        float s = sat / 255.0f;
        float v = val / 255.0f;

        int i = (int)h;
        float f = h - i;
        float p = v * (1.0f - s);
        float q = v * (1.0f - s * f);
        float t = v * (1.0f - s * (1.0f - f));

        float r, g, b;
        switch (i % 6) {
            case 0: r = v; g = t; b = p; break;
            case 1: r = q; g = v; b = p; break;
            case 2: r = p; g = v; b = t; break;
            case 3: r = p; g = q; b = v; break;
            case 4: r = t; g = p; b = v; break;
            case 5: r = v; g = p; b = q; break;
            default: r = g = b = 0;
        }

        RGB rgb;
        rgb.r = (uint8_t)(r * 255 * LED_BRIGHTNESS / 255);
        rgb.g = (uint8_t)(g * 255 * LED_BRIGHTNESS / 255);
        rgb.b = (uint8_t)(b * 255 * LED_BRIGHTNESS / 255);
        return rgb;
    }

    static void setLedColor(int ledIndex, RGB color) {
        // WS2812/SK6812 color order: G R B
        int bitBase = ledIndex * 24;

        uint8_t colors[] = {color.g, color.r, color.b};
        for (int col = 0; col < 3; col++) {
            uint8_t byte = colors[col];
            for (int bit = 0; bit < 8; bit++) {
                bool isBitSet = (byte & (1 << (7 - bit))) != 0;
                setLedBit(bitBase + col * 8 + bit, isBitSet);
            }
        }
    }
#endif

void led_setup() {
    #if defined(COLOR_LED_PIN)
        if (!rmtInit(COLOR_LED_PIN, RMT_TX_MODE, RMT_MEM_NUM_BLOCKS_1, 10000000)) {
            log_e("RMT init failed on pin %d", COLOR_LED_PIN);
        }
    #endif
}

void led_update(float value, int led) {
    #if defined(COLOR_LED_PIN)
        if (led >= 0 && led < NUM_LEDS) {
            uint8_t hue = map(value * 100, 0, 300, 0, 255);
            RGB color = hsvToRgb(hue, 255, 255);
            setLedColor(led, color);
            rmtWrite(COLOR_LED_PIN, led_data, NUM_LEDS * 24, RMT_WAIT_FOR_EVER);
        }
    #endif
}

void setRainbowColor(float value, int led) {
    #if defined(COLOR_LED_PIN)
        if (led >= 0 && led < NUM_LEDS) {
            uint8_t hue = map(value * 100, 0, 300, 0, 255);
            RGB color = hsvToRgb(hue, 255, 255);
            setLedColor(led, color);
        }
    #endif
}