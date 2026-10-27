#include <stddef.h>
#include <stdint.h>

#include <filesystem>
#include <fstream>
#include <ios>
#include <iostream>
#include <iterator>
#include <vector>

#include "FuzzTarget.h"

namespace {

std::vector<uint8_t> read_file(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  return std::vector<uint8_t>(std::istreambuf_iterator<char>(stream),
                              std::istreambuf_iterator<char>());
}

}

int main(int argc, const char* const* argv) {
  if (argc != 2) {
    std::cerr << "usage: " << argv[0] << " <corpus directory>\n";
    return 2;
  }
  const std::filesystem::path corpus(argv[1]);
  size_t replayed = 0U;
  for (const auto& entry : std::filesystem::directory_iterator(corpus)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const std::vector<uint8_t> input = read_file(entry.path());
    const int result = LLVMFuzzerTestOneInput(input.data(), input.size());
    if (result != 0) {
      std::cerr << entry.path() << " returned " << result << "\n";
      return 1;
    }
    ++replayed;
  }
  std::cout << "replayed " << replayed << " inputs\n";
  return (replayed > 0U) ? 0 : 1;
}
