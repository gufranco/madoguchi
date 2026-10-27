#include "PosixClient.h"

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

PosixClient::PosixClient(const int descriptor) : descriptor_(descriptor) {
}

int PosixClient::available() const {
  int pending = 0;
  const bool open = descriptor_ >= 0;
  return (open && (ioctl(descriptor_, FIONREAD, &pending) == 0)) ? pending : 0;
}

int PosixClient::read() {
  uint8_t value = 0U;
  return (read(&value, 1U) == 1) ? value : -1;
}

int PosixClient::read(uint8_t* const buffer, const size_t size) {
  const ssize_t got = (descriptor_ >= 0) ? recv(descriptor_, buffer, size, 0) : -1;
  return (got > 0) ? static_cast<int>(got) : -1;
}

size_t PosixClient::write(const uint8_t value) {
  return write(&value, 1U);
}

size_t PosixClient::write(const uint8_t* const buffer, const size_t size) {
  const ssize_t sent = (descriptor_ >= 0) ? send(descriptor_, buffer, size, 0) : -1;
  return (sent > 0) ? static_cast<size_t>(sent) : 0U;
}

uint8_t PosixClient::connected() const {
  uint8_t probe = 0U;
  const bool open = descriptor_ >= 0;
  const ssize_t peeked = open ? recv(descriptor_, &probe, 1U, MSG_PEEK | MSG_DONTWAIT) : 0;
  return (open && (peeked != 0)) ? 1U : 0U;
}

void PosixClient::stop() {
  if (descriptor_ >= 0) {
    close(descriptor_);
  }
  descriptor_ = -1;
}
