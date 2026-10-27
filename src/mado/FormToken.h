// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_FORM_TOKEN_H
#define MADO_FORM_TOKEN_H

#include "FormStatus.h"

namespace mado {

struct FormToken {
  FormStatus status;
  bool stopped_at_separator;
};

}

#endif
