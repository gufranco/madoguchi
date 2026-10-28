// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_API_ITEMS_ITEMS_CONTROLLER_H
#define REST_API_ITEMS_ITEMS_CONTROLLER_H

#include <Arduino.h>
#include <Madoguchi.h>
#include <mado/LogEntry.h>
#include <mado/Precondition.h>
#include <mado/Problem.h>
#include <mado/Reply.h>
#include <mado/check.h>
#include <mado/http_status.h>
#include <mado/if_match.h>
#include <mado/parse_idempotency_key.h>
#include <mado/send_problem.h>
#include <stdint.h>
#include <string.h>

#include "../../http/api.generated.h"
#include "../application/item_catalog.service.h"
#include "../domain/item.entity.h"
#include "item.etag.h"

namespace items {

constexpr uint32_t kMaxBody = 256U;

template <typename ExchangeT, typename RepositoryT, typename LedgerT, typename JournalT>
class ItemsController {
  using Catalog = ItemCatalog<RepositoryT>;

 public:
  static bool list(ExchangeT& exchange, const RepositoryT& repository) {
    bool written =
        send_json_head(exchange, mado::http::kOk, nullptr) && (exchange.print(F("[")) > 0U);
    bool first = true;
    for (uint8_t item_id = 1U; written && (item_id <= kCapacity); ++item_id) {
      const ItemLookup lookup = Catalog::find(repository, item_id);
      if (!lookup.found) {
        continue;
      }
      const bool separated = first || (exchange.print(F(",")) > 0U);
      written = separated && mado::check(lookup.item.id == item_id) &&
                rest_api::write_item(exchange, to_dto(lookup.item));
      first = false;
    }
    return written && (exchange.print(F("]")) > 0U);
  }

  static bool get(ExchangeT& exchange, const RepositoryT& repository, const uint8_t route) {
    const uint8_t item_id = requested_id(exchange, route);
    const ItemLookup lookup =
        mado::check(item_id != 0U) ? Catalog::find(repository, item_id) : ItemLookup{};
    if (!mado::check(lookup.found)) {
      return send_refusal(exchange, Outcome::kMissing);
    }
    return send_item(exchange, mado::http::kOk, lookup.item);
  }

  static bool create(ExchangeT& exchange, RepositoryT& repository, LedgerT& ledger,
                     JournalT& journal) {
    const char* const header = exchange.header(F("Idempotency-Key"));
    char key[mado::kIdempotencyKeySize] = {};
    if (!mado::check((header == nullptr) || mado::parse_idempotency_key(header, key))) {
      return mado::send_problem(exchange, bad_key());
    }
    rest_api::Item body = {};
    uint8_t present = 0U;
    const mado::Problem problem = rest_api::read_item_body(exchange, body, present, kMaxBody);
    if (!mado::check(problem.status == mado::http::kNoResponse)) {
      return mado::send_problem(exchange, problem);
    }
    const Result result = Catalog::add_once(repository, ledger, &key[0], draft_of(body));
    warn_unsaved(journal, result);
    const bool created =
        mado::check((result.outcome == Outcome::kDone) || (result.outcome == Outcome::kReplayed));
    if (!created) {
      return send_refusal(exchange, result.outcome);
    }
    char tag[kItemTagSize] = {};
    const bool tagged = mado::check(item_tag(result.item, tag));
    mado::Reply<ExchangeT> reply(exchange);
    const bool head = tagged && reply.status(mado::http::kCreated)
                                    .json()
                                    .no_store()
                                    .location(rest_api::get_item_url(), result.item.id)
                                    .header(F("ETag"), &tag[0])
                                    .end();
    return head && rest_api::write_item(exchange, to_dto(result.item));
  }

