#if defined(M5UNIFIED)
    #include <M5Unified.h>
#endif
#include <ESPmDNS.h>
#include <Wire.h>
#include <PicoMQTT.h>
#include <PicoWebsocket.h>
#include <WiFi.h>
#include "ESP_HostedOTA.h"
#include <SD_MMC.h>
#include "BLEScanner.h"
#include "led.hpp"
#include "i2cio.hpp"

#ifdef TELEPLOT_ARDUINO
    #include "Teleplot.h"
    Teleplot teleplot;
    float i;
#endif

#ifdef LVGL_UI
    #include <lvgl.h>
    #include "display_driver.h"
    #include "ui.h"
#endif

#ifdef BOARD_HAS_PSRAM
    #define RBMEM MALLOC_CAP_SPIRAM
#else
    #define RBMEM MALLOC_CAP_DEFAULT
#endif

extern PicoMQTT::Server mqtt;
#ifdef USE_BLESCANNER
    static auto &bleScanner = BLEScanner::instance();
#endif
extern bool decodedOnly;
extern uint8_t wifi_status;

void i2c_init(TwoWire &wire);
void wifi_setup();
void wifi_loop();
void http_setup(void);
void http_loop(void);
void websocket_setup();
void websocket_loop();

size_t websocketWriteCallback(const uint8_t* buf, size_t len);

void setup() {
    Serial.begin(115200);
#if defined(COLOR_LED_PIN)
    led_setup();
    led_update(0.1, 0);
    led_update(0.3, 1);
    led_update(0.5, 2);
#endif
    //delay(3000);
#if defined(M5UNIFIED)
    auto cfg = M5.config();
    cfg.output_power = true;
    M5.begin(cfg);
    M5.Ex_I2C.begin();
    Wire.end();
    Wire.begin(M5.Ex_I2C.getSDA(), M5.Ex_I2C.getSCL(), 100000);
#else
    Wire.setClock(400000);
    // Wire.begin(WIRE_SDA, WIRE_SCL, 100000);
    i2c_scan(Wire);

    Wire1.begin(WIRE1_SDA  , WIRE1_SCL, 400000);
    i2c_scan(Wire1);
#endif
#ifdef USE_I2C_SENSORS
    i2c_init(Wire);
#endif
    // pinMode(RELAY1_PIN, OUTPUT);
    // pinMode(RELAY2_PIN, OUTPUT);
    // digitalWrite(RELAY1_PIN, HIGH);
    // digitalWrite(RELAY2_PIN, LOW);
    // delay(500);
    // digitalWrite(RELAY1_PIN, LOW);
    // digitalWrite(RELAY2_PIN, HIGH);
    // delay(500);
    // digitalWrite(RELAY1_PIN, HIGH);
    // digitalWrite(RELAY2_PIN, LOW);
#if defined(HAS_DISPLAY) && defined(M5UNIFIED)
    M5.Display.setRotation(3);
    M5.Display.setBrightness(200);
#ifdef LVGL_UI
    delay(100);
    display_init();
    ui_init();
#endif
#endif
    wifi_setup();
    http_setup();
#ifdef USE_BLESCANNER
    bleScanner.begin(4096, 15000, 100, 99, 4096, 1, RBMEM);
#endif
#ifdef TELEPLOT_ARDUINO
    teleplot.begin(websocketWriteCallback);
    // teleplot.begin(&Serial);
#endif
}

void loop() {
    unsigned long now = millis();

    http_loop();
#if defined(M5UNIFIED)
    M5.update();
#endif
#ifdef LVGL_UI
    display_update();
#endif
    if (wifi_status == WL_CONNECTED) {
#ifdef USE_BLESCANNER
        {
            JsonDocument doc;
            char mac[16];
            if (bleScanner.process(doc, mac, sizeof(mac))) {
                // Only publish if not decodedOnly mode, or if decoded property is true
                if (!decodedOnly || doc["decoded"].as<bool>()) {
                    // Remove decoded attribute in decodedOnly mode before publishing
                    if (decodedOnly) {
                        doc.remove("decoded");
                    }
                    String topic = String("ble/") + mac;
                    auto publish = mqtt.begin_publish(topic.c_str(), measureJson(doc));
                    serializeJson(doc, publish);
                    publish.send();
                }
            }
        } {
            static BLEScanner::Stats lastStats = {};
            static unsigned long lastPublishTime = 0;

            if (now - lastPublishTime >= 10000) {
                auto st = bleScanner.stats();
                if (st.hwmBytes > lastStats.hwmBytes ||
                        st.queueFull > lastStats.queueFull ||
                        st.acquireFail > lastStats.acquireFail ||
                        st.received > lastStats.received ||
                        st.decoded > lastStats.decoded) {
                    lastStats = st;
                    JsonDocument sdoc;
                    sdoc["hwm"] = st.hwmPercent;
                    sdoc["qfull"] = st.queueFull;
                    sdoc["afail"] = st.acquireFail;
                    sdoc["rx"] = st.received;
                    sdoc["dec"] = st.decoded;
                    auto publish = mqtt.begin_publish("ble/$stats", measureJson(sdoc));
                    serializeJson(sdoc, publish);
                    publish.send();
                }
                lastPublishTime = now;
            }
        }
#endif
#ifdef TELEPLOT_ARDUINO

        static unsigned long lastTpTime = 0;
        if (now - lastTpTime >= 50) {
            teleplot.update("sin", sin(i), "");
            teleplot.update("cos", cos(i), "");
            i += 0.1;
        }
#endif
        mqtt.loop();
    }
    wifi_loop();
    yield();
}