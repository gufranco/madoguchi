// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_FILES_FILES_FILE_PATH_VALUE_H
#define REST_FILES_FILES_FILE_PATH_VALUE_H

#include <mado/check.h>
#include <mado/valid_card_name.h>
#include <stdint.h>

namespace files {

constexpr uint8_t kPathSize = mado::kCardNameSize + 1U;

struct FilePath {
  char text[kPathSize];
};

inline bool is_file_path(const FilePath& path) {
  const bool rooted = mado::check(path.text[0] == '/');
  return rooted && mado::check(mado::valid_card_name(&path.text[1]));
}

}

#endif
