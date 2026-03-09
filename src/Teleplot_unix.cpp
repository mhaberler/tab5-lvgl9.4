#ifndef TELEPLOT_ARDUINO

#include "Teleplot_unix.h"
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>

TeleplotUnixBackend::TeleplotUnixBackend()
  : enabled_(false), mode_(-1), sockfd_(-1), fd_(-1), port_(47269) {
  std::memset(&serv_addr_, 0, sizeof(serv_addr_));
}

TeleplotUnixBackend::~TeleplotUnixBackend() {
  if (sockfd_ >= 0) {
    close(sockfd_);
    sockfd_ = -1;
  }
}

void TeleplotUnixBackend::begin(const std::string& address, unsigned int port) {
  address_ = address;
  port_ = port;
  mode_ = 0;  // UDP socket mode

  // Create UDP socket
  sockfd_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd_ < 0) {
    return;  // Socket creation failed
  }

  // Configure server address
  serv_addr_.sin_family = AF_INET;
  serv_addr_.sin_port = htons(port);
  serv_addr_.sin_addr.s_addr = inet_addr(address_.c_str());
}

void TeleplotUnixBackend::begin(int fd) {
  fd_ = fd;
  mode_ = 1;  // File descriptor mode
  sockfd_ = -1;
}

int TeleplotUnixBackend::sendData(const uint8_t* data, size_t len) {
  if (mode_ == 0 && sockfd_ >= 0) {
    // UDP socket mode
    ssize_t sent = sendto(sockfd_, data, len, 0,
                          (struct sockaddr*)&serv_addr_, sizeof(serv_addr_));
    return (sent < 0) ? -1 : sent;
  }

  if (mode_ == 1 && fd_ >= 0) {
    // File descriptor mode
    ssize_t written = write(fd_, data, len);
    return (written < 0) ? -1 : written;
  }

  return 0;
}

#endif // !TELEPLOT_ARDUINO
