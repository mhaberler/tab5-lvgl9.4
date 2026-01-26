#include <PicoMQTT.h>
#include <PicoWebsocket.h>
#include <ArduinoJson.h>

WiFiServer tcp_server(MQTT_PORT);
WiFiServer websocket_underlying_server(MQTTWS_PORT);
PicoWebsocket::Server<::WiFiServer>
    websocket_server(websocket_underlying_server);

class CustomMQTTServer : public PicoMQTT::Server
{
    using PicoMQTT::Server::Server;

public:
    int32_t connected, subscribed, messages;

protected:
    void on_connected(const char *client_id) override
    {
        log_i("client %s connected", client_id);
        connected++;
    }
    virtual void on_disconnected(const char *client_id) override
    {
        log_i("client %s disconnected", client_id);
        connected--;
    }
    virtual void on_subscribe(const char *client_id, const char *topic) override
    {
        log_i("client %s subscribed %s", client_id, topic);
        subscribed++;
    }
    virtual void on_unsubscribe(const char *client_id,
                                const char *topic) override
    {
        log_i("client %s unsubscribed %s", client_id, topic);
        subscribed--;
    }
    virtual void on_message(const char *topic,
                            PicoMQTT::IncomingPacket &packet) override
    {
        log_i("message topic=%s", topic);
        PicoMQTT::Server::Server::on_message(topic, packet);
        messages++;
    }
};

CustomMQTTServer mqtt(tcp_server, websocket_server);

extern "C"
{
    void report_brightness(int32_t value)
    {
        JsonDocument output;
        output["level"] = value;
        auto publish = mqtt.begin_publish("brightness", measureJson(output));
        serializeJson(output, publish);
        publish.send();
    }
}