  static bool replace(ExchangeT& exchange, RepositoryT& repository, JournalT& journal,
                      const uint8_t route) {
    const uint8_t item_id = requested_id(exchange, route);
    const ItemLookup current = Catalog::find(repository, item_id);
    if (!mado::check(current.found)) {
      return send_refusal(exchange, Outcome::kMissing);
    }
    rest_api::Item body = {};
    uint8_t present = 0U;
    const mado::Problem problem = rest_api::read_item_body(exchange, body, present, kMaxBody);
    if (!mado::check(problem.status == mado::http::kNoResponse)) {
      return mado::send_problem(exchange, problem);
    }
    if (!mado::check(unchanged(exchange, current.item))) {
      return mado::send_problem(exchange, stale());
    }
    return answer(exchange, journal, Catalog::replace(repository, item_id, draft_of(body)));
  }

  static bool update(ExchangeT& exchange, RepositoryT& repository, JournalT& journal,
                     const uint8_t route) {
    const uint8_t item_id = requested_id(exchange, route);
    const ItemLookup current = Catalog::find(repository, item_id);
    if (!mado::check(current.found)) {
      return send_refusal(exchange, Outcome::kMissing);
    }
    rest_api::ItemPatch body = {};
    uint8_t present = 0U;
    const mado::Problem problem = rest_api::read_item_patch_body(exchange, body, present, kMaxBody);
    if (!mado::check(problem.status == mado::http::kNoResponse)) {
      return mado::send_problem(exchange, problem);
    }
    if (!mado::check(unchanged(exchange, current.item))) {
      return mado::send_problem(exchange, stale());
    }
    return answer(exchange, journal,
                  Catalog::change(repository, item_id, change_of(body, present)));
  }

  static bool remove(ExchangeT& exchange, RepositoryT& repository, JournalT& journal,
                     const uint8_t route) {
    const Result result = Catalog::remove(repository, requested_id(exchange, route));
    warn_unsaved(journal, result);
    if (!mado::check(result.outcome == Outcome::kDone)) {
      return send_refusal(exchange, result.outcome);
    }
    return exchange.send_empty(mado::http::kNoContent);
  }

 private:
  static_assert(sizeof(rest_api::Item::name) == kNameSize, "the schema and the entity agree");
  static_assert(sizeof(rest_api::ItemPatch::name) == kNameSize, "the schema and the entity agree");

  static bool answer(ExchangeT& exchange, JournalT& journal, const Result& result) {
    warn_unsaved(journal, result);
    const bool sound =
        mado::check((result.outcome != Outcome::kDone) || is_item_id(result.item.id));
    if (!(sound && mado::check(result.outcome == Outcome::kDone))) {
      return send_refusal(exchange, result.outcome);
    }
    return send_item(exchange, mado::http::kOk, result.item);
  }

  static_assert(mado::kIdempotencyKeySize == kKeySize, "the header parser and the record agree");

  static mado::Problem bad_key() {
    return mado::Problem{
        mado::http::kBadRequest,
        F("Idempotency-Key must be a quoted string of 1 to 64 characters"),
    };
  }

  static mado::Problem stale() {
    return mado::Problem{
        mado::http::kPreconditionFailed,
        F("the item changed since the ETag in If-Match"),
    };
  }

  static bool unchanged(ExchangeT& exchange, const Item& item) {
    char tag[kItemTagSize] = {};
    const bool tagged = mado::check(item_tag(item, tag));
    const mado::Precondition outcome =
        mado::if_match(exchange.header(F("If-Match")), tagged ? &tag[0] : nullptr);
    return mado::check(outcome != mado::Precondition::kFails);
  }

  static void warn_unsaved(JournalT& journal, const Result& result) {
    const bool lost = mado::check(!result.saved) && mado::check(result.outcome == Outcome::kDone);
    if (lost) {
      const bool logged = journal.event(mado::kLogWarn, F("the SD card refused a save"), millis());
      static_cast<void>(logged);
    }
  }

  static uint8_t requested_id(const ExchangeT& exchange, const uint8_t route) {
    uint8_t item_id = 0U;
    const bool parsed = mado::check(rest_api::route_path_id(exchange.path(), route, item_id));
    return (parsed && mado::check(is_item_id(item_id))) ? item_id : 0U;
  }

