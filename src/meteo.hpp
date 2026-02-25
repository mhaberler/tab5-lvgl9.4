#pragma once
#include <math.h>

#define QNH                  1013.25

static inline float Get_QNH_Altitude(float pressure, float calc_qnh) {
    // return the altitude in meters that corresponds to the given pressure in hundredths of a millibar
    float pressure_fact = pressure/calc_qnh;
    float temp_fact = (273.15+15)/0.0065;
    float air_const = 1/5.255;
    return ((1 - pow(pressure_fact, air_const))*temp_fact);
}

static inline float altitude_from_pressure(float pressure) {
    return Get_QNH_Altitude(pressure, QNH);
}
