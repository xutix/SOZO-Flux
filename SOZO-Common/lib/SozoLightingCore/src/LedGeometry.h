#pragma once

#include <stdint.h>

namespace sozo::lighting {

// Keeps effect-space pixels separate from the physical strip that receives a
// full frame. A physical pixel outside the active logical range is unmapped.
struct LedGeometry {
  uint16_t activeLedCount{0};
  uint16_t physicalLedCount{0};
  bool reversed{false};
};

constexpr bool isValidLedGeometry(const LedGeometry &geometry) {
  return geometry.activeLedCount > 0 &&
         geometry.activeLedCount <= geometry.physicalLedCount;
}

constexpr int32_t logicalIndexForPhysical(const LedGeometry &geometry,
                                          const uint16_t physicalIndex) {
  return !isValidLedGeometry(geometry) ||
                 physicalIndex >= geometry.physicalLedCount ||
                 physicalIndex >= geometry.activeLedCount
             ? -1
             : (geometry.reversed
                    ? static_cast<int32_t>(geometry.activeLedCount - 1U -
                                           physicalIndex)
                    : static_cast<int32_t>(physicalIndex));
}

}  // namespace sozo::lighting
