#include <Arduino.h>
#include <WiFiServer.h>
#include <PicoWebsocket.h>

::WiFiServer websocket_http_server(8080);
PicoWebsocket::Server<::WiFiServer> teleplot_websocket_server(websocket_http_server);

Client *websocket;

size_t websocketWriteCallback(const uint8_t* buf, size_t len) {
    Serial.printf("[Teleplot] Writing %zu bytes: ", len);
    Serial.write(buf, len);
    Serial.println();
    return len;
    // websocket.write(buffer, bytes_read);

}


void websocket_setup() {
    teleplot_websocket_server.begin();
}

void websocket_loop() {
    // websocket = teleplot_websocket_server.accept();
    // if (!websocket) {
    //     return;
    // }

    // while (websocket.connected()) {
    //     yield();

    //     if (websocket.available()) {
    //         uint8_t buffer[128];
    //         const auto bytes_read = websocket.read(buffer, 128);
    //         Serial.printf("[Teleplot] Writing %zu bytes: ", bytes_read);
    //         Serial.write(buffer, bytes_read);
    //         Serial.println();

    //     }
    // }

}