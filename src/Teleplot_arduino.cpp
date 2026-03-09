#ifdef TELEPLOT_ARDUINO

#include "Teleplot_arduino.h"

// Static member initialization
WiFiUDP TeleplotArduinoBackend::udp_;

TeleplotArduinoBackend::TeleplotArduinoBackend() : use_stream_(false) {}

void TeleplotArduinoBackend::begin(IPAddress address, uint16_t port) {
  address_ = address;
  port_ = port;
  use_stream_ = false;
  stream_ = nullptr;
}

void TeleplotArduinoBackend::begin(Stream* stream) {
  stream_ = stream;
  use_stream_ = true;
  address_ = IPAddress();
  port_ = -1;
}

int TeleplotArduinoBackend::sendData(const uint8_t* data, size_t len) {
  if (use_stream_ && stream_) {
    return stream_->write(data, len);
  }

  if (!use_stream_ && port_ > -1) {
    udp_.beginPacket(address_, port_);
    size_t written = udp_.write(data, len);
    udp_.flush();
    udp_.endPacket();
    return written;
  }

  return 0;
}

#endif // TELEPLOT_ARDUINO
