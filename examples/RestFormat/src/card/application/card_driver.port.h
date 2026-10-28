// SPDX-FileCopyrightText: 2026 Gustavo Franco
// SPDX-License-Identifier: MIT

#ifndef REST_FORMAT_CARD_CARD_DRIVER_PORT_H
#define REST_FORMAT_CARD_CARD_DRIVER_PORT_H

#include <mado/reference_to.h>
#include <mado/same_type.h>
#include <stddef.h>
#include <stdint.h>

#include "../domain/file_system.policy.h"

namespace card {

template <typename DriverT>
struct CardDriverPort {
  using Device = typename DriverT::Device;
  using Open = decltype(DriverT::open());
  using Format = decltype(DriverT::format(mado::reference_to<Device>(), CardFileSystem::kNone,
                                          static_cast<uint8_t*>(nullptr)));
  using Mounted = decltype(DriverT::mounted(mado::reference_to<Device>()));
  using Count = decltype(mado::reference_to<Device>().sectorCount());
  using Read = decltype(mado::reference_to<Device>().readSectors(
      uint32_t{}, static_cast<uint8_t*>(nullptr), size_t{}));
  using Write = decltype(mado::reference_to<Device>().writeSectors(
      uint32_t{}, static_cast<const uint8_t*>(nullptr), size_t{}));

  static_assert(mado::same_type<Open, Device*>::value, "open() returns the card or nullptr");
  static_assert(mado::same_type<Format, bool>::value, "format(card, kind, sector) lays a volume");
  static_assert(mado::same_type<Mounted, CardFileSystem>::value, "mounted(card) names the volume");
  static_assert(mado::same_type<Count, uint32_t>::value, "sectorCount() counts 512-byte sectors");
  static_assert(mado::same_type<Read, bool>::value, "readSectors(first, out, count) reads");
  static_assert(mado::same_type<Write, bool>::value, "writeSectors(first, in, count) writes");

  static constexpr bool kSatisfied = true;
};

}

#endif
