#include "Wire.h"
#include <M5Unified.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BMP5xx.h"
#include <LPS22DFSensor.h>
#include <Dps3xx.h>
#include <Adafruit_INA228.h>

Adafruit_BMP5xx bmp581; // Create BMP5xx object
bmp5xx_powermode_t desiredMode = BMP5XX_POWERMODE_NORMAL;

// Get separate sensor objects for temperature and pressure
Adafruit_Sensor *bmp_temp = NULL;
Adafruit_Sensor *bmp_pressure = NULL;

LPS22DFSensor *lps22;
Dps3xx Dps3xxPressureSensor = Dps3xx();

Adafruit_INA228 ina228 = Adafruit_INA228();


void scanI2C(m5::I2C_Class* scanWire) {
    bool result[0x80];
    scanWire->scanID(result);
    for(int i = 0x08; i < 0x78; ++i) {
        if (result[i])
            log_w("%02x", i);
    }
}

bool bmp581_init(TwoWire& wire, uint8_t address ) {
    if (!bmp581.begin(address, &wire)) {
        log_e("BMP581 not connected");
        return false;
    }
    bmp581.setTemperatureOversampling(BMP5XX_OVERSAMPLING_1X);
    bmp581.setPressureOversampling(BMP5XX_OVERSAMPLING_1X);
    bmp581.setIIRFilterCoeff(BMP5XX_IIR_FILTER_BYPASS);
    bmp581.setOutputDataRate(BMP5XX_ODR_50_HZ);
    bmp581.setPowerMode(BMP5XX_POWERMODE_NORMAL);

    // bmp581.setTemperatureOversampling(BMP5XX_OVERSAMPLING_2X);
    // bmp581.setPressureOversampling(BMP5XX_OVERSAMPLING_16X);
    // bmp581.setIIRFilterCoeff(BMP5XX_IIR_FILTER_COEFF_3);
    // bmp581.setOutputDataRate(BMP5XX_ODR_50_HZ);
    // bmp581.setPowerMode(BMP5XX_POWERMODE_NORMAL);

    bmp_temp = bmp581.getTemperatureSensor();
    bmp_pressure = bmp581.getPressureSensor();
    sensors_event_t temp_event, pressure_event;

    for (int i = 0; i < 10; i++) {
        // Get pressure event using unified sensor API
        if (bmp_pressure->getEvent(&pressure_event)) {
            log_w("BMP581  %f  hPa", pressure_event.pressure);
        } else {
            log_e("Failed to get pressure event");
        }
        delay(100);
    }
    return true;
}


void lps22_init(TwoWire& wire, uint8_t address ) {
    lps22 = new LPS22DFSensor(&wire, address);
    lps22->begin();
    lps22->Enable();
    for (int i = 0; i < 10; i++) {
        float pressure;
        lps22->GetPressure(&pressure);
        log_w("LPS22DF  %f  hPa", pressure);
        delay(100);
    }
}

void dps368_init(TwoWire& wire, uint8_t address ) {
    Dps3xxPressureSensor.begin(wire, address);
    lps22->begin();
    lps22->Enable();
    for (int i = 0; i < 10; i++) {
        float temperature;
        float pressure;
        uint8_t oversampling = 1;
        int16_t ret;
        ret = Dps3xxPressureSensor.measureTempOnce(temperature, oversampling);
        ret = Dps3xxPressureSensor.measurePressureOnce(pressure, oversampling);
        log_w("DPS368  %f  hPa", pressure/100.0);
        delay(100);
    }
}

bool ina228_init(TwoWire& wire, uint8_t address ) {
    if (!ina228.begin(address, &wire)) {
        log_e("INA228 not connected");
        return false;
    }
    ina228.setShunt(0.02, 5.0);
    ina228.setAveragingCount(INA228_COUNT_16);
    ina228.setVoltageConversionTime(INA2XX_TIME_4120_us);
    ina228.setCurrentConversionTime(INA2XX_TIME_4120_us);

    for (int i = 0; i < 10; i++) {
        float V = ina228.getBusVoltage_V();
        float mA = ina228.getCurrent_mA();
        log_w("INA228 %f V %f mA", V, mA);
        delay(100);
    }
    return true;
}


void i2c_init(TwoWire &wire) {
    bmp581_init(wire, BMP5XX_ALTERNATIVE_ADDRESS);
    lps22_init(wire, LPS22DF_I2C_ADD_H);
    dps368_init(wire, 0x77);
    ina228_init(wire, 0x40);
}

