
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

#include <WiFi.h>
#include "ESP_HostedOTA.h"
#include <ESPmDNS.h>

bool decodedOnly = true;
static wl_status_t wifi_status = WL_STOPPED;

void wifi_setup() {
#ifdef BOARD_HAS_SDIO_ESP_HOSTED
    WiFi.setPins(BOARD_SDIO_ESP_HOSTED_CLK, BOARD_SDIO_ESP_HOSTED_CMD, BOARD_SDIO_ESP_HOSTED_D0,
                 BOARD_SDIO_ESP_HOSTED_D1, BOARD_SDIO_ESP_HOSTED_D2, BOARD_SDIO_ESP_HOSTED_D3,
                 BOARD_SDIO_ESP_HOSTED_RESET);
#endif
    WiFi.STA.begin();
    log_w("connecting to SSID %s", WIFI_SSID);
    WiFi.STA.connect(WIFI_SSID, WIFI_PASS);
}

void wifi_loop() {
    wl_status_t ws = WiFi.STA.status();
    if (ws ^ wifi_status) {
        wifi_status = ws; // track changes
        switch (ws) {
            case WL_CONNECTED: {


                    log_w("WiFi: Connected, IP: %s", WiFi.STA.localIP().toString().c_str());

                    if (updateEspHostedSlave()) {
                        // Restart the host ESP32 after successful update
                        // This is currently required to properly activate the new firmware
                        // on the ESP-Hosted co-processor
                        ESP.restart();
                    }
                    // Get MAC address
                    uint8_t mac[6];
                    WiFi.macAddress(mac);
                    String macStr = String(mac[0], HEX) + String(mac[1], HEX) + String(mac[2], HEX) + String(mac[3], HEX) + String(mac[4], HEX) + String(mac[5], HEX);
                    macStr.toUpperCase();


                    if (MDNS.begin(hostname)) {
                        MDNS.addService("mqtt", "tcp", MQTT_PORT);
                        MDNS.addService("mqtt-ws", "tcp", MQTTWS_PORT);
                        MDNS.addServiceTxt("mqtt-ws", "tcp", "path", "/mqtt");
                        mdns_service_instance_name_set("_mqtt", "_tcp", ("PicoMQTT-TCP-" + macStr).c_str());
                        mdns_service_instance_name_set("_mqtt-ws", "_tcp",
                                                       ("PicoMQTT-WS-" + macStr).c_str());
                    }
                }
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
}
#endif