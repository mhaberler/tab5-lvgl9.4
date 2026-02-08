#include "BLEScanner.h"

#include <Arduino.h>
#include <vector>
#include <string>

#include "freertos/ringbuf.h"
#include "ringbuffer.hpp"
#include "esp_timer.h"

#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#include "BTHomeDecoder.h"

// ---------------------------------------------------------------------------
// Timing helper (replaces fmicro.h dependency)
// ---------------------------------------------------------------------------
static inline float fseconds() {
    return ((float)esp_timer_get_time()) * 1.0e-6f;
}

// ---------------------------------------------------------------------------
// Rounding helpers
// ---------------------------------------------------------------------------
#define round1(x) (round((x)*1.0e1) / 1.0e1)

// ---------------------------------------------------------------------------
// Decoder constants
// ---------------------------------------------------------------------------
static constexpr float k0 = 273.15f;

#define MOPEKA_TANK_LEVEL_COEFFICIENTS_PROPANE_0  0.573045f
#define MOPEKA_TANK_LEVEL_COEFFICIENTS_PROPANE_1 -0.002822f
#define MOPEKA_TANK_LEVEL_COEFFICIENTS_PROPANE_2 -0.00000535f

// ---------------------------------------------------------------------------
// Byte helpers
// ---------------------------------------------------------------------------
static inline int16_t getInt16LE(const std::vector<uint8_t> &data, int index) {
    return (int16_t)((data[index]) | (data[index + 1] << 8));
}

static inline int32_t getInt32LE(const std::vector<uint8_t> &data, int index) {
    return (int32_t)((data[index]) |
                     (data[index + 1] << 8) |
                     (data[index + 2] << 16) |
                     (data[index + 3] << 24));
}

static inline uint16_t getUint16LE(const std::vector<uint8_t> &data, int index) {
    return (uint16_t)((data[index]) | (data[index + 1] << 8));
}

static inline int16_t getInt16BE(const std::vector<uint8_t> &data, int index) {
    return (int16_t)(((uint16_t)data[index] << 8) | (uint16_t)data[index + 1]);
}

static inline uint16_t getUint16BE(const std::vector<uint8_t> &data, int index) {
    return (uint16_t)(((uint16_t)data[index] << 8) | (uint16_t)data[index + 1]);
}

static inline uint32_t getUint32LE(const std::vector<uint8_t> &data, int index) {
    return (uint32_t)((data[index]) |
                      (data[index + 1] << 8) |
                      (data[index + 2] << 16) |
                      (data[index + 3] << 24));
}

static inline int8_t getInt8(const std::vector<uint8_t> &data, int index) {
    return (int8_t)data[index];
}

static inline uint8_t getUint8(const std::vector<uint8_t> &data, int index) {
    return data[index];
}

static inline float convert_8_8_to_float(const std::vector<uint8_t> &data, int index) {
    // 8.8 to float converter
    auto frac = getUint8(data, index);
    auto base = getUint8(data, index+1);
    if (frac == 0xFF && base == 0xFF) {
        return 0.0f;
    } else {
        return (float)base + ((float)frac / 256.0f);
    }
}

// ---------------------------------------------------------------------------
// Hex conversion helpers
// ---------------------------------------------------------------------------
static bool hexStringToVector(const String &hexStr, std::vector<uint8_t> &buffer) {
    size_t len = hexStr.length();
    if (len & 1)
        return false;

    buffer.clear();
    if (len == 0)
        return true;

    buffer.resize(len / 2);
    uint8_t *out = buffer.data();
    const char *in = hexStr.c_str();

    for (size_t i = 0; i < len; i += 2) {
        uint8_t val = 0;
        for (int j = 0; j < 2; j++) {
            uint8_t nibble;
            char c = in[i + j];
            if (c >= '0' && c <= '9')
                nibble = c - '0';
            else if (c >= 'A' && c <= 'F')
                nibble = c - 'A' + 10;
            else if (c >= 'a' && c <= 'f')
                nibble = c - 'a' + 10;
            else {
                buffer.clear();
                return false;
            }
            val = (val << 4) | nibble;
        }
        *out++ = val;
    }
    return true;
}

static void bytesToHexString(const uint8_t *data, size_t len, String &hexStr) {
    static const char HEX_CHARS[] = "0123456789ABCDEF";
    hexStr = "";
    if (len == 0)
        return;
    hexStr.reserve(len * 2);
    for (size_t i = 0; i < len; i++) {
        hexStr += HEX_CHARS[data[i] >> 4];
        hexStr += HEX_CHARS[data[i] & 0x0F];
    }
}

