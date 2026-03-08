// Teleplot
// Source: https://github.com/nesnes/teleplot

#ifndef TELEPLOT_H
#define TELEPLOT_H

// Platform-specific headers must come first for type declarations
#ifdef EMBEDDED_TELEPLOT
#include <Arduino.h>
#include <IPAddress.h>
#endif

#ifndef EMBEDDED_TELEPLOT
#include <string>
#endif

#include <chrono>
#include "fmt.h"
#include <functional>
#include <map>
#include <memory>
#include <cstdint>
#include "Teleplot_backend.h"

// Enable/Disable implementation optimisations:
#define TELEPLOT_USE_BUFFERING // Allows to group updates sent, but will use dynamic buffer map

// Default flags
#define TELEPLOT_FLAG_DEFAULT ""
#define TELEPLOT_FLAG_NOPLOT "np"
#define TELEPLOT_FLAG_2D "xy"
#define TELEPLOT_FLAG_TEXT "text"

// Packet format constants
const char PACKET_SHAPE_TYPE = 'S';
const char PACKET_COLOR = 'C';
const char PACKET_POSITION = 'P';
const char PACKET_QUATERNION = 'Q';
const char PACKET_ROTATION = 'R';
const char PACKET_RADIUS = 'A';
const char PACKET_HEIGHT = 'H';
const char PACKET_WIDTH = 'W';
const char PACKET_DEPTH = 'D';
const unsigned short ROUNDING_PRECISION = 3;

class ShapeTeleplot {
public:
  class ShapeValue {
  public:
    ShapeValue() : is_set_(false), value_(0){}
    ShapeValue(double val) : is_set_(true), value_(val){}

    bool is_set_;
    double value_;
    std::string valueRounded() const { return roundValue(value_, ROUNDING_PRECISION); }

  private:
    std::string roundValue(double value, unsigned short precision) const {
      std::string value_str = std::to_string(value);
      int res_length = value_str.length();

      int i = 0;
      bool stop = false;

      while (i < res_length && !stop) {
        if (value_str[i] == '.') {
          int u = i + precision;
          if (u + 1 < static_cast<int>(value_str.length())) {
            while (value_str[u] == '0')
              u--;

            res_length = u;
            if (i != u)
              res_length++;
          }

          stop = true;
        }
        i++;
      }

      return value_str.substr(0, res_length);
    }
  };

  ShapeTeleplot() = default;

  ShapeTeleplot(const std::string& name, const std::string& type,
                const std::string& color = "")
      : name_(name), type_(type), color_(color) {}

  const std::string& getName() const { return name_; }

  ShapeTeleplot& setPos(const ShapeValue& posX, const ShapeValue& posY = {},
                        const ShapeValue& posZ = {}) {
    pos_x_ = posX;
    pos_y_ = posY;
    pos_z_ = posZ;
    return *this;
  }

  ShapeTeleplot& setRot(const ShapeValue& rotX, const ShapeValue& rotY = {},
                        const ShapeValue& rotZ = {},
                        const ShapeValue& rotW = {}) {
    rot_x_ = rotX;
    rot_y_ = rotY;
    rot_z_ = rotZ;
    rot_w_ = rotW;
    return *this;
  }

  ShapeTeleplot& setCubeProperties(const ShapeValue& height,
                                   const ShapeValue& width = {},
                                   const ShapeValue& depth = {}) {
    height_ = height;
    width_ = width;
    depth_ = depth;
    return *this;
  }

  ShapeTeleplot& setSphereProperties(const ShapeValue& radius,
                                     const ShapeValue& precision) {
    radius_ = radius;
    precision_ = precision;
    return *this;
  }

