#ifndef MADO_TEST_SHIM_ARDUINO_H
#define MADO_TEST_SHIM_ARDUINO_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define PROGMEM
#define PSTR(literal) (literal)
#define F(literal) (reinterpret_cast<const __FlashStringHelper*>(PSTR(literal)))
#define bit(b) (1UL << (b))
#define bitRead(value, b) (((value) >> (b)) & 0x01)
#define bitSet(value, b) ((value) |= (1UL << (b)))
#define bitClear(value, b) ((value) &= ~(1UL << (b)))
#define lowByte(w) ((uint8_t)((w) & 0xff))
#define highByte(w) ((uint8_t)((w) >> 8))
#define sq(x) ((x) * (x))
#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))

class __FlashStringHelper;

class IPAddress {
 public:
  IPAddress() = default;
  IPAddress(uint8_t first, uint8_t second, uint8_t third, uint8_t fourth)
      : bytes_{first, second, third, fourth} {
  }

  uint8_t operator[](int index) const {
    return bytes_[index & 3];
  }

  explicit operator uint32_t() const {
    uint32_t value = 0U;
    memcpy(&value, &bytes_[0], sizeof(value));
    return value;
  }

 private:
  uint8_t bytes_[4] = {};
};

inline uint8_t pgm_read_byte(const void* address) {
  return *static_cast<const uint8_t*>(address);
}

inline uint32_t pgm_read_dword(const void* address) {
  uint32_t value = 0U;
  memcpy(&value, address, sizeof(value));
  return value;
}

inline size_t strlen_P(const char* text) {
  return strlen(text);
}

inline int strcmp_P(const char* left, const char* right) {
  return strcmp(left, right);
}

inline char* strncpy_P(char* target, const char* source, size_t count) {
  return strncpy(target, source, count);
}

inline int strncmp_P(const char* left, const char* right, size_t count) {
  return strncmp(left, right, count);
}

inline void* memcpy_P(void* target, const void* source, size_t count) {
  return memcpy(target, source, count);
}

uint32_t millis();

#endif
