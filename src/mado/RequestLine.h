// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_REQUEST_LINE_H
#define MADO_REQUEST_LINE_H

#include "HttpVersion.h"
#include "Method.h"

namespace mado {

struct RequestLine {
  Method method;
  const char* path;
  const char* query;
  bool valid;
  HttpVersion version;
};

}

#endif
