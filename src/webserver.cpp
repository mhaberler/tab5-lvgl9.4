#include <WiFi.h>
#include <NetworkClient.h>
#include <WebServer.h>
#include "svelteesp32webserver.h"

#ifndef SVELTEESP32_FILE_INDEX_HTML
    #error Missing index file
#endif

WebServer server(80);

bool ledState = false;

String getStatusJson() {
    return "{\"uptime\":" + String(millis() / 1000) + ",\"led\":" + (ledState ? "true" : "false") + "}";
}


void handleRoot() {
    //   digitalWrite(led, 1);
    char temp[400];
    int sec = millis() / 1000;
    int hr = sec / 3600;
    int min = (sec / 60) % 60;
    sec = sec % 60;

    snprintf(
        temp, 400,

        "<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <title>ESP32 Demo</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Hello from ESP32!</h1>\
    <p>Uptime: %02d:%02d:%02d</p>\
    <img src=\"/test.svg\" />\
  </body>\
</html>",

        hr, min, sec
    );
    server.send(200, "text/html", temp);
    //   digitalWrite(led, 0);
}

void handleNotFound() {
    //   digitalWrite(led, 1);
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";

    for (uint8_t i = 0; i < server.args(); i++) {
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }

    server.send(404, "text/plain", message);
    //   digitalWrite(led, 0);
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
}

void http_loop(void) {
    server.handleClient();
}
