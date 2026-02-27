
static const char *hostname = HOSTNAME;


#include <Network.h>
#include <WiFi.h>
#include <WiFiMulti.h>

#include "ESP_HostedOTA.h"
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <PicoMQTT.h>
#include <HTTPClient.h>
#include "mdns.h"


extern PicoMQTT::Server mqtt;
bool decodedOnly = true;
bool scanningWifi = false;
uint8_t wifi_status = WL_STOPPED;
uint8_t prev_clients = 255;
WiFiMulti wifiMulti;

void http_setup();
void http_loop();

// callback used to check Internet connectivity
// bool testConnection() {
//     HTTPClient http;
//     http.begin("http://www.espressif.com");
//     int httpCode = http.GET();
//     // we expect to get a 301 because it will ask to use HTTPS instead of HTTP
//     if (httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
//         return true;
//     }
//     return false;
// }

String macAddress;

void getMacAddress(String &macStr) {
    // Get MAC address
    uint8_t mac[6];
    Network.macAddress(mac);
    macStr = String(mac[0], HEX) + String(mac[1], HEX) + String(mac[2], HEX) + String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX);
    macStr.toUpperCase();
}

void onNetworkEvent(arduino_event_id_t event) {
    log_i("Network event: %s", NetworkEvents::eventName(event));
    // switch (event) {
    //     case ARDUINO_EVENT_ETH_GOT_IP:
    //     case ARDUINO_EVENT_ETH_GOT_IP6:
    //     case ARDUINO_EVENT_PPP_GOT_IP:
    //     case ARDUINO_EVENT_PPP_GOT_IP6:
    //     case ARDUINO_EVENT_WIFI_STA_GOT_IP:
    //     case ARDUINO_EVENT_WIFI_STA_GOT_IP6:
    //     case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
    //         break;
    //     default:
    //         ;
    // }
}
void startWiFiScan() {
    if (scanningWifi) {
        log_e("wifi scan already running");
        return;
    }
    log_w("wifi scan start");
    // WiFi.scanNetworks will return immediately in Async Mode.
    WiFi.scanNetworks(true);  // 'true' turns Async Mode ON
    scanningWifi = true;
}

static const char* authStr(wifi_auth_mode_t mode) {
    switch (mode) {
        case WIFI_AUTH_OPEN:            return "open";
        case WIFI_AUTH_WEP:             return "WEP";
        case WIFI_AUTH_WPA_PSK:         return "WPA";
        case WIFI_AUTH_WPA2_PSK:        return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:    return "WPA+WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-EAP";
        case WIFI_AUTH_WPA3_PSK:        return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK:   return "WPA2+WPA3";
        case WIFI_AUTH_WAPI_PSK:        return "WAPI";
        default:                        return "unknown";
    }
}

void publishScannedNetworks(uint16_t networksFound) {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (int i = 0; i < networksFound; ++i) {
        JsonObject net = arr.add<JsonObject>();
        net["ssid"]    = WiFi.SSID(i);
        net["bssid"]   = WiFi.BSSIDstr(i);
        net["rssi"]    = WiFi.RSSI(i);
        net["channel"] = WiFi.channel(i);
        net["auth"]    = authStr(WiFi.encryptionType(i));
    }

    auto publish = mqtt.begin_publish("/wifi/networks", measureJson(doc));
    serializeJson(doc, publish);
    publish.send();

    WiFi.scanDelete();
}


