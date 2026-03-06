// Teleplot
// Source: https://github.com/nesnes/teleplot
// #define TELEPLOT_DISABLE

#ifndef TELEPLOT_H
#define TELEPLOT_H

#ifdef EMBEDDED_TELEPLOT
#include <Arduino.h>
#include <IPAddress.h>
// #include <StreamLib.h>
#include <WiFiUdp.h>

// workaround https://github.com/fmtlib/fmt/pull/2440
// #undef B1
 WiFiUDP udp;
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

#include <chrono>
// #include <fmt/core.h>
// #include <fmt/format.h>
#include "fmt.h"

#include <map>

#include <unistd.h>

// Enable/Disable implementation optimisations:

#define TELEPLOT_USE_BUFFERING // Allows to group updates sent, but will use
// a dynamic buffer map

#define TELEPLOT_FLAG_DEFAULT ""
#define TELEPLOT_FLAG_NOPLOT "np"
#define TELEPLOT_FLAG_2D "xy"
#define TELEPLOT_FLAG_TEXT "text"

class ShapeTeleplot {
public:
  class ShapeValue {
  public:
    ShapeValue() : isSet(false), value(0){};
    ShapeValue(double val) : isSet(true), value(val){};

    bool isSet;
    double value;
    std::string valueRounded() const { return roundValue(value, 3); }

