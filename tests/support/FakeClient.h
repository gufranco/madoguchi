#ifndef MADO_TEST_FAKE_CLIENT_H
#define MADO_TEST_FAKE_CLIENT_H

#include <stddef.h>
#include <stdint.h>

#include <string>

#include <Arduino.h>

class FakeClient {
 public:
  explicit FakeClient(std::string input);

  int available() const;
  int read();
  int read(uint8_t* buffer, size_t size);
  size_t write(uint8_t value);
  size_t write(const uint8_t* buffer, size_t size);
  uint8_t connected() const;
  IPAddress remoteIP() const;
  void stop();

  void disconnect();
  void limit_available(size_t max_bytes);
  void limit_write(size_t max_bytes);
  void overstate_block_reads(int extra);
  void force_single_read(int value);
  void close_when_drained();
  void fail_writes_after(size_t total_bytes);
  void delay_each_read(uint32_t delay_ms);

  const std::string& output() const;
  bool stopped() const;
  uint32_t single_byte_reads() const;
  uint32_t block_reads() const;
  uint32_t write_calls() const;
  size_t unread() const;

 private:
  std::string input_;
  size_t position_;
  size_t max_available_;
  size_t max_write_;
  int read_overstatement_;
  int forced_single_read_;
  bool single_read_forced_;
  bool close_when_drained_;
  size_t write_budget_;
  std::string output_;
  bool connected_;
  IPAddress remote_{192, 168, 1, 50};
  bool stopped_;
  uint32_t single_byte_reads_;
  uint32_t block_reads_;
  uint32_t write_calls_;
  uint32_t read_delay_ms_;
};

#endif
