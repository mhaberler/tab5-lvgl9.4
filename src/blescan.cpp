#include <Arduino.h>
#include "freertos/ringbuf.h"
#include "ringbuffer.hpp"

#define ARDUINOJSON_USE_LONG_LONG 1
#include "ArduinoJson.h"
#include "broker.hpp"
#include "fmicro.h"

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEEddystoneURL.h>
#include <BLEEddystoneTLM.h>
#include <BLEBeacon.h>

#ifndef BLE_ADV_QUEUELEN
#define BLE_ADV_QUEUELEN 2048
#endif

static BLEScan *pBLEScan;
static espidf::RingBuffer *bleadv_queue;
static uint32_t queue_full, acquire_fail;

static constexpr uint32_t scanTime = 15 * 1000; // 15 seconds scan time.

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{

  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
    JsonDocument doc;
    JsonObject BLEdata = doc.to<JsonObject>();
    String mac_adress = advertisedDevice.getAddress().toString().c_str();
    mac_adress.toUpperCase();
    BLEdata["id"] = (char *)mac_adress.c_str();
    BLEdata["rssi"] = (int)advertisedDevice.getRSSI();

    if (advertisedDevice.haveName())
      BLEdata["name"] = (char *)advertisedDevice.getName().c_str();

    if (advertisedDevice.haveManufacturerData())
    {
      String strManufacturerData = advertisedDevice.getManufacturerData();
      // Convert to hex string
      String hexData = "";
      for (size_t i = 0; i < strManufacturerData.length(); i++)
      {
        char buf[3];
        sprintf(buf, "%02X", (uint8_t)strManufacturerData[i]);
        hexData += buf;
      }
      BLEdata["manufacturerdata"] = hexData;
    }

    if (advertisedDevice.haveTXPower())
      BLEdata["txpower"] = (int8_t)advertisedDevice.getTXPower();

    BLEdata["time"] = fseconds();
    void *ble_adv = nullptr;
    size_t total = measureMsgPack(BLEdata);
    if (bleadv_queue->send_acquire((void **)&ble_adv, total, 0) != pdTRUE)
    {
      acquire_fail++;
      return;
    }
    size_t n = serializeMsgPack(BLEdata, ble_adv, total);
    if (n != total)
    {
      log_e("serializeMsgPack: expected %u got %u", total, n);
    }
    else
    {
      if (bleadv_queue->send_complete(ble_adv) != pdTRUE)
      {
        queue_full++;
      }
    }
  }
};

void bleDeliver(JsonDocument &doc)
{
  auto publish = mqtt.begin_publish("ble/", measureJson(doc));
  serializeJson(doc, publish);
  publish.send();
}

void process_ble(void)
{
  if (bleadv_queue == NULL)
  {
    bleadv_queue = new espidf::RingBuffer();
    bleadv_queue->create(BLE_ADV_QUEUELEN, RINGBUF_TYPE_NOSPLIT);
  }
  size_t size = 0;
  void *buffer = bleadv_queue->receive(&size, 0);
  if (buffer == nullptr)
  {
    return;
  }
  JsonDocument doc;
  DeserializationError e = deserializeMsgPack(doc, buffer, size);
  bleDeliver(doc);
  bleadv_queue->return_item(buffer);
}

void run_ble(void)
{
  if (bleadv_queue == NULL)
  {
    bleadv_queue = new espidf::RingBuffer();
    bleadv_queue->create(BLE_ADV_QUEUELEN, RINGBUF_TYPE_NOSPLIT);
  }
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks(),
                                         true, true);
  pBLEScan->setActiveScan(false);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);

  while (1)
  {
    BLEScanResults *foundDevices = pBLEScan->start(scanTime / 1000, false);
    log_i("Devices found: %d", foundDevices->getCount());
    pBLEScan->clearResults();
    delay(1);
  }
}

void setup_ble(void)
{
  // Start BLE scanning in a separate RTOS task
  xTaskCreate(
      [](void *param)
      {
        run_ble();
        vTaskDelete(NULL); // Delete task after setup completes
      },
      "run_ble", // Task name
      4096,      // Stack size (bytes)
      NULL,      // Task parameter
      1,         // Task priority
      NULL       // Task handle
  );
}