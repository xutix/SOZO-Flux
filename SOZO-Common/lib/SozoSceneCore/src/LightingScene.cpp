#include "LightingScene.h"

namespace sozo {

LightingScene makeLightingScene(const PersistedLightingState &state,
                                const int16_t manualLitPixelCount) {
  LightingScene scene{};
  scene.mode = state.mode;
  scene.brightness = state.brightness;
  scene.primaryColor = state.primaryColor;
  scene.audioColorStyle = state.audioColorStyle;
  scene.cometColorStyle = state.cometColorStyle;
  scene.settings = state.lighting;
  scene.manualLitPixelCount = manualLitPixelCount;
  return scene;
}

PersistedLightingState applySceneToLocalState(
    const LightingScene &scene, const LocalLightConfiguration &configuration,
    const AudioTuning &audio) {
  PersistedLightingState state{};
  state.mode = scene.mode;
  state.brightness = scene.brightness;
  state.primaryColor = scene.primaryColor;
  state.audioColorStyle = scene.audioColorStyle;
  state.cometColorStyle = scene.cometColorStyle;
  state.lighting = scene.settings;
  state.layout = configuration.layout;
  state.startupColor = configuration.startupColor;
  state.startupAnimationSpeed = configuration.startupAnimationSpeed;
  state.audio = audio;
  return state;
}

PersistedLightingState applyLightingSceneToNodeState(
    const PersistedLightingState &localState, const LightingScene &scene) {
  PersistedLightingState state = localState;
  state.mode = scene.mode;
  state.brightness = scene.brightness;
  state.primaryColor = scene.primaryColor;
  state.audioColorStyle = scene.audioColorStyle;
  state.cometColorStyle = scene.cometColorStyle;
  state.lighting = scene.settings;
  return state;
}

}  // namespace sozo
