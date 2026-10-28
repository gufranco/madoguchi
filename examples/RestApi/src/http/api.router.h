// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_API_API_ROUTER_H
#define REST_API_API_ROUTER_H

#include <Arduino.h>
#include <Madoguchi.h>
#include <mado/Problem.h>
#include <mado/RequestLine.h>
#include <mado/ResponseSummary.h>
#include <mado/TokenClaims.h>
#include <mado/TokenStatus.h>
#include <mado/check.h>
#include <mado/http_status.h>
#include <mado/send_bearer_challenge.h>
#include <mado/send_problem.h>
#include <stdint.h>

#include "../audit/adapters/audit.controller.h"
#include "../identity/adapters/auth.controller.h"
#include "../items/adapters/items.controller.h"
#include "api.generated.h"

namespace api {

template <typename ClientT, typename BoardT>
class Router {
 public:
  using Exchange = mado::Exchange<ClientT, rest_api::Limits>;
  using Items = items::ItemsController<Exchange, typename BoardT::Repository,
                                       typename BoardT::Ledger, typename BoardT::Journal>;
  using Auth =
      identity::AuthController<Exchange, typename BoardT::Access, typename BoardT::Journal>;
  using Audit = audit::AuditController<Exchange, typename BoardT::Journal>;

  static void attach(BoardT& board) {
    wiring() = &board;
  }

  static void log_response(const mado::RequestLine& request,
                           const mado::ResponseSummary& response) {
    BoardT* const board = wiring();
    const bool logged =
        mado::check(board != nullptr) && board->journal.request(request, response, millis());
    static_cast<void>(logged);
  }

  static bool handle(Exchange& exchange, const uint8_t route) {
    const bool known = mado::check(route <= rest_api::kCardFile);
    BoardT* const board = wiring();
    if (!(known && mado::check(board != nullptr))) {
      return mado::send_problem(exchange, mado::Problem{mado::http::kNotFound, nullptr});
    }
    board->uptime.advance(millis());
    const uint32_t now = board->uptime.seconds();
    if (rest_api::route_needs_token(route) && (!authorized(exchange, *board, now))) {
      return true;
    }
    return dispatch(exchange, route, *board, now);
  }

 private:
  static BoardT*& wiring() {
    static BoardT* instance = nullptr;
    return instance;
  }

  static bool authorized(Exchange& exchange, const BoardT& board, const uint32_t now) {
    mado::TokenClaims claims = {};
    const mado::TokenStatus status =
        board.access.authorize(exchange.header(F("Authorization")), now, claims);
    if (mado::check(status == mado::TokenStatus::kValid)) {
      return true;
    }
    const bool sent = mado::send_bearer_challenge(exchange, status);
    static_cast<void>(sent);
    return false;
  }

  static bool dispatch(Exchange& exchange, const uint8_t route, BoardT& board, const uint32_t now) {
    switch (route) {
      case rest_api::kLogin: return Auth::login(exchange, board.access, board.journal, now);
      case rest_api::kListItems: return Items::list(exchange, board.repository);
      case rest_api::kCreateItem:
        return Items::create(exchange, board.repository, board.ledger, board.journal);
      case rest_api::kGetItem: return Items::get(exchange, board.repository, route);
      case rest_api::kReplaceItem:
        return Items::replace(exchange, board.repository, board.journal, route);
      case rest_api::kUpdateItem:
        return Items::update(exchange, board.repository, board.journal, route);
      case rest_api::kDeleteItem:
        return Items::remove(exchange, board.repository, board.journal, route);
      case rest_api::kReadLog: return Audit::read(exchange, board.journal);
      case rest_api::kClearLog: return Audit::clear(exchange, board.journal);
      default: return rest_api::send_card<BoardT::kReadMode>(exchange, board.card, route);
    }
  }
};

}

#endif
