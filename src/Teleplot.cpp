#include "Teleplot.h"
#include "Teleplot_backend.h"
#include "Teleplot_callback.h"

#ifdef EMBEDDED_TELEPLOT
#include "Teleplot_arduino.h"
#else
#include "Teleplot_unix.h"
#endif

Teleplot::Teleplot() : backend_(nullptr), millis_offset_(0) {}

Teleplot::~Teleplot() = default;

#ifdef EMBEDDED_TELEPLOT
void Teleplot::begin(IPAddress address, uint16_t port, int64_t millis_offset, bool enabled) {
  auto arduino_backend = std::make_unique<TeleplotArduinoBackend>();
  arduino_backend->begin(address, port, enabled);
  backend_ = std::move(arduino_backend);
  millis_offset_ = millis_offset;
}

void Teleplot::begin(Stream* stream, int64_t millis_offset, bool enabled) {
  auto arduino_backend = std::make_unique<TeleplotArduinoBackend>();
  arduino_backend->begin(stream, enabled);
  backend_ = std::move(arduino_backend);
  millis_offset_ = millis_offset;
}
#endif

#ifndef EMBEDDED_TELEPLOT
void Teleplot::begin(const std::string& address, unsigned int port, bool enabled) {
  auto unix_backend = std::make_unique<TeleplotUnixBackend>();
  unix_backend->begin(address, port, enabled);
  backend_ = std::move(unix_backend);
  millis_offset_ = 0;
}

void Teleplot::begin(int fd, bool enabled) {
  auto unix_backend = std::make_unique<TeleplotUnixBackend>();
  unix_backend->begin(fd, enabled);
  backend_ = std::move(unix_backend);
  millis_offset_ = 0;
}
#endif

void Teleplot::begin(WriteCallback callback, int64_t millis_offset, bool enabled) {
  auto callback_backend = std::make_unique<TeleplotCallbackBackend>();
  callback_backend->begin(callback, enabled);
  backend_ = std::move(callback_backend);
  millis_offset_ = millis_offset;
}

void Teleplot::emit(const std::string& data) {
  if (!backend_) {
    return;
  }
  backend_->sendData(reinterpret_cast<const uint8_t*>(data.c_str()), data.size());
}

std::string Teleplot::formatPacket(const std::string& key, const std::string& values,
                                  const std::string& flags, std::string unit,
                                  bool is3D) {
  if (!backend_) {
    return "";
  }
  std::string prefix = backend_->getPrefix();
  std::string suffix = backend_->getSuffix();
  char section = backend_->getSectionSeparator();

  std::string unitFormatted = (unit == "") ? "" : std::string(1, section) + unit;
  return fmt::format("{}{}{}:{}{}|{}{}", prefix, is3D ? "3D|" : "", key, values,
                unitFormatted, flags, suffix);
}

void Teleplot::log(const std::string& log) {
  if (!backend_) {
    return;
  }
  int64_t nowMs = std::chrono::time_point_cast<std::chrono::milliseconds>(
                      std::chrono::system_clock::now())
                      .time_since_epoch()
                      .count();
  std::string prefix = backend_->getPrefix();
  std::string suffix = backend_->getSuffix();
  if (prefix.empty()) {
    prefix = ">";  // Default prefix for log
  }
  emit(prefix + std::to_string(nowMs) + ":" + log + suffix);
}

void Teleplot::log_ms(const std::string& log, unsigned long nowMs) {
  if (!backend_) {
    return;
  }
  int64_t timeStamp = nowMs + millis_offset_;
  std::string prefix = backend_->getPrefix();
  std::string suffix = backend_->getSuffix();
  if (prefix.empty()) {
    prefix = ">";  // Default prefix for log
  }
  emit(prefix + std::to_string(timeStamp) + ":" + log + suffix);
}
