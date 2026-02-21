
static const char *hostname = HOSTNAME;

#ifdef ESP_FS_WEBSERVER
#include <FS.h>
#include <LittleFS.h>
#include "FSWebServer.h"
#include <ESPmDNS.h>
#include <WiFi.h>

#include "index_htm.h"

#define FILESYSTEM LittleFS
const char* hostname = HOSTNAME;

// If you edit server port, remember to change also websocket port in index_htm.h
// By default websocket port with F,WebServer library is server port + 1
FSWebServer server(FILESYSTEM, 80, hostname);

// Log messages both on Serial and WebSocket clients
void wsLogPrintf(bool toSerial, const char* format, ...) {
    char buffer[128];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, 128, format, args);
    va_end(args);
    server.broadcastWebSocket(buffer);
    if (toSerial)
        Serial.println(buffer);
}

/////////////////////////   WebSocket event callback////////////////////////////////   WebSocket Handler  /////////////////////////////
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Disconnected!\n", num);
            break;
        case WStype_CONNECTED: {
                IPAddress ip = server.getWebSocketServer()->remoteIP(num);
                server.getWebSocketServer()->sendTXT(num, "{\"Connected\": true}");

                // Print welcome message to all clients and to Serial
                wsLogPrintf(true, "Hello to client #%d [%s]\n", (int)num, ip.toString().c_str());
            }
            break;
        case WStype_TEXT:
            Serial.printf("[%u] got Text: %s\n", num, payload);   // Got text message from a client
            break;
        case WStype_BIN:
            Serial.printf("[%u] got binary length: %u\n", num, length); // Got binary message from a client
            break;
        default:
            break;
    }
}

// Test "config" values
bool decodedOnly = true;

// Timezone definition to get properly time from NTP server
#define MYTZ "CET-1CEST,M3.5.0,M10.5.0/3"
struct tm Time;

////////////////////////////////  NTP Time  /////////////////////////////////////
void getUpdatedtime(const uint32_t timeout) {
    uint32_t start = millis();
    Serial.print("Sync time...");
    while (millis() - start < timeout && Time.tm_year <= (1970 - 1900)) {
        time_t now = time(nullptr);
        Time = *localtime(&now);
        delay(5);
    }
    Serial.println(" done.");
}


////////////////////////////////  Filesystem  /////////////////////////////////////////
bool startFilesystem() {
    if (FILESYSTEM.begin()) {
        server.printFileList(FILESYSTEM, "/", 1, Serial);
        return true;
    } else {
        Serial.println("ERROR on mounting filesystem. It will be reformatted!");
        FILESYSTEM.format();
        ESP.restart();
    }
    return false;
}


////////////////////  Load and save application configuration from filesystem  ////////////////////
bool loadApplicationConfig() {
    if (FILESYSTEM.exists(server.getConfiFileName())) {
        server.getOptionValue("Report decoded ads only", decodedOnly);
        server.closeSetupConfiguration();  // Close configuration to free resources
        return true;
    }
    return false;
}


void wifi_setup() {

    // FILESYSTEM INIT
    if (startFilesystem()) {
        // Load configuration (if not present, default will be created when webserver will start)
        if (loadApplicationConfig()) {
            Serial.println(F("\nApplication option loaded"));
            Serial.printf("Report decoded ads only: %d\n",decodedOnly);
        } else
            Serial.println(F("Application options NOT loaded!"));
    }

    // Try to connect to WiFi (will start AP if not connected after timeout)
    if (!server.startWiFi(10000)) {
        Serial.println("\nWiFi not connected! Starting AP mode...");
        server.startCaptivePortal("ESP_AP", "123456789", "/setup");
    }

    // Configure /setup page
    server.addOptionBox("Options");
    server.addOption("Report decoded ads only", decodedOnly);

    // Add custom page handlers
    server.on("/", HTTP_GET, []() {
        server.send_P(200, "text/html", homepage);
    });

    // Enable ACE FS file web editor and add FS info callback function
    server.enableFsCodeEditor();

    // Init with WebSocket event handler and start server
    server.begin(webSocketEvent);

    MDNS.addService("mqtt", "tcp", MQTT_PORT);
    MDNS.addService("mqtt-ws", "tcp", MQTTWS_PORT);
    MDNS.addServiceTxt("mqtt-ws", "tcp", "path", "/mqtt");

    // Get MAC address
    uint8_t mac[6];
    WiFi.macAddress(mac);
    String macStr = String(mac[0], HEX) + String(mac[1], HEX) + String(mac[2], HEX) + String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX);
    macStr.toUpperCase();

    mdns_service_instance_name_set("_mqtt", "_tcp", ("PicoMQTT TCP broker (" + macStr + ")").c_str());
    mdns_service_instance_name_set("_mqtt-ws", "_tcp",
                                   ("PicoMQTT Websockets broker (" + macStr + ")").c_str());
    Serial.print(F("ESP Web Server started on IP Address: "));
    Serial.println(server.getServerIP());
    Serial.println(F(
                       "Open /setup page to configure optional parameters.\n"
                       "Open /edit page to view, edit or upload example or your custom webserver source files."
                   ));

    // Set hostname
    WiFi.setHostname(hostname);
    configTzTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
}


void wifi_loop() {
    server.run();  // Handle client requests
}
#else

#include <Network.h>
#include <WiFi.h>
#include <WiFiMulti.h>

#include "ESP_HostedOTA.h"
#include <ESPmDNS.h>
#include <PicoMQTT.h>
#include <HTTPClient.h>
#include "mdns.h"


extern PicoMQTT::Server mqtt;
bool decodedOnly = true;
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


// static void addInterface(NetworkInterface * iface){
// #if defined(CONFIG_MDNS_ADD_CUSTOM_NETIF) && !defined(CONFIG_MDNS_PREDEF_NETIF_STA) && !defined(CONFIG_MDNS_PREDEF_NETIF_ETH)
//     /* Demonstration of adding a custom netif to mdns service, but we're adding the default example one,
//      * so we must disable all predefined interfaces (PREDEF_NETIF_STA, AP and ETH) first
//      */
//     ESP_ERROR_CHECK(mdns_register_netif(iface->netif()));
//     /* It is not enough to just register the interface, we have to enable is manually.
//      * This is typically performed in "GOT_IP" event handler, but we call it here directly
//      * since the `EXAMPLE_INTERFACE` netif is connected already, to keep the example simple.
//      */
//     ESP_ERROR_CHECK(mdns_netif_action(iface->netif(), MDNS_EVENT_ENABLE_IP4 | MDNS_EVENT_ENABLE_IP6));
//     ESP_ERROR_CHECK(mdns_netif_action(iface->netif(), MDNS_EVENT_ANNOUNCE_IP4 | MDNS_EVENT_ANNOUNCE_IP6));

// #if defined(CONFIG_MDNS_RESPOND_REVERSE_QUERIES)
//     ESP_ERROR_CHECK(mdns_netif_action(iface->netif(), MDNS_EVENT_IP4_REVERSE_LOOKUP | MDNS_EVENT_IP6_REVERSE_LOOKUP));
// #endif
// #endif // CONFIG_MDNS_ADD_CUSTOM_NETIF
// }

// static void _on_sys_event(arduino_event_t *event){
//     mdns_handle_system_event(NULL, event);
// }

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
    http_loop();
}
#endif