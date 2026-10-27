#ifndef MADO_TEST_FAKE_CLOCK_H
#define MADO_TEST_FAKE_CLOCK_H

#include <stdint.h>

namespace fake_clock {

void set(uint32_t now_ms);
void advance(uint32_t delta_ms);
uint32_t now();

}

#endif