  std::string toString() const {
    std::string result;

    result.append(fmt::format("S:{}", type_));
    if (color_ != "") {
      result.append(fmt::format(":C:{}", color_));
    }

    if (pos_x_.is_set_ || pos_y_.is_set_ || pos_z_.is_set_) {
      result.append(fmt::format(":P:"));
      if (pos_x_.is_set_) {
        result.append(fmt::format("{}", pos_x_.valueRounded()));
      }
      result.append(fmt::format(":"));

      if (pos_y_.is_set_) {
        result.append(fmt::format("{}", pos_y_.valueRounded()));
      }
      result.append(fmt::format(":"));
      if (pos_z_.is_set_) {
        result.append(fmt::format("{}", pos_z_.valueRounded()));
      }
    }

    if (rot_x_.is_set_ || rot_y_.is_set_ || rot_z_.is_set_ || rot_w_.is_set_) {
      result.append(fmt::format("{}", rot_w_.is_set_ ? ":Q:" : ":R:"));

      if (rot_x_.is_set_) {
        result.append(fmt::format("{}", rot_x_.valueRounded()));
      }
      result.append(fmt::format(":"));
      if (rot_y_.is_set_) {
        result.append(fmt::format("{}", rot_y_.valueRounded()));
      }
      result.append(fmt::format(":"));
      if (rot_z_.is_set_) {
        result.append(fmt::format("{}", rot_z_.valueRounded()));
      }
      result.append(fmt::format(":"));
      if (rot_w_.is_set_) {
        result.append(fmt::format("{}", rot_w_.valueRounded()));
      }
    }

    if (type_ == "sphere") {
      if (radius_.is_set_) {
        result.append(fmt::format(":RA:{}", radius_.valueRounded()));
      }
      if (precision_.is_set_) {
        result.append(fmt::format(":P:{}", precision_.valueRounded()));
      }
    }

    if (type_ == "cube") {
      if (height_.is_set_) {
        result.append(fmt::format(":H:{}", height_.valueRounded()));
      }
      if (width_.is_set_) {
        result.append(fmt::format(":W:{}", width_.valueRounded()));
      }
      if (depth_.is_set_) {
        result.append(fmt::format(":D:{}", depth_.valueRounded()));
      }
    }
    return result;
  }

private:
  const std::string name_;
  const std::string type_;
  const std::string color_;

  ShapeValue pos_x_;
  ShapeValue pos_y_;
  ShapeValue pos_z_;

  ShapeValue rot_x_;
  ShapeValue rot_y_;
  ShapeValue rot_z_;
  ShapeValue rot_w_;

  ShapeValue height_;
  ShapeValue width_;
  ShapeValue depth_;

  ShapeValue radius_;
  ShapeValue precision_;
};

class Teleplot {
public:
  using WriteCallback = std::function<size_t(const uint8_t*, size_t)>;

  Teleplot();
  ~Teleplot();

  // Platform-specific initialization methods

#ifdef EMBEDDED_TELEPLOT
  // Arduino backend with UDP
  void begin(IPAddress address, uint16_t port = 47269,
             int64_t millis_offset = 0, bool enabled = true);

  // Arduino backend with Stream (Serial, etc.)
  void begin(Stream* stream, int64_t millis_offset = 0, bool enabled = true);
#endif

#ifndef EMBEDDED_TELEPLOT
  // Unix backend with UDP socket
  void begin(const std::string& address, unsigned int port = 47269, bool enabled = true);

  // Unix backend with file descriptor
  void begin(int fd, bool enabled = true);

  // Static localhost instance for desktop debugging
  static Teleplot& localhost() {
    static Teleplot teleplot;
    teleplot.begin("127.0.0.1");
    return teleplot;
  }
#endif

  // Platform-agnostic callback backend (works everywhere)
  void begin(WriteCallback callback, int64_t millis_offset = 0, bool enabled = true);
  template <typename T>
  void update(const std::string& key, const T& value, std::string unit = "",
              std::string flags = TELEPLOT_FLAG_DEFAULT) {
    if (!enabled_) return;
    int64_t nowUs = std::chrono::time_point_cast<std::chrono::microseconds>(
                        std::chrono::system_clock::now())
                        .time_since_epoch()
                        .count();
    double nowMs = static_cast<double>(nowUs) / 1000.0;
    updateData(key, nowMs, value, 0, flags, unit);
  }

  template <typename T>
  void update_ms(const std::string& key, unsigned long nowMs, const T& value,
                 std::string unit = "",
                 std::string flags = TELEPLOT_FLAG_DEFAULT) {
    if (!enabled_) return;
    int64_t timeStamp = nowMs + millis_offset_;
    updateData(key, timeStamp, value, 0, flags, unit);
  }

  template <typename T1, typename T2>
  void update2D(const std::string& key, const T1& valueX, const T2& valueY,
                std::string flags = TELEPLOT_FLAG_2D) {
    if (!enabled_) return;
    int64_t nowUs = std::chrono::time_point_cast<std::chrono::microseconds>(
                        std::chrono::system_clock::now())
                        .time_since_epoch()
                        .count();
    double nowMs = static_cast<double>(nowUs) / 1000.0;
    updateData(key, valueX, valueY, nowMs, flags);
  }

  template <typename T1, typename T2>
  void update2D_ms(const std::string& key, unsigned long nowMs,
                   const T1& valueX, const T2& valueY,
                   std::string flags = TELEPLOT_FLAG_2D) {
    if (!enabled_) return;
    int64_t timeStamp = nowMs + millis_offset_;
    updateData(key, valueX, valueY, timeStamp, flags);
  }

  void update3D(const ShapeTeleplot& mshape,
                std::string flags = TELEPLOT_FLAG_DEFAULT) {
    if (!enabled_) return;
    int64_t nowUs = std::chrono::time_point_cast<std::chrono::microseconds>(
                        std::chrono::system_clock::now())
                        .time_since_epoch()
                        .count();
    double nowMs = static_cast<double>(nowUs) / 1000.0;
    updateData(mshape.getName(), nowMs, NULL, NULL, flags, "", mshape);
  }