static bool stringToHexString(const std::string &str, String &hexStr) {
    bytesToHexString((const uint8_t *)str.data(), str.length(), hexStr);
    return true;
}

// Non-static — also used by BTHomeDecoder.cpp
bool stringToHexString(const String &str, String &hexStr) {
    bytesToHexString((const uint8_t *)str.c_str(), str.length(), hexStr);
    return true;
}

// ---------------------------------------------------------------------------
// BLEScanner::Impl — hidden state
// ---------------------------------------------------------------------------
struct BLEScanner::Impl {
    espidf::RingBuffer *queue = nullptr;
    BLEScan *pBLEScan = nullptr;
    BTHomeDecoder bthDecoder;
    const char *bthKey = "";

    uint32_t scanTimeMs = 15000;
    uint16_t scanInterval = 100;
    uint16_t scanWindow = 99;
    bool activeScan = false;

    uint32_t queueFull = 0;
    uint32_t acquireFail = 0;
    uint32_t received = 0;
    uint32_t decoded = 0;
};

// Singleton storage — the Impl pointer lives on the single instance.
static BLEScanner::Impl *s_impl = nullptr;

// ---------------------------------------------------------------------------
// volt2percent
// ---------------------------------------------------------------------------
static uint8_t volt2percent(float v) {
    float percent = (v - 2.2f) / 0.65f * 100.0f;
    if (percent < 0.0f)
        return 0;
    if (percent > 100.0f)
        return 100;
    return (uint8_t)percent;
}

// ---------------------------------------------------------------------------
// Decoders (file-static)
// ---------------------------------------------------------------------------
static bool decodeRuuvi(const std::vector<uint8_t> &data, JsonDocument &json) {
    if (data.size() < 20)
        return false;
    if (data[2] != 5)
        return false;

    json["dev"] = "Ruuvi";

    int16_t tempRaw = getInt16BE(data, 3);
    if (tempRaw != (int16_t)0x8000)
        json["temp"] = tempRaw * 0.005;

    uint16_t humidityRaw = getUint16BE(data, 5);
    if (humidityRaw != 0xFFFF)
        json["hum"] = humidityRaw * 0.0025;

    uint16_t pressureRaw = getUint16BE(data, 7);
    if (pressureRaw != 0xFFFF)
        json["press"] = (pressureRaw + 50000.0) / 100.0;

    int16_t accX = getInt16BE(data, 9);
    int16_t accY = getInt16BE(data, 11);
    int16_t accZ = getInt16BE(data, 13);
    if (accX != (int16_t)0x8000) json["accx"] = accX;
    if (accY != (int16_t)0x8000) json["accy"] = accY;
    if (accZ != (int16_t)0x8000) json["accz"] = accZ;

    uint16_t powerInfo = getUint16BE(data, 15);
    uint16_t batteryRaw = (powerInfo >> 5);
    if (batteryRaw != 2047) {
        float volt = (batteryRaw + 1600) / 1000.0f;
        json["bat"] = volt;
        json["batpct"] = volt2percent(volt);
    }

    uint8_t txpRaw = powerInfo & 0x1F;
    if (txpRaw != 31)
        json["txpwr"] = (txpRaw * 2) - 40;

    if (data[17] != 255)
        json["move"] = data[17];

    uint16_t sequenceNumber = getUint16BE(data, 18);
    if (sequenceNumber != 0xFFFF)
        json["seq"] = sequenceNumber;

    return true;
}

static bool decodeMopeka(const std::vector<uint8_t> &data, JsonDocument &json) {
    if (data.size() != 12)
        return false;

    json["dev"] = "Mopeka";
    json["type"] = data[2];
    float volt = (data[3] & 0x7f) / 32.0f;
    json["bat"] = volt;
    json["batpct"] = volt2percent(volt);
    json["sync"] = (data[4] & 0x80) > 0;
    float raw_temp = (data[4] & 0x7f);
    json["temp"] = raw_temp - 40.0f;
    json["quality"] = (data[6] >> 6);
    json["accx"] = data[10];
    json["accy"] = data[11];
    float raw_level = ((int(data[6]) << 8) + data[5]) & 0x3fff;

    json["lvl_raw"] = raw_level;
    json["lvl_prop"] = round1(raw_level *
                              (MOPEKA_TANK_LEVEL_COEFFICIENTS_PROPANE_0 +
                               (MOPEKA_TANK_LEVEL_COEFFICIENTS_PROPANE_1 * raw_temp) +
                               (MOPEKA_TANK_LEVEL_COEFFICIENTS_PROPANE_2 * raw_temp *
                                raw_temp)));
    return true;
}

