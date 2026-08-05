#include "SpatialLightCore.h"

namespace spatial_light {

SpatialLayout defaultLayout() {
  return {LayoutProfile::Continuous, kDefaultLedCount,
          resolveCenterIndex(kDefaultLedCount, 0, 0, 0, false), 0,
          kDefaultLedCount, 0, false};
}

SpatialLayout normalizeLayout(const SpatialLayout &candidate) {
  SpatialLayout normalized = candidate;
  normalized.activeCount = clampLedCount(candidate.activeCount);
  if (candidate.profile == LayoutProfile::Segmented &&
      isValidSegmentedLayout(normalized.activeCount, candidate.leftCount,
                             candidate.centerCount, candidate.rightCount)) {
    normalized.profile = LayoutProfile::Segmented;
    normalized.centerIndex = resolveCenterIndex(
        normalized.activeCount, normalized.leftCount, normalized.centerCount,
        normalized.rightCount, true);
    return normalized;
  }
  normalized.profile = LayoutProfile::Continuous;
  normalized.leftCount = 0;
  normalized.centerCount = normalized.activeCount;
  normalized.rightCount = 0;
  normalized.centerIndex = candidate.centerIndex < normalized.activeCount
                               ? candidate.centerIndex
                               : resolveCenterIndex(normalized.activeCount, 0,
                                                    0, 0, false);
  return normalized;
}

}  // namespace spatial_light
