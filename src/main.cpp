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
static auto &bleScanner = BLEScanner::instance();
extern bool decodedOnly;

void i2c_init(TwoWire &wire);
void wifi_setup();
void wifi_loop();


void setup() {
    Serial.begin(115200);
    delay(3000);
#if defined(M5UNIFIED)
    auto cfg = M5.config();
    cfg.output_power = true;
    M5.begin(cfg);
    M5.Ex_I2C.begin();
    Wire.end();
    Wire.begin(M5.Ex_I2C.getSDA(), M5.Ex_I2C.getSCL(), 100000);
#else
    Wire.begin();
#endif
#ifdef USE_I2C_SENSORS
    i2c_init(Wire);
#endif
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
    mqtt.begin();
    bleScanner.begin(4096, 15000, 100, 99, 4096, 1, RBMEM);
}

void loop() {
#if defined(M5UNIFIED)
    M5.update();
#endif
#ifdef LVGL_UI
    display_update();
#endif
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
    }
    {
        static BLEScanner::Stats lastStats = {};
        static unsigned long lastPublishTime = 0;
        unsigned long now = millis();

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
    mqtt.loop();
    wifi_loop();
    yield();
}