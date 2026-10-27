#ifndef MADO_TEST_POSIX_CLIENT_H
#define MADO_TEST_POSIX_CLIENT_H

#include <stddef.h>
#include <stdint.h>

class PosixClient {
 public:
  explicit PosixClient(int descriptor);

  int available() const;
  int read();
  int read(uint8_t* buffer, size_t size);
  size_t write(uint8_t value);
  size_t write(const uint8_t* buffer, size_t size);
  uint8_t connected() const;
  void stop();

 private:
  int descriptor_;
};

#endif
