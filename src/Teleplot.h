// Teleplot
// Source: https://github.com/nesnes/teleplot

#ifndef TELEPLOT_H
#define TELEPLOT_H

#ifdef EMBEDDED_TELEPLOT
#include <Arduino.h>
#include <IPAddress.h>
#include <WiFiUdp.h>
 WiFiUDP udp;
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

#include <chrono>
#include "fmt.h"
#include <functional>
#include <map>
#include <unistd.h>

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

  Teleplot() : enabled_(false) {}
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
  }

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

  void begin(WriteCallback callback, int64_t millis_offset = 0, bool enabled = true) {
    enabled_ = enabled;
    millis_offset_ = millis_offset;
    writeCallback_ = callback;
    prefix_ = ">";
    suffix_ = "\n";
    section_ = "\xA7";
  }

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
  void update(const std::string& key, const T& value, std::string unit = "",
              std::string flags = TELEPLOT_FLAG_DEFAULT) {
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
    int64_t timeStamp = nowMs + millis_offset_;
    updateData(key, timeStamp, value, 0, flags, unit);
  }

  template <typename T1, typename T2>
  void update2D(const std::string& key, const T1& valueX, const T2& valueY,
                std::string flags = TELEPLOT_FLAG_2D) {
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
    int64_t timeStamp = nowMs + millis_offset_;
    updateData(key, valueX, valueY, timeStamp, flags);
  }

  void update3D(const ShapeTeleplot& mshape,
                std::string flags = TELEPLOT_FLAG_DEFAULT) {

    int64_t nowUs = std::chrono::time_point_cast<std::chrono::microseconds>(
                        std::chrono::system_clock::now())
                        .time_since_epoch()
                        .count();
    double nowMs = static_cast<double>(nowUs) / 1000.0;
    updateData(mshape.getName(), nowMs, NULL, NULL, flags, "", mshape);
  }

  void update3D_ms(const ShapeTeleplot& mshape, unsigned long nowMs,
                   std::string flags = TELEPLOT_FLAG_DEFAULT) {
    int64_t timeStamp = nowMs + millis_offset_;
    updateData(mshape.getName(), timeStamp, NULL, NULL, flags, "", mshape);
  }

  void log(const std::string& log) {
    int64_t nowMs = std::chrono::time_point_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now())
                        .time_since_epoch()
                        .count();
    emit(">" + std::to_string(nowMs) + ":" + log + suffix_);
  }

  void log_ms(const std::string& log, unsigned long nowMs) {
    int64_t timeStamp = nowMs + millis_offset_;
    // emit(prefix_ + std::to_string(timeStamp) + ":" + log + suffix_);
    emit(">" + std::to_string(timeStamp) + ":" + log + suffix_);
  }

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
                           bool is3D = false) {
    std::string unitFormatted = (unit == "") ? "" : section_ + unit;
    return fmt::format("{}{}{}:{}{}|{}{}", prefix_, is3D ? "3D|" : "", key, values,
                  unitFormatted, flags, suffix_);
  }

  void emit(const std::string& data) {
    if (!enabled_)
      return;
    if (writeCallback_) {
      writeCallback_((const uint8_t *)data.c_str(), data.size());
      return;
    }
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

  bool enabled_;
  int sockfd_ = -1;
  int32_t port_ = -1;
  WriteCallback writeCallback_ = nullptr;
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
