#ifndef MADO_TEST_FUZZ_TARGET_H
#define MADO_TEST_FUZZ_TARGET_H

#include <stddef.h>
#include <stdint.h>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

#endif
