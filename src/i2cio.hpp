#pragma once
#include <Wire.h>

bool i2c_probe(TwoWire &w, uint8_t addr);
void i2c_scan(TwoWire &w);

static inline uint8_t detect(TwoWire &w, uint8_t addr) {
    return (i2c_probe(w, addr)) ?  addr : 0;
}