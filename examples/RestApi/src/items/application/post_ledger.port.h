// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_API_ITEMS_POST_LEDGER_PORT_H
#define REST_API_ITEMS_POST_LEDGER_PORT_H

#include <mado/reference_to.h>
#include <mado/same_type.h>

#include "../domain/post_record.value.h"

namespace items {

template <typename LedgerT>
struct PostLedgerPort {
  using Recall = decltype(mado::reference_to<const LedgerT>().recall(
      mado::reference_to<const char*>(), mado::reference_to<PostRecord>()));
  using Remember =
      decltype(mado::reference_to<LedgerT>().remember(mado::reference_to<const PostRecord>()));
  static_assert(mado::same_type<Recall, bool>::value,
                "recall(key, PostRecord&) copies the record kept under the key and returns "
                "whether one was");
  static_assert(mado::same_type<Remember, bool>::value,
                "remember(const PostRecord&) keeps the record and returns whether it was kept");
  static constexpr bool kSatisfied = true;
};

}

#endif