void wifi_setup() {
#ifdef BOARD_HAS_SDIO_ESP_HOSTED
    WiFi.setPins(BOARD_SDIO_ESP_HOSTED_CLK, BOARD_SDIO_ESP_HOSTED_CMD, BOARD_SDIO_ESP_HOSTED_D0,
                 BOARD_SDIO_ESP_HOSTED_D1, BOARD_SDIO_ESP_HOSTED_D2, BOARD_SDIO_ESP_HOSTED_D3,
                 BOARD_SDIO_ESP_HOSTED_RESET);
#endif

    getMacAddress(macAddress);
    log_i("MAC address=%s", macAddress.c_str());

    WiFi.mode(WIFI_AP_STA);
    String apSSID = "ESP32-" + macAddress;
    String apPASS = "ESP32-" + macAddress;
    log_i("AP SSID: %s PW: %s", apSSID.c_str(), apPASS.c_str());
    WiFi.AP.create(apSSID, apPASS);          // AP mode
    WiFi.AP.enableDhcpCaptivePortal();
    WiFi.AP.enableIPv6();

    WiFi.STA.begin();
    if (MDNS.begin(hostname)) {
        MDNS.enableWorkstation();
        MDNS.addService("mqtt", "tcp", MQTT_PORT);
        MDNS.addService("mqtt-ws", "tcp", MQTTWS_PORT);
        MDNS.addServiceTxt("mqtt-ws", "tcp", "path", "/mqtt");
        mdns_service_instance_name_set("_mqtt", "_tcp", ("PicoMQTT-TCP-" + macAddress).c_str());
        mdns_service_instance_name_set("_mqtt-ws", "_tcp",
                                       ("PicoMQTT-WS-" + macAddress).c_str());
    }
    // Set WiFi as default interface
    // Network.setDefaultInterface(WiFi.STA);
    Network.onEvent(onNetworkEvent);
    // WiFi.setBandMode(WIFI_BAND_MODE_AUTO); // default
    // WiFi.setBandMode(WIFI_BAND_MODE_5G_ONLY);
    // WiFi.setBandMode(WIFI_BAND_MODE_2G_ONLY);

#ifdef SSID1
    wifiMulti.addAP(SSID1, PW1);
#endif
#ifdef SSID2
    wifiMulti.addAP(SSID2, PW2);
#endif
#ifdef SSID3
    wifiMulti.addAP(SSID3, PW3);
#endif
#ifdef SSID4
    wifiMulti.addAP(SSID4, PW4);
#endif
#ifdef SSID5
    wifiMulti.addAP(SSID5, PW5);
#endif
#ifdef SSID6
    wifiMulti.addAP(SSID6, PW6);
#endif
    // These options can help when you need ANY kind of wifi connection to get a config file, report errors, etc.
    wifiMulti.setStrictMode(false);  // Default is true.  Library will disconnect and forget currently connected AP if it's not in the AP list.
    wifiMulti.setAllowOpenAP(true);  // Default is false.  True adds open APs to the AP list.
    // wifiMulti.setConnectionTestCallbackFunc(testConnection);  // Attempts to connect to a remote webserver in case of captive portals.
    log_i("connecting to WiFi");
    http_setup();
}

void wifi_loop() {
    uint8_t clients = WiFi.softAPgetStationNum();
    if (prev_clients ^ clients) {
        log_i("AP clients: %u", clients);
        prev_clients = clients;
    }
    uint8_t ws = wifiMulti.run();
    if (ws ^ wifi_status) {
        wifi_status = ws; // track changes
        switch (ws) {
            case WL_CONNECTED: {
                    log_i("WiFi: Connected to %s %s RSSI %d  IP: %s",
                          WiFi.STA.SSID().c_str(),
                          WiFi.STA.BSSIDstr().c_str(),
                          WiFi.STA.RSSI(),
                          WiFi.STA.localIP().toString().c_str());
                    switch (WiFi.getBand()) {
                        case WIFI_BAND_2G:
                            log_i("Band is 2.4 GHz");
                            break;
                        case WIFI_BAND_5G:
                            log_i("Band is 5 GHz");
                            break;
                    }
                    log_i("AP IP: %s", WiFi.softAPIP().toString().c_str());

                    if (updateEspHostedSlave()) {
                        // Restart the host ESP32 after successful update
                        // This is currently required to properly activate the new firmware
                        // on the ESP-Hosted co-processor
                        ESP.restart();
                    }
                    mqtt.begin();
                }
                break;
            case WL_NO_SSID_AVAIL:
                log_i("WiFi: WL_NO_SSID_AVAIL");
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
    static unsigned long lastScanTime = 0;
    unsigned long now = millis();

    // if (now - lastScanTime >= 20000) {
    //     lastScanTime = now;
    //     scanningWifi = true;
    //     startWiFiScan();
    // }

    if (scanningWifi) {
        int16_t WiFiScanStatus = WiFi.scanComplete();
        if (WiFiScanStatus < 0) {  // it is busy scanning or got an error
            if (WiFiScanStatus == WIFI_SCAN_FAILED) {
                Serial.println("WiFi Scan has failed. Starting again.");
                // startWiFiScan();
            }
            // other option is status WIFI_SCAN_RUNNING - just wait.
        } else {  // Found Zero or more Wireless Networks
            publishScannedNetworks(WiFiScanStatus);
            scanningWifi = false;
        }
    }


    http_loop();
}
