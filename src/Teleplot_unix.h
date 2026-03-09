#ifndef TELEPLOT_UNIX_H
#define TELEPLOT_UNIX_H

#ifndef TELEPLOT_ARDUINO

#include "Teleplot_backend.h"
#include <string>
#include <sys/socket.h>
#include <netinet/in.h>

/**
 * Unix/POSIX backend for Teleplot using UDP sockets or file descriptors.
 * Handles initialization with hostname/IP address, and transmission via UDP or file descriptor.
 */
class TeleplotUnixBackend : public TeleplotBackend {
public:
  TeleplotUnixBackend();
  ~TeleplotUnixBackend();

  /**
   * Initialize backend for UDP transmission to a remote host.
   * @param address Target hostname or IP address.
   * @param port Target UDP port (default: 47269).
   */
  void begin(const std::string& address, unsigned int port = 47269);

  /**
   * Initialize backend for file descriptor (stdout, stderr, or socket).
   * @param fd File descriptor to write to (default: 1 for stdout).
   */
  void begin(int fd);

  // TeleplotBackend interface
  int sendData(const uint8_t* data, size_t len) override;
  const char* getPrefix() const override { return ">"; }
  const char* getSuffix() const override { return "\n"; }
  char getSectionSeparator() const override { return 0xA7; }

private:
  int mode_ = -1;  // -1=uninitialized, 0=UDP socket, 1=file descriptor
  int sockfd_ = -1;
  int fd_ = -1;
  std::string address_;
  unsigned int port_ = 47269;
  struct sockaddr_in serv_addr_;
};

#endif // !TELEPLOT_ARDUINO

#endif // TELEPLOT_UNIX_H