static bool decodeTPMS100(const std::vector<uint8_t> &data, JsonDocument &json) {
    if (data.size() != 18)
        return false;

    json["dev"] = "TPMS0100";
    json["loc"] = data[2] & 0x7f;
    json["press"] = (float)getInt32LE(data, 8) / 100000.0f;
    json["temp"] = (float)getInt32LE(data, 12) / 100.0f;
    json["batpct"] = data[16];
    json["status"] = data[17];
    return true;
}

static bool decodeTPMS00AC(const std::vector<uint8_t> &data, JsonDocument &json) {
    if (data.size() != 15)
        return false;

    json["dev"] = "TPMS00AC";
    json["loc"] = data[6] & 0x7f;
    json["press"] = (float)getInt32LE(data, 0);
    json["temp"] = k0 + getInt32LE(data, 4) / 100.0f;
    json["batpct"] = data[5];
    json["status"] = 0;
    return true;
}

static bool decodeOtodata(const std::vector<uint8_t> &data, JsonDocument &json) {
    switch (data.size()) {
        case 21:
            json["dev"] = "Otodata";
            json["level"] = ((float)getUint16LE(data, 11)) / 100.0f;
            json["status"] = getUint16LE(data, 13);
            break;
        case 24: {
                json["dev"] = "Otodata";
                char buffer[12];
                utoa(getUint32LE(data, 9), buffer, 10);
                json["serial"] = buffer;
                utoa(getUint16LE(data, 21), buffer, 10);
                json["model"] = buffer;
                break;
            }
        default:
            return false;
    }
    return true;
}

static bool decodeRotarexELG(const std::vector<uint8_t> &data, JsonDocument &json,
                             JsonObject BLEdata) {
    if (data.size() != 12)
        return false;

    json["dev"] = "Rotarex";
    int16_t level = getInt16LE(data, 8) / 10.0f;

    switch (level) {
        case -32768:
            json["status"] = "no sensor";
            json["level"] = 0.0;
            break;
        case 10000:
            json["status"] = "full";
            json["level"] = level / 100.0f;
            break;
        default:
            json["level"] = level;
            json["status"] = "OK";
    }
    float volt = getInt16LE(data, 10) / 1000.0f;
    json["bat"] = volt;
    json["batpct"] = volt2percent(volt);
    json["connectable"] = BLEdata["connectable"];
    return true;
}

// assumes Mikrotik advertisements, no encryption
static bool decodeMikrotik(const std::vector<uint8_t> &data, JsonDocument &json) {
    if (data.size() != 20)
        return false;
    int16_t t = getInt16LE(data, 12);
    if (t != -32768) { // 0x8000 -> temp is unsupported (indoor)
        json["dev"] = "Mikrotik TG-BT5-OUT";
        json["tempc"] = t / 256.0;
    } else {
        json["dev"] = "Mikrotik TG-BT5-IN";
    }
    json["version"] = getUint8(data, 2);
    auto user = getUint8(data, 3);
    if (user & 0x01) {
        json["encrypted"] = true;
    } else {
        json["salt"] = getUint16LE(data, 4);;
        json["accx"] = convert_8_8_to_float(data, 6);
        json["accy"] = convert_8_8_to_float(data, 8);
        json["accz"] = convert_8_8_to_float(data, 10);

        // uptime (4 bytes, little-endian)
        json["uptime"] = getUint32LE(data, 14);

        // flags (1 byte)
        uint8_t flags = getUint8(data, 18);
        if (flags & 1) {
            json["reed_switch"] = true;
        }
        if (flags & 2) {
            json["accel_tilt"] = true;
        }
        if (flags & 4) {
            json["accel_drop"] = true;
        }
        if (flags & 8) {
            json["impact_x"] = true;
        }
        if (flags & 16) {
            json["impact_y"] = true;
        }
        if (flags & 32) {
            json["impact_z"] = true;
        }
        // battery (1 byte)
        uint8_t batt = getUint8(data, 19);
        json["batt"] = batt;
    }
    return true;
}

