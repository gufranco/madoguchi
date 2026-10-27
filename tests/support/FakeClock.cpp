#include "FakeClock.h"

#include <Arduino.h>

namespace {

uint32_t g_now_ms = 0U;

}

namespace fake_clock {

void set(uint32_t now_ms) {
  g_now_ms = now_ms;
}

void advance(uint32_t delta_ms) {
  g_now_ms += delta_ms;
}

uint32_t now() {
  return g_now_ms;
}

}

uint32_t millis() {
  return fake_clock::now();
}
