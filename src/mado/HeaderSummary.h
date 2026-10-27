// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_HEADER_SUMMARY_H
#define MADO_HEADER_SUMMARY_H

#include <stdint.h>

#include "HeaderStatus.h"

namespace mado {

struct HeaderSummary {
  HeaderStatus status;
  uint32_t content_length;
  bool has_content_length;
  bool expect_continue;
  bool has_host;
  bool host_trusted;
};

}

#endif
