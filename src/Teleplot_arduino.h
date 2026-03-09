#ifndef TELEPLOT_ARDUINO_H
#define TELEPLOT_ARDUINO_H

#ifdef TELEPLOT_ARDUINO

#include "Teleplot_backend.h"
#include <Arduino.h>
#include <IPAddress.h>
#include <WiFiUdp.h>
#include <cstdint>

/**
 * Arduino-specific backend for Teleplot using WiFiUDP or Stream.
 * Handles initialization with IPAddress and port, and transmission via UDP or Stream.
 */
class TeleplotArduinoBackend : public TeleplotBackend {
public:
  TeleplotArduinoBackend();
  ~TeleplotArduinoBackend() = default;

  /**
   * Initialize backend for UDP transmission to a remote address.
   * @param address Target IP address.
   * @param port Target UDP port (default: 47269).
   */
  void begin(IPAddress address, uint16_t port = 47269);

  /**
   * Initialize backend for Stream (serial, etc.) transmission.
   * @param stream Pointer to Stream object (e.g., Serial).
   */
  void begin(Stream* stream);

  // TeleplotBackend interface
  int sendData(const uint8_t* data, size_t len) override;
  const char* getPrefix() const override { return ""; }
  const char* getSuffix() const override { return ""; }
  char getSectionSeparator() const override { return 0xA7; }

private:
  bool use_stream_ = false;
  Stream* stream_ = nullptr;
  IPAddress address_;
  uint16_t port_ = 47269;
  static WiFiUDP udp_;  // Shared UDP socket across all instances
};

#endif // TELEPLOT_ARDUINO

#endif // TELEPLOT_ARDUINO_H
