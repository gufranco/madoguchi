#ifndef MADO_TEST_TEST_SECRETS_H
#define MADO_TEST_TEST_SECRETS_H

#include <Arduino.h>
#include <stdint.h>

namespace test_secrets {

const uint8_t kKey[32] PROGMEM = {
    1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
};
const uint8_t kZeroKey[32] PROGMEM = {};
const char kUser[] PROGMEM = "admin";
const uint8_t kSalt[16] PROGMEM = {
    100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115,
};
const uint8_t kHash[32] PROGMEM = {
    0xe3, 0x5c, 0xe1, 0x07, 0xbd, 0x17, 0x5a, 0x44, 0xe2, 0x5b, 0x78, 0x78, 0x47, 0x52, 0xd4, 0x01,
    0x6f, 0x23, 0x89, 0x16, 0xfe, 0x84, 0x13, 0xb0, 0xa9, 0x79, 0x8b, 0x78, 0xd3, 0x75, 0x1c, 0x56,
};
const char kPassword[] = "correct-horse-battery";
const char kToken[] =
    "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiJhZG1pbiIsInNlcyI6NywiZW5kIjo5MDB9."
    "9PQ6D5Wpr82DfguGZcQy8Q8cfHT24Tg0qGQ3xAtxJDQ";

struct TestSecrets {
  static const uint8_t* token_key() {
    return &kKey[0];
  }

  static const char* user_name() {
    return &kUser[0];
  }

  static const uint8_t* password_salt() {
    return &kSalt[0];
  }

  static const uint8_t* password_hash() {
    return &kHash[0];
  }

  static uint32_t password_iterations() {
    return 3U;
  }
};

struct UnprovisionedSecrets {
  static const uint8_t* token_key() {
    return &kZeroKey[0];
  }

  static const char* user_name() {
    return &kUser[0];
  }

  static const uint8_t* password_salt() {
    return &kSalt[0];
  }

  static const uint8_t* password_hash() {
    return &kHash[0];
  }

  static uint32_t password_iterations() {
    return 3U;
  }
};

}

#endif
