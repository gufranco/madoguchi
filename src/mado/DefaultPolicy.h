// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef MADO_DEFAULT_POLICY_H
#define MADO_DEFAULT_POLICY_H

#include <stdint.h>

#include "RequestLine.h"
#include "ResponseSummary.h"
#include "check.h"
#include "http_status.h"

namespace mado {

struct DefaultPolicy {
  static constexpr bool kTrustedHostsOnly = false;

  static void on_response(const RequestLine& request, const ResponseSummary& response) {
    const bool valid =
        check(response.status >= http::kFirstValid) && check(response.status <= http::kLastValid);
    static_cast<void>(request);
    static_cast<void>(valid);
  }
};

}

#endif
