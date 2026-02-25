#include <WiFi.h>
#include <NetworkClient.h>
#include <WebServer.h>
#include "svelteesp32webserver.h"


#ifndef SVELTEESP32_FILE_INDEX_HTML
    #error Missing index file
#endif

#ifdef USE_ELEGANT_OTA
    #include <ElegantOTA.h>
#endif

WebServer server(80);

bool ledState = false;

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
    return "{\"uptime\":" + String(millis() / 1000) + ",\"led\":" + (ledState ? "true" : "false") + "}";
}

void http_setup(void) {

    initSvelteStaticFiles(&server);

    server.on("/api/status", HTTP_GET, []() {
        server.send(200, "application/json", getStatusJson());
    });

    server.on("/api/toggle", HTTP_POST, []() {
        ledState = !ledState;
        server.send(200, "application/json", getStatusJson());
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
#ifdef USE_ELEGANT_OTA
    ElegantOTA.loop();
#endif
}
