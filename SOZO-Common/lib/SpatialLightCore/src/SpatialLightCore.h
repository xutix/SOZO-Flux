#pragma once

#include <stdint.h>

namespace spatial_light {

constexpr uint16_t kDefaultLedCount = 308;
constexpr uint16_t kMaxLedCount = 1024;

enum class LayoutProfile : uint8_t { Continuous = 0, Segmented = 1 };

struct SpatialLayout {
  LayoutProfile profile;
  uint16_t activeCount;
  uint16_t centerIndex;
  uint16_t leftCount;
  uint16_t centerCount;
  uint16_t rightCount;
  bool reversed;
};

constexpr uint16_t clampLedCount(const uint16_t count) {
  return count == 0 ? 1 : (count > kMaxLedCount ? kMaxLedCount : count);
}

constexpr bool isValidSegmentedLayout(const uint16_t activeCount,
                                      const uint16_t leftCount,
                                      const uint16_t centerCount,
                                      const uint16_t rightCount) {
  return activeCount > 0 && centerCount > 0 &&
         static_cast<uint32_t>(leftCount) + centerCount + rightCount ==
             activeCount;
}

constexpr uint16_t resolveCenterIndex(const uint16_t activeCount,
                                      const uint16_t leftCount,
                                      const uint16_t centerCount,
                                      const uint16_t,
                                      const bool segmented) {
  return segmented && centerCount > 0
             ? static_cast<uint16_t>(leftCount + (centerCount - 1) / 2)
             : static_cast<uint16_t>((clampLedCount(activeCount) - 1) / 2);
}

SpatialLayout defaultLayout();
SpatialLayout normalizeLayout(const SpatialLayout &candidate);

}  // namespace spatial_light