  private:
    std::string roundValue(const double value,
                           const unsigned short precision) const {
      std::string value_str = std::to_string(value);
      int res_length = value_str.length();

      int i = 0;
      bool stop = false;

      while (i < res_length && !stop) {
        if (value_str[i] == '.') {
          int u = i + precision;
          if (u + 1 < value_str.length()) {
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

  ShapeTeleplot(){};

  ShapeTeleplot(std::string const &name, std::string const &type,
                std::string const &color = "")
      : _name(name), _type(type), _color(color){};

  std::string const &getName() const { return _name; }

  ShapeTeleplot &setPos(ShapeValue const posX, ShapeValue const posY = {},
                        ShapeValue const posZ = {}) {
    _posX = posX;
    _posY = posY;
    _posZ = posZ;

    return *this;
  }

  ShapeTeleplot &setRot(ShapeValue const rotX, ShapeValue const rotY = {},
                        ShapeValue const rotZ = {},
                        ShapeValue const rotW = {}) {
    _rotX = rotX;
    _rotY = rotY;
    _rotZ = rotZ;
    _rotW = rotW;

    return *this;
  }

  ShapeTeleplot &setCubeProperties(ShapeValue const height,
                                   ShapeValue const width = {},
                                   ShapeValue const depth = {}) {
    _height = height;
    _width = width;
    _depth = depth;

    return *this;
  }

  ShapeTeleplot &setSphereProperties(ShapeValue const radius,
                                     ShapeValue const precision) {
    _radius = radius;
    _precision = precision;

    return *this;
  }

  std::string toString() const {
    std::string result;

    result.append(fmt::format("S:{}", _type));
    if (_color != "") {
      result.append(fmt::format(":C:{}", _color));
    }

    if (_posX.isSet || _posY.isSet || _posZ.isSet) {
      result.append(fmt::format(":P:"));
      if (_posX.isSet) {
        result.append(fmt::format("{}", _posX.valueRounded()));
      }
      result.append(fmt::format(":"));

      if (_posY.isSet) {
        result.append(fmt::format("{}", _posY.valueRounded()));
      }
      result.append(fmt::format(":"));
      if (_posZ.isSet) {
        result.append(fmt::format("{}", _posZ.valueRounded()));
      }
    }

    if (_rotX.isSet || _rotY.isSet || _rotZ.isSet || _rotW.isSet) {
      result.append(fmt::format("{}", _rotW.isSet ? ":Q:" : ":R:"));

      if (_rotX.isSet) {
        result.append(fmt::format("{}", _rotX.valueRounded()));
      }
      result.append(fmt::format(":"));
      if (_rotY.isSet) {
        result.append(fmt::format("{}", _rotY.valueRounded()));
      }
      result.append(fmt::format(":"));
      if (_rotZ.isSet) {
        result.append(fmt::format("{}", _rotZ.valueRounded()));
      }
      result.append(fmt::format(":"));
      if (_rotW.isSet) {
        result.append(fmt::format("{}", _rotW.valueRounded()));
      }
    }

    if (_type == "sphere") {
      if (_radius.isSet) {
        result.append(fmt::format(":RA:{}", _radius.valueRounded()));
      }
      if (_precision.isSet) {
        result.append(fmt::format(":P:{}", _precision.valueRounded()));
      }
    }

    if (_type == "cube") {
      if (_height.isSet) {
        result.append(fmt::format(":H:{}", _height.valueRounded()));
      }
      if (_width.isSet) {
        result.append(fmt::format(":W:{}", _width.valueRounded()));
      }
      if (_depth.isSet) {
        result.append(fmt::format(":D:{}", _depth.valueRounded()));
      }
    }
    return result;
  }

private:
  const std::string _name;
  const std::string _type;
  const std::string _color;

  ShapeValue _posX;
  ShapeValue _posY;
  ShapeValue _posZ;

  ShapeValue _rotX;
  ShapeValue _rotY;
  ShapeValue _rotZ;
  ShapeValue _rotW;

  ShapeValue _height;
  ShapeValue _width;
  ShapeValue _depth;

  ShapeValue _radius;
  ShapeValue _precision;
};

class Teleplot {
public:
  Teleplot() { enabled_ = false; };
#ifdef ARDUINO
  void begin(IPAddress address, uint16_t port = 47269,
             int64_t millis_offset = 0, bool enabled = true) {
    address_ = address;
    port_ = port;
    enabled_ = enabled;
    millis_offset_ = millis_offset;
    prefix_ = "";
    suffix_ = "";
    section_ = "§";
  }
#else
  void begin(std::string address, unsigned int port = 47269,
             bool enabled = true) {
    address_ = address;
    port_ = port;
    enabled_ = enabled;
    // Create UDP socket
    sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
    serv_.sin_family = AF_INET;
    serv_.sin_port = htons(port);
    serv_.sin_addr.s_addr = inet_addr(address_.c_str());
  };

  void begin(int fd = 1, bool enabled = true) {
    fd_ = fd;
    enabled_ = enabled;
    port_ = -1;
    prefix_ = ">";
    suffix_ = "\n";
    section_ = "\xA7";
  };
#endif

#ifdef EMBEDDED_TELEPLOT
  void begin(Stream *stream, int64_t millis_offset = 0, bool enabled = true) {
    enabled_ = enabled;
    millis_offset_ = millis_offset;
    stream_ = stream;
    prefix_ = ">";
    suffix_ = "\n";
    section_ = "\xA7";
  };
#endif

  ~Teleplot() = default;

#ifndef EMBEDDED_TELEPLOT
  // Static localhost instance
  // makes no sense on embedded
  static Teleplot &localhost() {
    static Teleplot teleplot;

    teleplot.begin("127.0.0.1");
    return teleplot;
  }
#endif
  template <typename T>
  void update(std::string const &key, T const &value, std::string unit = "",
              std::string flags = TELEPLOT_FLAG_DEFAULT) {
    int64_t nowUs = std::chrono::time_point_cast<std::chrono::microseconds>(
                        std::chrono::system_clock::now())
                        .time_since_epoch()
                        .count();
    double nowMs = static_cast<double>(nowUs) / 1000.0;
    updateData(key, nowMs, value, 0, flags, unit);
  }

  template <typename T>
  void update_ms(std::string const &key, unsigned long nowMs, T const &value,
                 std::string unit = "",
                 std::string flags = TELEPLOT_FLAG_DEFAULT) {
    int64_t timeStamp = nowMs + millis_offset_;
    updateData(key, timeStamp, value, 0, flags, unit);
  }

  template <typename T1, typename T2>
  void update2D(std::string const &key, T1 const &valueX, T2 const &valueY,
                std::string flags = TELEPLOT_FLAG_2D) {
    int64_t nowUs = std::chrono::time_point_cast<std::chrono::microseconds>(
                        std::chrono::system_clock::now())
                        .time_since_epoch()
                        .count();
    double nowMs = static_cast<double>(nowUs) / 1000.0;
    updateData(key, valueX, valueY, nowMs, flags);
  }

  template <typename T1, typename T2>
  void update2D_ms(std::string const &key, unsigned long nowMs,
                   T1 const &valueX, T2 const &valueY,
                   std::string flags = TELEPLOT_FLAG_2D) {
    int64_t timeStamp = nowMs + millis_offset_;
    updateData(key, valueX, valueY, timeStamp, flags);
  }

  void update3D(ShapeTeleplot const &mshape,
                std::string flags = TELEPLOT_FLAG_DEFAULT) {

    int64_t nowUs = std::chrono::time_point_cast<std::chrono::microseconds>(
                        std::chrono::system_clock::now())
                        .time_since_epoch()
                        .count();
    double nowMs = static_cast<double>(nowUs) / 1000.0;
    updateData(mshape.getName(), nowMs, NULL, NULL, flags, "", mshape);
  }

  void update3D_ms(ShapeTeleplot const &mshape, unsigned long nowMs,
                   std::string flags = TELEPLOT_FLAG_DEFAULT) {
    int64_t timeStamp = nowMs + millis_offset_;
    updateData(mshape.getName(), timeStamp, NULL, NULL, flags, "", mshape);
  }

  void log(std::string const &log) {
    int64_t nowMs = std::chrono::time_point_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now())
                        .time_since_epoch()
                        .count();
    emit(">" + std::to_string(nowMs) + ":" + log + suffix_);
  }

  void log_ms(std::string const &log, unsigned long nowMs) {
    int64_t timeStamp = nowMs + millis_offset_;
    // emit(prefix_ + std::to_string(timeStamp) + ":" + log + suffix_);
    emit(">" + std::to_string(timeStamp) + ":" + log + suffix_);
  }

private:
  template <typename T1, typename T2, typename T3>
  void updateData(std::string const &key, T1 const &valueX, T2 const &valueY,
                  T3 const &valueZ, std::string const &flags,
                  std::string unit = "",
                  ShapeTeleplot const &mshape = ShapeTeleplot()) {
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
  std::string formatValues(T1 const &valueX, T2 const &valueY, T3 const &valueZ,
                           ShapeTeleplot const &mshape,
                           std::string const &flags) {
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

  std::string formatPacket(std::string const &key, std::string const &values,
                           std::string const &flags, std::string unit,
                           bool is3D = false) {
    std::string unitFormatted = (unit == "") ? "" : section_ + unit;
    // std::string unitFormatted = (unit == "") ? "" : "\xA7" + unit;
    return fmt::format("{}{}{}:{}{}|{}{}", prefix_, is3D ? "3D|" : "", key, values,
                  unitFormatted, flags, suffix_);
  }

  void emit(std::string const &data) {
    if (!enabled_)
      return;
#ifdef EMBEDDED_TELEPLOT
    if (stream_) {
      stream_->write(data.c_str(), data.size());
      return;
    }
    if (port_ > -1) {
      udp.beginPacket(address_, port_);
      udp.write((const uint8_t *)data.c_str(), data.size());
      udp.flush();
      udp.endPacket();
    }
#else
    if (port_ > -1) {
      (void)sendto(sockfd_, data.c_str(), data.size(), 0,
                   (struct sockaddr *)&serv_, sizeof(serv_));
    } else {
      (void)write(fd_, data.c_str(), data.size());
    }
#endif
  }

#ifdef TELEPLOT_USE_BUFFERING
  void buffer(std::string const &key, std::string const &values,
              std::string const &flags, std::string unit, bool is3D = false) {
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

  void flushBuffer(std::string const &key, std::string const &flags,
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

  bool enabled_;
  int sockfd_ = -1;
  int32_t port_ = -1;
#ifdef EMBEDDED_TELEPLOT
  Stream *stream_ = NULL;
  IPAddress address_;
#else
  std::string address_;
  sockaddr_in serv_;
  bool use_fd_;
  int fd_;

#endif
  int64_t millis_offset_ = 0;

  std::string prefix_;
  std::string suffix_;
  std::string section_;

  int64_t lastBufferingFlushTimestampUs_ = 0;
};

#endif
