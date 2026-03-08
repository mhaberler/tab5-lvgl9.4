#include <Arduino.h>
#include <WiFiServer.h>
#include <PicoWebsocket.h>
#include "ArduinoJson.h"
#include <list>
#include <chrono>

::WiFiServer websocket_http_server(8080);
PicoWebsocket::Server<::WiFiServer> teleplot_websocket_server(websocket_http_server);

std::list<PicoWebsocket::Server<::WiFiServer>::Client> websocket_clients;

static constexpr size_t MAX_WEBSOCKET_CLIENTS = 10;

size_t websocketWriteCallback(const uint8_t* buf, size_t len) {
    JsonDocument doc;
    String msg;
    doc["data"] = String(buf, len);
    doc["fromSerial"] = false;
    doc["timestamp"] = std::chrono::time_point_cast<std::chrono::microseconds>(
                           std::chrono::system_clock::now())
                       .time_since_epoch()
                       .count() / 1000.0;;
    serializeJson(doc, msg);
    // Broadcast to all connected WebSocket clients
    size_t broadcast_count = 0;
    for (auto &websocket : websocket_clients) {
        if (websocket.connected()) {
            websocket.write(msg.c_str(), msg.length(), true, false);
            broadcast_count++;
        }
    }

    // if (broadcast_count > 0) {
    //     Serial.printf("[Teleplot] Broadcast %zu bytes to %zu WebSocket client(s)\n", len, broadcast_count);
    // }

    return len;
}

void websocket_setup() {
    teleplot_websocket_server.begin();
    Serial.println("[WebSocket] Server started on port 8080");
}

void websocket_loop() {
    // Accept new clients (non-blocking)
    if (websocket_clients.size() < MAX_WEBSOCKET_CLIENTS) {
        auto websocket = teleplot_websocket_server.accept();
        if (websocket.connected()) {
            websocket_clients.push_back(websocket);
            Serial.printf("[WebSocket] Client connected. Total clients: %zu\n", websocket_clients.size());
        }
    }

    yield();

    // Process existing clients (iterate and remove disconnected)
    for (auto it = websocket_clients.begin(); it != websocket_clients.end(); ) {
        auto &websocket = *it;

        // Remove disconnected clients
        if (!websocket.connected()) {
            it = websocket_clients.erase(it);
            Serial.printf("[WebSocket] Client disconnected. Total clients: %zu\n", websocket_clients.size());
            continue;
        }

        // Process incoming data
        if (websocket.available()) {
            uint8_t buffer[256];
            const auto bytes_read = websocket.read(buffer, 256);
            if (bytes_read > 0) {
                // Serial.printf("[WebSocket] Received %zu bytes: ", bytes_read);
                // Serial.write(buffer, bytes_read);
                // Serial.println();
            }
        }

        ++it;
    }

    yield();
}