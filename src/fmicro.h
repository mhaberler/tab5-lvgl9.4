#pragma once
#include <cmath>
#include "esp_timer.h"

// https://stackoverflow.com/questions/14369673/round-double-to-3-points-decimal
static inline double dround_to(double value, double precision = 1.0)
{
    return std::round(value / precision) * precision;
}

static inline float fround_to(float value, float precision = 1.0)
{
    return std::round(value / precision) * precision;
}

static inline double dmillis(void) {
    return (double) (esp_timer_get_time() / 1000.0L);
}

static inline double  dmicros(void) {
    return (double) esp_timer_get_time();
}

static inline double  dseconds(void) {
    return ((double) esp_timer_get_time()) *1.0e-6;;
}

static inline float fmillis(void) {
    return ((float)esp_timer_get_time()) / 1000.0;
}

static inline float  fseconds(void) {
    return ((float) esp_timer_get_time()) *1.0e-6;;
}

static inline float  fmicros(void) {
    return (float) esp_timer_get_time();
}

static inline float  fsecondsrd(float precision = 0.001) {
    return fround_to(((float) esp_timer_get_time()) *1.0e-6, precision);
}