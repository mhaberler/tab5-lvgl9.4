#include <WiFi.h>
#include <WiFiMulti.h>
#include <PicoMQTT.h>
#include <PicoWebsocket.h>
#include <ArduinoJson.h>

WiFiServer tcp_server(MQTT_PORT);
WiFiServer websocket_underlying_server(MQTTWS_PORT);
PicoWebsocket::Server<::WiFiServer>
websocket_server(websocket_underlying_server);

extern bool ledState;
extern JsonDocument wifiCredentials;
extern WiFiMulti wifiMulti;
void publishStatus();
void startWiFiScan();
void saveWifiCredentials();
void publishWifiCredentials();

class CustomMQTTServer : public PicoMQTT::Server {
    using PicoMQTT::Server::Server;

  public:
    int32_t connected, subscribed, messages;

  protected:
    void on_connected(const char *client_id) override {
        log_i("client %s connected", client_id);
        connected++;
    }
    virtual void on_disconnected(const char *client_id) override {
        log_i("client %s disconnected", client_id);
        connected--;
    }
    virtual void on_subscribe(const char *client_id, const char *topic) override {
        log_i("client %s subscribed %s", client_id, topic);
        subscribed++;
    }
    virtual void on_unsubscribe(const char *client_id,
                                const char *topic) override {
        log_i("client %s unsubscribed %s", client_id, topic);
        subscribed--;
    }
    virtual void on_message(const char *topic,
                            PicoMQTT::IncomingPacket &packet) override {
        log_i("message topic=%s", topic);
        if (strcmp(topic, "command") == 0) {
            char buf[256];
            size_t len = packet.readBytes(buf, sizeof(buf) - 1);
            buf[len] = '\0';
            JsonDocument doc;
            if (!deserializeJson(doc, buf)) {
                const char* action = doc["action"];
                if (action && strcmp(action, "toggle") == 0) {
                    ledState = !ledState;
                    publishStatus();
                }
                if (action && strcmp(action, "wifiscan") == 0) {
                    startWiFiScan();
                }
                if (action && strcmp(action, "restart") == 0) {
                    ESP.restart();
                }
                if (action && strcmp(action, "wifi_save") == 0) {
                    const char* ssid = doc["ssid"];
                    const char* pw = doc["pw"] | "";
                    if (ssid && ssid[0]) {
                        bool found = false;
                        for (JsonObject cred : wifiCredentials.as<JsonArray>()) {
                            if (strcmp(cred["ssid"], ssid) == 0) {
                                cred["pw"] = pw;
                                found = true;
                                break;
                            }
                        }
                        if (!found) {
                            JsonObject o = wifiCredentials.as<JsonArray>().add<JsonObject>();
                            o["ssid"] = ssid;
                            o["pw"] = pw;
                            wifiMulti.addAP(ssid, pw);
                        }
                        saveWifiCredentials();
                        publishWifiCredentials();
                    }
                }
                if (action && strcmp(action, "wifi_forget") == 0) {
                    const char* ssid = doc["ssid"];
                    if (ssid && ssid[0]) {
                        JsonArray arr = wifiCredentials.as<JsonArray>();
                        for (size_t i = 0; i < arr.size(); i++) {
                            if (strcmp(arr[i]["ssid"], ssid) == 0) {
                                arr.remove(i);
                                break;
                            }
                        }
                        saveWifiCredentials();
                        publishWifiCredentials();
                    }
                }
                if (action && strcmp(action, "wifi_list") == 0) {
                    publishWifiCredentials();
                }
            }
            messages++;
            return;
        }
        PicoMQTT::Server::Server::on_message(topic, packet);
        messages++;
    }
};

CustomMQTTServer mqtt(tcp_server, websocket_server);

void publishStatus() {
    JsonDocument doc;
    doc["uptime"] = millis() / 1000;
    doc["led"] = ledState;
    auto publish = mqtt.begin_publish("status", measureJson(doc));
    serializeJson(doc, publish);
    publish.send();
}

extern "C"
{
    void report_brightness(int32_t value) {
        JsonDocument output;
        output["level"] = value;
        auto publish = mqtt.begin_publish("brightness", measureJson(output));
        serializeJson(output, publish);
        publish.send();
    }
}