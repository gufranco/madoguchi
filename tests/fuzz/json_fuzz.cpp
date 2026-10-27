#include <stddef.h>
#include <stdint.h>

#include <cstdlib>

#include "../../examples/RestApi/generated_api.h"
#include "FuzzTarget.h"
#include "mado/JsonReader.h"
#include "mado/JsonStatus.h"
#include "mado/MemorySource.h"

namespace {

constexpr size_t kMaxInput = 0xFFFFU;

void check_item(const rest_api::Item& item, const uint8_t present) {
  const bool terminated = item.name[sizeof(item.name) - 1U] == '\0';
  const bool masked = (present & static_cast<uint8_t>(~rest_api::kItemWritable)) == 0U;
  if ((!terminated) || (!masked)) {
    std::abort();
  }
}

}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size > kMaxInput) {
    return 0;
  }
  mado::MemorySource source(data, static_cast<uint16_t>(size));
  mado::JsonReader<mado::MemorySource> reader(source);
  rest_api::Item item = {};
  uint8_t present = 0U;
  const mado::JsonStatus status = rest_api::read_item(reader, item, present);
  static_cast<void>(status);
  check_item(item, present);
  return 0;
}
