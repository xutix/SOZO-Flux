#include <Arduino.h>
#include <SpatialLightCore.h>

static_assert(spatial_light::clampLedCount(0) == 1,
              "zero must clamp to one");
static_assert(spatial_light::clampLedCount(144) == 144,
              "installed count must remain");
static_assert(spatial_light::clampLedCount(1300) == 1024,
              "count must honor firmware cap");
static_assert(spatial_light::isValidSegmentedLayout(144, 36, 72, 36),
              "three zones must sum to active count");
static_assert(!spatial_light::isValidSegmentedLayout(144, 35, 72, 36),
              "mismatched zones must be rejected");
static_assert(spatial_light::resolveCenterIndex(144, 0, 0, 0, false) == 71,
              "continuous layout center must be midpoint");
static_assert(spatial_light::resolveCenterIndex(144, 36, 72, 36, true) == 71,
              "segmented layout center must be center-zone midpoint");

void setup() {}

void loop() {}
