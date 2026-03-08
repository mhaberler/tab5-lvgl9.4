#ifndef TELEPLOT_BACKEND_H
#define TELEPLOT_BACKEND_H

#include <cstddef>
#include <cstdint>

/**
 * Abstract backend interface for Teleplot network/stream transport.
 * Concrete implementations (Arduino, Unix, Callback) inherit from this class
 * and provide platform-specific initialization and data transmission.
 */
class TeleplotBackend {
public:
  virtual ~TeleplotBackend() = default;

  /**
   * Send data bytes to the configured destination.
   * @param data Pointer to data buffer.
   * @param len Number of bytes to send.
   * @return Number of bytes successfully sent, or -1 on error.
   */
  virtual int sendData(const uint8_t* data, size_t len) = 0;

  /**
   * Get the prefix string for formatted packets (e.g., ">" for Unix, "" for Arduino UDP).
   */
  virtual const char* getPrefix() const = 0;

  /**
   * Get the suffix string for formatted packets (e.g., "\n" for Unix, "" for Arduino UDP).
   */
  virtual const char* getSuffix() const = 0;

  /**
   * Get the section separator character (e.g., "§" for Arduino, "\xA7" for Unix).
   */
  virtual char getSectionSeparator() const = 0;
};

#endif // TELEPLOT_BACKEND_H
