#ifndef MADO_TEST_FAKE_ETHERNET_H
#define MADO_TEST_FAKE_ETHERNET_H

#include <Arduino.h>
#include <stdint.h>

class FakeEthernet {
 public:
  explicit FakeEthernet(bool dhcp_answers) : dhcp_answers_(dhcp_answers) {
  }

  int begin(const uint8_t* mac, unsigned long timeout, unsigned long response_timeout) {
    dhcp_calls_ += 1U;
    last_timeout_ = timeout;
    last_response_timeout_ = response_timeout;
    mac_first_ = mac[0];
    return dhcp_answers_ ? 1 : 0;
  }

  void begin(const uint8_t* mac, const IPAddress& fixed) {
    static_calls_ += 1U;
    mac_first_ = mac[0];
    address_ = fixed;
  }

  uint32_t dhcp_calls() const {
    return dhcp_calls_;
  }

  uint32_t static_calls() const {
    return static_calls_;
  }

  unsigned long last_timeout() const {
    return last_timeout_;
  }

  unsigned long last_response_timeout() const {
    return last_response_timeout_;
  }

  const IPAddress& address() const {
    return address_;
  }

 private:
  bool dhcp_answers_;
  uint32_t dhcp_calls_ = 0U;
  uint32_t static_calls_ = 0U;
  unsigned long last_timeout_ = 0U;
  unsigned long last_response_timeout_ = 0U;
  uint8_t mac_first_ = 0U;
  IPAddress address_;
};

#endif
