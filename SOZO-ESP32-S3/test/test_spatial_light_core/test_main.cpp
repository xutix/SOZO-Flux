#include <Arduino.h>
#include <SpatialLightCore.h>

static_assert(spatial_light::clampLedCount(0) == 1,
              "zero must clamp to one");
static_assert(spatial_light::clampLedCount(308) == 308,
              "installed count must remain");
static_assert(spatial_light::clampLedCount(1300) == 1024,
              "count must honor firmware cap");
static_assert(spatial_light::isValidSegmentedLayout(308, 77, 154, 77),
              "three zones must sum to active count");
static_assert(!spatial_light::isValidSegmentedLayout(308, 70, 154, 77),
              "mismatched zones must be rejected");
static_assert(spatial_light::resolveCenterIndex(308, 0, 0, 0, false) == 153,
              "continuous layout center must be midpoint");
static_assert(spatial_light::resolveCenterIndex(308, 77, 154, 77, true) == 153,
              "segmented layout center must be center-zone midpoint");

void setup() {}

void loop() {}
