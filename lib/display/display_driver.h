#pragma once

#include <M5GFX.h>
#include "lvgl.h"

extern M5GFX display;

void display_init();
void display_update();
extern "C"
{
    void set_brightness(int32_t value);
}