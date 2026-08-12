#pragma once

#include <stdint.h>

#include <SozoDomain.h>

namespace sozo {

struct LightingScene {
  EffectMode mode{EffectMode::Static};
  uint8_t brightness{50U};
  Rgb primaryColor{255U, 120U, 0U};
  uint8_t audioColorStyle{0U};
  uint8_t cometColorStyle{0U};
  LightingSettings settings{};
  int16_t manualLitPixelCount{-1};
};

struct LocalLightConfiguration {
  spatial_light::SpatialLayout layout{makeDefaultSpatialLayout()};
  Rgb startupColor{50U, 120U, 220U};
  float startupAnimationSpeed{0.8F};
  uint32_t revision{0U};
};

LightingScene makeLightingScene(const PersistedLightingState &state,
                                int16_t manualLitPixelCount = -1);
PersistedLightingState applySceneToLocalState(
    const LightingScene &scene, const LocalLightConfiguration &configuration,
    const AudioTuning &audio);
PersistedLightingState applyLightingSceneToNodeState(
    const PersistedLightingState &localState, const LightingScene &scene);

}  // namespace sozo
