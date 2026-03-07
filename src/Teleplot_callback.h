#ifndef TELEPLOT_CALLBACK_H
#define TELEPLOT_CALLBACK_H

#include "Teleplot_backend.h"
#include <functional>
#include <cstddef>

/**
 * Callback-based backend for Teleplot.
 * Allows custom transport implementations by providing a callback function.
 * Platform-agnostic and works on all systems.
 */
class TeleplotCallbackBackend : public TeleplotBackend {
public:
  using WriteCallback = std::function<size_t(const uint8_t*, size_t)>;

  TeleplotCallbackBackend() = default;
  ~TeleplotCallbackBackend() = default;

  /**
   * Initialize backend with a custom write callback.
   * @param callback Function that writes data and returns bytes written.
   * @param enabled Enable the backend (default: true).
   */
  void begin(WriteCallback callback, bool enabled = true) {
    callback_ = callback;
    enabled_ = enabled;
  }

  // TeleplotBackend interface
  int sendData(const uint8_t* data, size_t len) override {
    if (!enabled_ || !callback_) {
      return 0;
    }
    return callback_(data, len);
  }

  const char* getPrefix() const override { return ">"; }
  const char* getSuffix() const override { return "\n"; }
  char getSectionSeparator() const override { return 0xA7; }
  bool isEnabled() const override { return enabled_; }
  void setEnabled(bool enabled) override { enabled_ = enabled; }

private:
  WriteCallback callback_;
  bool enabled_ = false;
};

#endif // TELEPLOT_CALLBACK_H
