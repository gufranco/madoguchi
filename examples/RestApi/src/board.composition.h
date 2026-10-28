// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_API_BOARD_COMPOSITION_H
#define REST_API_BOARD_COMPOSITION_H

#include <mado/UptimeClock.h>
#include <mado/check.h>
#include <stdint.h>

#include "audit/application/request_journal.service.h"
#include "identity/adapters/card_failure.store.h"
#include "identity/adapters/flash_secrets.authority.h"
#include "identity/application/access.service.h"
#include "items/adapters/card_item.repository.h"

template <typename SdT, typename SecretsT, uint16_t kRead, uint16_t kWrite>
struct Board {
  using Authority = identity::FlashSecretsAuthority<SecretsT>;
  using Failures = identity::CardFailureLog<SdT, kRead, kWrite>;
  using Access = identity::AccessService<Authority, Failures>;
  using Repository = items::CardItemRepository<SdT, kRead, kWrite>;
  using Journal = audit::RequestJournal<SdT, kRead, kWrite>;

  static constexpr uint16_t kReadMode = kRead;

  Authority authority;
  Failures failures{nullptr};
  Access access{authority, failures};
  Repository repository;
  Journal journal;
  mado::UptimeClock uptime;
  SdT* card = nullptr;
};

template <typename SdT, typename SecretsT, uint16_t kRead, uint16_t kWrite>
bool begin_board(Board<SdT, SecretsT, kRead, kWrite>& board, const uint32_t session, SdT& sd_card,
                 const uint8_t chip_select) {
  board.card = sd_card.begin(chip_select) ? &sd_card : nullptr;
  board.failures = typename Board<SdT, SecretsT, kRead, kWrite>::Failures(board.card);
  board.access.begin(session);
  const uint8_t loaded = board.repository.attach(board.card);
  board.journal.attach(board.card, session);
  return mado::check(loaded <= items::kCapacity) && mado::check(board.card != nullptr);
}

#endif
