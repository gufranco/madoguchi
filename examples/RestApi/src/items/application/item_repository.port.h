// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_API_ITEMS_ITEM_REPOSITORY_PORT_H
#define REST_API_ITEMS_ITEM_REPOSITORY_PORT_H

#include <mado/reference_to.h>
#include <mado/same_type.h>
#include <stdint.h>

#include "../domain/item.entity.h"

namespace items {

struct Written {
  bool done;
  bool saved;
};

template <typename RepositoryT>
struct ItemRepositoryPort {
  using Insert = decltype(mado::reference_to<RepositoryT>().insert(
      mado::reference_to<const ItemDraft>(), mado::reference_to<uint8_t>()));
  using Find =
      decltype(mado::reference_to<const RepositoryT>().find(uint8_t{}, mado::reference_to<Item>()));
  using Store = decltype(mado::reference_to<RepositoryT>().store(mado::reference_to<const Item>()));
  using Erase = decltype(mado::reference_to<RepositoryT>().erase(uint8_t{}));

  static_assert(mado::same_type<Insert, Written>::value,
                "insert(draft, id) keeps the item in memory and says whether the card saved it");
  static_assert(mado::same_type<Find, bool>::value,
                "find(id, Item&) copies the item out and returns whether the id holds one");
  static_assert(mado::same_type<Store, Written>::value,
                "store(item) overwrites the item and says whether the card saved it");
  static_assert(mado::same_type<Erase, Written>::value,
                "erase(id) frees the id and says whether the card forgot it");

  static constexpr bool kSatisfied = true;
};

}

#endif