static bool decodeBTHome(JsonObject BLEdata, JsonDocument &json,
                         BTHomeDecoder &decoder, const char *key) {
    std::vector<uint8_t> sd;
    if (!hexStringToVector(BLEdata["sd"], sd))
        return false;

    BTHomeDecodeResult bthRes = decoder.parseBTHomeV2(
                                    std::string(sd.begin(), sd.end()),
                                    BLEdata["mac"],
                                    key);

    if (bthRes.isBTHome && bthRes.decryptionSucceeded) {
        JsonObject root = json.to<JsonObject>();
        root["bthome_version"] = bthRes.bthomeVersion;
        JsonArray measArr = root["measurements"].to<JsonArray>();

        for (auto &m : bthRes.measurements) {
            JsonObject obj = measArr.add<JsonObject>();
            obj["object_id"] = m.objectID;
            obj["name"]      = m.name;
            obj["value"]     = m.value;
            obj["unit"]      = m.unit;
        }
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// BLE scan callback — enqueues raw advertisement data as MsgPack
// ---------------------------------------------------------------------------
class ScanCallback : public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) override {
        if (!s_impl || !s_impl->queue)
            return;

        JsonDocument doc;
        JsonObject BLEdata = doc.to<JsonObject>();

        String mac = advertisedDevice.getAddress().toString();
        mac.toUpperCase();
        BLEdata["mac"] = (char *)mac.c_str();
        BLEdata["rssi"] = (int)advertisedDevice.getRSSI();

        if (advertisedDevice.haveName())
            BLEdata["name"] = (char *)advertisedDevice.getName().c_str();

        if (advertisedDevice.haveManufacturerData()) {
            String hexData;
            stringToHexString(advertisedDevice.getManufacturerData(), hexData);
            BLEdata["mfd"] = hexData;
        }

        if (advertisedDevice.haveServiceUUID())
            BLEdata["svcuuid"] = (char *)advertisedDevice.getServiceUUID().toString().c_str();

        int sdCount = advertisedDevice.getServiceDataUUIDCount();
        if (sdCount > 0) {
            int idx = sdCount - 1;
            BLEdata["svduuid"] = (char *)advertisedDevice.getServiceDataUUID(idx).toString().c_str();
            String hexData;
            stringToHexString(advertisedDevice.getServiceData(idx), hexData);
            BLEdata["sd"] = hexData;
        }

        if (advertisedDevice.haveTXPower())
            BLEdata["txpwr"] = (int8_t)advertisedDevice.getTXPower();

        BLEdata["time"] = fseconds();

        void *ble_adv = nullptr;
        size_t total = measureMsgPack(BLEdata);
        if (s_impl->queue->send_acquire((void **)&ble_adv, total, 0) != pdTRUE) {
            s_impl->acquireFail++;
            return;
        }

        size_t n = serializeMsgPack(BLEdata, ble_adv, total);
        if (n != total) {
            log_e("serializeMsgPack: expected %u got %u", total, n);
        } else {
            if (s_impl->queue->send_complete(ble_adv) != pdTRUE) {
                s_impl->queueFull++;
            } else {
                s_impl->queue->update_high_watermark();
            }
        }
    }
};

// ---------------------------------------------------------------------------
// Scan task (runs forever on its own RTOS task)
// ---------------------------------------------------------------------------
static void scanTask(void *param) {
    auto *impl = static_cast<BLEScanner::Impl *>(param);

    BLEDevice::init("");
    impl->pBLEScan = BLEDevice::getScan();
    impl->pBLEScan->setAdvertisedDeviceCallbacks(new ScanCallback(), true, true);
    impl->pBLEScan->setActiveScan(impl->activeScan);
    impl->pBLEScan->setInterval(impl->scanInterval);
    impl->pBLEScan->setWindow(impl->scanWindow);

    while (true) {
        BLEScanResults *foundDevices = impl->pBLEScan->start(impl->scanTimeMs / 1000, false);
        log_d("Devices found: %d", foundDevices->getCount());
        impl->pBLEScan->clearResults();
        delay(1);
    }
}

// ---------------------------------------------------------------------------
// BLEScanner public API
// ---------------------------------------------------------------------------
BLEScanner &BLEScanner::instance() {
    static BLEScanner s;
    return s;
}

void BLEScanner::setBTHomeKey(const char *hexKey) {
    if (!_impl) {
        _impl = new Impl();
        s_impl = _impl;
    }
    _impl->bthKey = hexKey ? hexKey : "";
}

void BLEScanner::setActiveScan(bool active) {
    if (!_impl) {
        _impl = new Impl();
        s_impl = _impl;
    }
    _impl->activeScan = active;
}

