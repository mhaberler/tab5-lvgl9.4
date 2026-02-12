#include <lvgl.h>
#include <M5Unified.h>
#include <ESPmDNS.h>
#include <PicoMQTT.h>
#include <PicoWebsocket.h>
#include <WiFi.h>
#include "ESP_HostedOTA.h"
#include <SD_MMC.h>
#include "BLEScanner.h"

#ifdef LVGL_UI
    #include "display_driver.h"
    #include "ui.h"
#endif

static const char *hostname = HOSTNAME;
static wl_status_t wifi_status = WL_STOPPED;

extern PicoMQTT::Server mqtt;

static auto &bleScanner = BLEScanner::instance();
void i2c_init(TwoWire &scanWire);
void scanI2C(m5::I2C_Class* scanWire);

void wifi_scan();


void setup() {
    Serial.begin(115200);

    delay(3000);


    auto cfg = M5.config();
    cfg.output_power = true;
    M5.begin(cfg);


#ifdef BOARD_HAS_SDIO_ESP_HOSTED
    WiFi.setPins(BOARD_SDIO_ESP_HOSTED_CLK, BOARD_SDIO_ESP_HOSTED_CMD, BOARD_SDIO_ESP_HOSTED_D0,
                 BOARD_SDIO_ESP_HOSTED_D1, BOARD_SDIO_ESP_HOSTED_D2, BOARD_SDIO_ESP_HOSTED_D3,
                 BOARD_SDIO_ESP_HOSTED_RESET);
#endif
    WiFi.STA.begin();
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, true, 1000); // needed for scanNetworks to work

    wifi_scan();
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

    log_w("connecting to SSID %s", WIFI_SSID);
    WiFi.STA.connect(WIFI_SSID, WIFI_PASS);
    bleScanner.begin(4096, 15000, 100, 99, 4096, 1, MALLOC_CAP_SPIRAM);
}

void loop() {
    M5.update();
    wl_status_t ws = WiFi.STA.status();
    if (ws ^ wifi_status) {
        wifi_status = ws; // track changes
        switch (ws) {
            case WL_CONNECTED:
                log_w("WiFi: Connected, IP: %s", WiFi.STA.localIP().toString().c_str());

                if (updateEspHostedSlave()) {
                    // Restart the host ESP32 after successful update
                    // This is currently required to properly activate the new firmware
                    // on the ESP-Hosted co-processor
                    ESP.restart();
                }
                if (MDNS.begin(hostname)) {
                    MDNS.addService("mqtt", "tcp", MQTT_PORT);
                    MDNS.addService("mqtt-ws", "tcp", MQTTWS_PORT);
                    MDNS.addServiceTxt("mqtt-ws", "tcp", "path", "/mqtt");
                    mdns_service_instance_name_set("_mqtt", "_tcp", "PicoMQTT TCP broker");
                    mdns_service_instance_name_set("_mqtt-ws", "_tcp",
                                                   "PicoMQTT Websockets broker");
                }
                mqtt.begin();
                break;
            case WL_NO_SSID_AVAIL:
                log_w("WiFi: SSID %s not found", WIFI_SSID);
                break;
            case WL_DISCONNECTED:
                log_w("WiFi: disconnected");
                break;
            default:
                log_w("WiFi status: %d", ws);
                break;
        }
        delay(300);
    }
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