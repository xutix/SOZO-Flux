#include <LightingScene.h>

#include "../TestHarness.h"

namespace {

void test_scene_is_the_only_publishable_lighting_contract() {
  sozo::PersistedLightingState initial =
      sozo::makeDefaultPersistedLightingState();
  initial.mode = sozo::EffectMode::Rainbow;
  initial.layout.activeCount = 144U;
  initial.startupColor = {1U, 2U, 3U};
  const sozo::LightingScene scene = sozo::makeLightingScene(initial, 42);

  CHECK_EQ(sozo::EffectMode::Rainbow, scene.mode);
  CHECK_EQ(42, scene.manualLitPixelCount);
  CHECK_EQ(50U, scene.brightness);
}

void test_local_configuration_is_applied_only_at_the_node_boundary() {
  sozo::LightingScene scene{};
  scene.mode = sozo::EffectMode::Comet;
  scene.brightness = 87U;
  sozo::LocalLightConfiguration local{};
  local.layout.activeCount = 144U;
  local.layout.centerIndex = 71U;
  local.startupColor = {12U, 34U, 56U};
  sozo::AudioTuning audio{};

  const sozo::PersistedLightingState state =
      sozo::applySceneToLocalState(scene, local, audio);
  CHECK_EQ(sozo::EffectMode::Comet, state.mode);
  CHECK_EQ(87U, state.brightness);
  CHECK_EQ(144U, state.layout.activeCount);
  CHECK_EQ(12U, state.startupColor.red);
}

}  // namespace

int main(int, char **) {
  test_scene_is_the_only_publishable_lighting_contract();
  test_local_configuration_is_applied_only_at_the_node_boundary();
  return sozo::test::finish("lighting scene contract tests");
}
