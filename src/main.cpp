#include <M5Unified.h>
#include <ESPmDNS.h>
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

extern PicoMQTT::Server mqtt;
static auto &bleScanner = BLEScanner::instance();

void i2c_init(TwoWire &scanWire);
void scanI2C(m5::I2C_Class* scanWire);
void cfg_setup();
void cfg_loop();


void setup() {
    Serial.begin(115200);
    delay(3000);
    auto cfg = M5.config();
    cfg.output_power = true;
    M5.begin(cfg);

    M5.Ex_I2C.begin();
    scanI2C(&M5.Ex_I2C);
    Wire.end();
    Wire.begin(M5.Ex_I2C.getSDA(), M5.Ex_I2C.getSCL(), 100000);
    i2c_init(Wire);

#ifdef  HAS_DISPLAY
    M5.Display.setRotation(3);
    M5.Display.setBrightness(200);
#ifdef LVGL_UI
    delay(100);
    display_init();
    ui_init();
#endif
#endif
    cfg_setup();
    mqtt.begin();
    bleScanner.begin(4096, 15000, 100, 99, 4096, 1, RBMEM);
}

void loop() {
    cfg_loop();
    M5.update();

#ifdef LVGL_UI
    display_update();
#endif
    {
        JsonDocument doc;
        char mac[16];
        if (bleScanner.process(doc, mac, sizeof(mac))) {
            String topic = String("ble/") + mac;
            auto publish = mqtt.begin_publish(topic.c_str(), measureJson(doc));
            serializeJson(doc, publish);
            publish.send();
        }
    }
    {
        static BLEScanner::Stats lastStats = {};
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
    }
    mqtt.loop();
    yield();
}