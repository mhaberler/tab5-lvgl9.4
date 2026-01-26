#include <lvgl.h>
#include <M5Unified.h>
#include <ESPmDNS.h>
#include <PicoMQTT.h>
#include <PicoWebsocket.h>
#include <WiFi.h>
#include "ESP_HostedOTA.h"
#include <SD_MMC.h>

#include "display_driver.h"
#include "ui.h"

// SDIO pins for Tab5
#define SDIO2_CLK GPIO_NUM_12
#define SDIO2_CMD GPIO_NUM_13
#define SDIO2_D0 GPIO_NUM_11
#define SDIO2_D1 GPIO_NUM_10
#define SDIO2_D2 GPIO_NUM_9
#define SDIO2_D3 GPIO_NUM_8
#define SDIO2_RST GPIO_NUM_15

static const char *hostname = "picomqtt";
static wl_status_t wifi_status = WL_STOPPED;

extern PicoMQTT::Server mqtt;

void setup_ble(void);
void process_ble(void);

void setup()
{
  Serial.begin(115200);

  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(3);
  delay(100);
  display_init();
  M5.Display.setBrightness(200);
  ui_init();

  WiFi.setPins(SDIO2_CLK, SDIO2_CMD, SDIO2_D0, SDIO2_D1, SDIO2_D2, SDIO2_D3, SDIO2_RST);
  WiFi.STA.begin();

  WiFi.STA.connect(WIFI_SSID, WIFI_PASS);
}

void loop()
{
  M5.update();
  wl_status_t ws = WiFi.STA.status();
  if (ws ^ wifi_status)
  {
    wifi_status = ws; // track changes
    switch (ws)
    {
    case WL_CONNECTED:
      log_i("WiFi: Connected, IP: %s", WiFi.STA.localIP().toString().c_str());

      if (updateEspHostedSlave())
      {
        // Restart the host ESP32 after successful update
        // This is currently required to properly activate the new firmware
        // on the ESP-Hosted co-processor
        ESP.restart();
      }
      setup_ble();
      if (MDNS.begin(hostname))
      {
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
      log_i("WiFi: SSID %s not found", WIFI_SSID);
      break;
    case WL_DISCONNECTED:
      log_i("WiFi: disconnected");
      break;
    default:
      log_i("WiFi status: %d", ws);
      break;
    }
    delay(300);
  }

  display_update();
  process_ble();
  mqtt.loop();
  delay(1);
}