BLEScanner::Stats BLEScanner::stats() const {
    Stats s = {};
    if (!_impl || !_impl->queue)
        return s;
    s.hwmBytes    = _impl->queue->get_high_watermark();
    s.totalBytes  = _impl->queue->get_total_size();
    s.hwmPercent  = s.totalBytes > 0 ? (uint8_t)((s.hwmBytes * 100) / s.totalBytes) : 0;
    s.queueFull   = _impl->queueFull;
    s.acquireFail = _impl->acquireFail;
    s.received    = _impl->received;
    s.decoded     = _impl->decoded;
    return s;
}

void BLEScanner::begin(size_t ringBufSize,
                       uint32_t scanTimeMs,
                       uint16_t scanInterval,
                       uint16_t scanWindow,
                       uint32_t taskStackSize,
                       UBaseType_t taskPriority,
                       UBaseType_t ringBufCap) {
    if (_started)
        return;
    _started = true;

    if (!_impl) {
        _impl = new Impl();
        s_impl = _impl;
    }

    _impl->scanTimeMs = scanTimeMs;
    _impl->scanInterval = scanInterval;
    _impl->scanWindow = scanWindow;

    _impl->queue = new espidf::RingBuffer();
    _impl->queue->create(ringBufSize, RINGBUF_TYPE_NOSPLIT, ringBufCap);

    xTaskCreate(scanTask, "ble_scan", taskStackSize, _impl, taskPriority, nullptr);
}

bool BLEScanner::deliver(JsonDocument &rawDoc, JsonDocument &outDoc) {
    bool decoded = false;
    std::vector<uint8_t> mfd;

    if (rawDoc.containsKey("svduuid") &&
            String(rawDoc["svduuid"]).indexOf("fcd2") != -1) {
        decoded = decodeBTHome(rawDoc.as<JsonObject>(), outDoc,
                               _impl->bthDecoder, _impl->bthKey);
    } else if (rawDoc.containsKey("mfd")) {
        if (hexStringToVector(rawDoc["mfd"], mfd) && mfd.size() >= 2) {
            uint16_t mfid = mfd[1] << 8 | mfd[0];
            switch (mfid) {
                case 0x0499:
                    decoded = decodeRuuvi(mfd, outDoc);
                    break;
                case 0x0059:
                    decoded = decodeMopeka(mfd, outDoc);
                    break;
                case 0x0100:
                    decoded = decodeTPMS100(mfd, outDoc);
                    break;
                case 0x00AC:
                    decoded = decodeTPMS00AC(mfd, outDoc);
                    break;
                case 0x03B1:
                    decoded = decodeOtodata(mfd, outDoc);
                    break;
                case 0xffff:
                    decoded = decodeRotarexELG(mfd, outDoc,
                                               rawDoc.as<JsonObject>());
                    break;
                case  0x094f:
                    decoded = decodeMikrotik(mfd, outDoc);
                    break;
            }
        }
    }
    return decoded;
}

bool BLEScanner::process(JsonDocument &doc, char *mac, size_t macLen) {
    if (!_impl || !_impl->queue)
        return false;

    size_t size = 0;
    void *buffer = _impl->queue->receive(&size, 0);
    if (buffer == nullptr)
        return false;

    JsonDocument rawDoc;
    deserializeMsgPack(rawDoc, buffer, size);
    _impl->queue->return_item(buffer);
    _impl->received++;

    // Decode
    JsonDocument decodedDoc;
    bool decoded = deliver(rawDoc, decodedDoc);
    if (decoded)
        _impl->decoded++;

    // Pick output document
    JsonDocument &outDoc = decoded ? decodedDoc : rawDoc;

    // Merge common metadata into decoded results
    if (decoded) {
        outDoc["mac"]  = rawDoc["mac"];
        outDoc["time"] = rawDoc["time"];
        outDoc["rssi"] = rawDoc["rssi"];
        if (rawDoc.containsKey("name"))
            outDoc["name"] = rawDoc["name"];
        if (rawDoc.containsKey("txpwr"))
            outDoc["txpwr"] = rawDoc["txpwr"];
    }

    // Extract MAC (strip colons)
    String macStr = rawDoc["mac"].as<String>();
    macStr.replace(":", "");
    size_t copyLen = macStr.length();
    if (copyLen >= macLen)
        copyLen = macLen - 1;
    memcpy(mac, macStr.c_str(), copyLen);
    mac[copyLen] = '\0';

    // Move result into caller's doc
    doc.set(outDoc);
    return true;
}
