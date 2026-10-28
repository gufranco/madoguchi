// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_FILES_FILES_FILE_CARD_PORT_H
#define REST_FILES_FILES_FILE_CARD_PORT_H

#include <mado/reference_to.h>
#include <mado/same_type.h>
#include <stddef.h>
#include <stdint.h>

namespace files {

template <typename CardT, typename FileT>
struct FileCardPort {
  using Begin = decltype(mado::reference_to<CardT>().begin(uint8_t{}));
  using Exists = decltype(mado::reference_to<CardT>().exists(static_cast<const char*>(nullptr)));
  using Remove = decltype(mado::reference_to<CardT>().remove(static_cast<const char*>(nullptr)));
  using Open =
      decltype(mado::reference_to<CardT>().open(static_cast<const char*>(nullptr), uint16_t{}));
  using Next = decltype(mado::reference_to<FileT>().openNextFile());
  using Name = decltype(mado::reference_to<FileT>().getName(static_cast<char*>(nullptr), size_t{}));

  static_assert(mado::same_type<Begin, bool>::value, "begin(chip_select) mounts the card");
  static_assert(mado::same_type<Exists, bool>::value, "exists(path) finds a file");
  static_assert(mado::same_type<Remove, bool>::value, "remove(path) deletes a file");
  static_assert(mado::same_type<Open, FileT>::value, "open(path, mode) returns the file type");
  static_assert(mado::same_type<Next, FileT>::value, "openNextFile() walks a folder");
  static_assert(mado::same_type<Name, size_t>::value, "getName(out, size) copies a long name");

  static constexpr bool kSatisfied = true;
};

}

#endif