  void update3D_ms(const ShapeTeleplot& mshape, unsigned long nowMs,
                   std::string flags = TELEPLOT_FLAG_DEFAULT) {
    if (!enabled_) return;
    int64_t timeStamp = nowMs + millis_offset_;
    updateData(mshape.getName(), timeStamp, NULL, NULL, flags, "", mshape);
  }

  void log(const std::string& log);

  void log_ms(const std::string& log, unsigned long nowMs);

  /**
   * Check if Teleplot is enabled.
   */
  bool isEnabled() const { return enabled_; }

  /**
   * Enable or disable Teleplot transmission.
   */
  void setEnabled(bool enabled) { enabled_ = enabled; }

private:
  template <typename T1, typename T2, typename T3>
  void updateData(const std::string& key, const T1& valueX, const T2& valueY,
                  const T3& valueZ, const std::string& flags,
                  std::string unit = "",
                  const ShapeTeleplot& mshape = ShapeTeleplot()) {
    // Format
    std::string valueStr = formatValues(valueX, valueY, valueZ, mshape, flags);

    // Emit
    bool is3D = !mshape.getName().empty();

#ifdef TELEPLOT_USE_BUFFERING
    buffer(key, valueStr, flags, unit, is3D);
#else
    emit(formatPacket(key, valueStr, flags, unit, is3D));
#endif
  }

  template <typename T1, typename T2, typename T3>
  std::string formatValues(const T1& valueX, const T2& valueY, const T3& valueZ,
                           const ShapeTeleplot& mshape,
                           const std::string& flags) {
    if (!mshape.getName().empty()) {
      // valueX contains the timestamp
      return fmt::format("{}:{}", valueX, mshape.toString());
    } else {
      if (flags.find(TELEPLOT_FLAG_2D) != std::string::npos) {
        return fmt::format("{}:{}:{}", valueX, valueY, valueZ);
      } else {
        return fmt::format("{}:{}", valueX, valueY);
      }
    }
  }

  std::string formatPacket(const std::string& key, const std::string& values,
                           const std::string& flags, std::string unit,
                           bool is3D = false);

  void emit(const std::string& data);

#ifdef TELEPLOT_USE_BUFFERING
  void buffer(const std::string& key, const std::string& values,
              const std::string& flags, std::string unit, bool is3D = false) {
    // Make sure buffer exists
    if (bufferingMap_.find(key) == bufferingMap_.end()) {
      bufferingMap_[key] = "";
      bufferingFlushTimestampsUs_[key] = 0;
    }
    // Check that buffer isn't about to blow up
    size_t keySize = key.size() + 1; // +1 is the separator
    size_t valuesSize =
        bufferingMap_[key].size() + values.size() + 1; // +1 is the separator
    size_t flagSize = 1 + flags.size();                // +1 is the separator
    size_t nextSize = keySize + valuesSize + flagSize;
    if (nextSize > maxBufferingSize_) {
      flushBuffer(key, flags, unit, true, is3D); // Force flush
    }
    bufferingMap_[key] += values + ";";
    flushBuffer(key, flags, unit, false, is3D);
  }

  void flushBuffer(const std::string& key, const std::string& flags,
                   std::string unit, bool force, bool is3D = false) {
    // Flush the buffer if the frequency is reached
    int64_t nowUs = std::chrono::time_point_cast<std::chrono::microseconds>(
                        std::chrono::system_clock::now())
                        .time_since_epoch()
                        .count();
    int64_t elasped = nowUs - bufferingFlushTimestampsUs_[key];
    if (force || elasped >= static_cast<int64_t>(1e6 / bufferingFrequencyHz_)) {
      emit(formatPacket(key, bufferingMap_[key], flags, unit, is3D));
      bufferingMap_[key].clear();
      bufferingFlushTimestampsUs_[key] = nowUs;
    }
  }
  unsigned int bufferingFrequencyHz_ = 5;

  std::map<std::string, std::string> bufferingMap_;

  std::map<std::string, int64_t> bufferingFlushTimestampsUs_;
  size_t maxBufferingSize_ = 1432; // from
// https://github.com/statsd/statsd/blob/master/docs/metric_types.md
#endif
#ifdef TELEPLOT_USE_FREQUENCY
  std::map<std::string, int64_t> updateTimestampsUs_;
#endif

  // Backend instance
  std::unique_ptr<TeleplotBackend> backend_;
  bool enabled_ = false;
  int64_t millis_offset_ = 0;
  int64_t lastBufferingFlushTimestampUs_ = 0;
};

#endif
