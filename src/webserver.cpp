#include <WiFi.h>
#include <NetworkClient.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "svelteesp32webserver.h"


#ifndef SVELTEESP32_FILE_INDEX_HTML
    #error Missing index file
#endif

#ifdef USE_ELEGANT_OTA
    #include <ElegantOTA.h>
#endif

WebServer server(80);

bool ledState = false;
extern void publishStatus();

#ifdef USE_ELEGANT_OTA
unsigned long ota_progress_millis = 0;
void onOTAStart() {
    // Log when OTA has started
    log_w("OTA update started!");
    // <Add your own code here>
}

void onOTAProgress(size_t current, size_t final) {
    // Log every 1 second
    if (millis() - ota_progress_millis > 1000) {
        ota_progress_millis = millis();
        log_w("OTA Progress Current: %u bytes, Final: %u bytes", current, final);
    }
}

void onOTAEnd(bool success) {
    // Log when OTA has finished
    if (success) {
        log_w("OTA update finished successfully!");
    } else {
        log_e("There was an error during OTA update!");
    }
    // <Add your own code here>
}
#endif

String getStatusJson() {
    JsonDocument doc;
    doc["uptime"] = millis() / 1000;
    doc["led"] = ledState;
    String out;
    serializeJson(doc, out);
    return out;
}

// Send CORS headers so the Vite dev server (different origin) can reach us.
static void sendCorsHeaders() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void http_setup(void) {

    initSvelteStaticFiles(&server);

    // Handle CORS preflight for all /api/* routes
    server.on("/api/status",  HTTP_OPTIONS, []() { sendCorsHeaders(); server.send(204); });
    server.on("/api/toggle",  HTTP_OPTIONS, []() { sendCorsHeaders(); server.send(204); });

    server.on("/api/status", HTTP_GET, []() {
        sendCorsHeaders();
        server.send(200, "application/json", getStatusJson());
    });

    server.on("/api/toggle", HTTP_POST, []() {
        sendCorsHeaders();
        ledState = !ledState;
        server.send(200, "application/json", getStatusJson());
        publishStatus();
    });

    server.begin();
#ifdef USE_ELEGANT_OTA
    ElegantOTA.begin(&server);
    // ElegantOTA callbacks
    ElegantOTA.onStart(onOTAStart);
    ElegantOTA.onProgress(onOTAProgress);
    ElegantOTA.onEnd(onOTAEnd);
#endif
}

void http_loop(void) {
    server.handleClient();

    static unsigned long lastStatusPublish = 0;
    if (millis() - lastStatusPublish >= 5000) {
        lastStatusPublish = millis();
        publishStatus();
    }

#ifdef USE_ELEGANT_OTA
    ElegantOTA.loop();
#endif
}