  static ItemDraft draft_of(const rest_api::Item& body) {
    const bool terminated = mado::check(body.name[kNameSize - 1U] == '\0');
    const bool unnumbered = mado::check(body.id == 0);
    ItemDraft draft = {};
    static_cast<void>(memcpy(&draft.name[0], &body.name[0], sizeof(draft.name)));
    draft.value = body.value;
    draft.on = body.on;
    return (terminated && unnumbered) ? draft : ItemDraft{};
  }

  static ItemChange change_of(const rest_api::ItemPatch& body, const uint8_t present) {
    const bool sound =
        mado::check((present & static_cast<uint8_t>(~rest_api::kItemPatchWritable)) == 0U);
    const bool terminated = mado::check(body.name[kNameSize - 1U] == '\0');
    ItemChange change = {};
    change.renames = sound && terminated && ((present & rest_api::kItemPatchName) != 0U);
    static_cast<void>(memcpy(&change.name[0], &body.name[0], sizeof(change.name)));
    change.revalues = sound && ((present & rest_api::kItemPatchValue) != 0U);
    change.value = body.value;
    change.switches = sound && ((present & rest_api::kItemPatchOn) != 0U);
    change.on = body.on;
    return change;
  }

  static rest_api::Item to_dto(const Item& item) {
    const bool numbered = mado::check(is_item_id(item.id));
    const bool named = mado::check(item.name[kNameSize - 1U] == '\0');
    rest_api::Item dto = {};
    dto.id = (numbered && named) ? static_cast<int32_t>(item.id) : 0;
    static_cast<void>(memcpy(&dto.name[0], &item.name[0], sizeof(dto.name)));
    dto.value = item.value;
    dto.on = item.on;
    return dto;
  }

  static bool send_json_head(ExchangeT& exchange, const uint16_t code, const char* const tag) {
    const bool fresh = mado::check(!exchange.status_sent());
    const bool success = mado::check((code == mado::http::kOk) || (code == mado::http::kCreated));
    if (!(fresh && success)) {
      return false;
    }
    mado::Reply<ExchangeT> reply(exchange);
    static_cast<void>(reply.status(code).json().no_store());
    if (tag != nullptr) {
      static_cast<void>(reply.header(F("ETag"), tag));
    }
    return reply.end();
  }

  static bool send_item(ExchangeT& exchange, const uint16_t code, const Item& item) {
    char tag[kItemTagSize] = {};
    const bool tagged = mado::check(item_tag(item, tag));
    const bool head = tagged && send_json_head(exchange, code, &tag[0]);
    return head && mado::check(is_item_id(item.id)) && rest_api::write_item(exchange, to_dto(item));
  }

  static bool send_refusal(ExchangeT& exchange, const Outcome outcome) {
    const bool known =
        mado::check(static_cast<uint8_t>(outcome) <= static_cast<uint8_t>(Outcome::kKeyReused));
    const bool refused = known && mado::check(outcome != Outcome::kDone);
    switch (outcome) {
      case Outcome::kMissing:
        return mado::send_problem(exchange,
                                  mado::Problem{mado::http::kNotFound, F("no item has this id")});
      case Outcome::kFull:
        return mado::send_problem(exchange, mado::Problem{
                                                mado::http::kInsufficientStorage,
                                                F("every item slot is taken"),
                                            });
      case Outcome::kNothing:
        return mado::send_problem(exchange, mado::Problem{
                                                mado::http::kUnprocessableContent,
                                                F("the body changes no property"),
                                            });
      case Outcome::kKeyReused:
        return mado::send_problem(exchange, mado::Problem{
                                                mado::http::kUnprocessableContent,
                                                F("this Idempotency-Key was sent before with a "
                                                  "different body"),
                                            });
      default:
        return refused && mado::send_problem(exchange, mado::Problem{
                                                           mado::http::kUnprocessableContent,
                                                           F("the name must not be empty"),
                                                       });
    }
  }
};

}

#endif
