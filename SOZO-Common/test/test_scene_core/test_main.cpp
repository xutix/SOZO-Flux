#include <SpaceSceneCoordinator.h>

#include "../TestHarness.h"

namespace {

sozo::ControlCommand setBrightness(const int32_t value) {
  return {sozo::kControlProtocolVersion,
          sozo::ControlSource::Web,
          0U,
          sozo::ControlCommandType::SetParameter,
          sozo::LightingParameter::Brightness,
          value,
          {0U, 0U, 0U},
          sozo::makeDefaultSpatialLayout()};
}

sozo::ControlCommand command(const sozo::ControlCommandType type,
                             const int32_t value = 0) {
  return {sozo::kControlProtocolVersion,
          sozo::ControlSource::Web,
          0U,
          type,
          sozo::LightingParameter::None,
          value,
          {0U, 0U, 0U},
          sozo::makeDefaultSpatialLayout()};
}

sozo::ControlCommand setLayout(const spatial_light::SpatialLayout &layout) {
  return {sozo::kControlProtocolVersion,
          sozo::ControlSource::Web,
          0U,
          sozo::ControlCommandType::SetLayout,
          sozo::LightingParameter::None,
          0,
          {0U, 0U, 0U},
          layout};
}

sozo::ControlCommand setParameter(const sozo::LightingParameter parameter,
                                  const int32_t value,
                                  const sozo::Rgb color = {0U, 0U, 0U}) {
  return {sozo::kControlProtocolVersion,
          sozo::ControlSource::Web,
          0U,
          sozo::ControlCommandType::SetParameter,
          parameter,
          value,
          color,
          sozo::makeDefaultSpatialLayout()};
}

void test_space_scene_changes_without_a_local_light_node() {
  sozo::SpaceSceneCoordinator coordinator;
  coordinator.begin(sozo::makeDefaultPersistedLightingState());
  CHECK_EQ(1U, coordinator.snapshot().revision);

  CHECK_TRUE(coordinator.apply(setBrightness(87)));
  CHECK_EQ(87U, coordinator.snapshot().lighting.brightness);
  CHECK_EQ(2U, coordinator.snapshot().revision);
}

void test_turn_off_keeps_the_last_active_scene_for_restart_and_turn_on() {
  sozo::PersistedLightingState initial =
      sozo::makeDefaultPersistedLightingState();
  initial.mode = sozo::EffectMode::Rainbow;
  sozo::SpaceSceneCoordinator coordinator;
  coordinator.begin(initial);

  CHECK_TRUE(coordinator.apply(command(sozo::ControlCommandType::TurnOff)));
  CHECK_EQ(sozo::EffectMode::Off, coordinator.snapshot().lighting.mode);
  CHECK_EQ(sozo::EffectMode::Rainbow,
           coordinator.persistedLightingState().mode);

  CHECK_TRUE(coordinator.apply(command(sozo::ControlCommandType::TurnOn)));
  CHECK_EQ(sozo::EffectMode::Rainbow, coordinator.snapshot().lighting.mode);
  CHECK_EQ(3U, coordinator.snapshot().revision);
}

void test_off_cannot_be_selected_as_an_effect() {
  sozo::SpaceSceneCoordinator coordinator;
  coordinator.begin(sozo::makeDefaultPersistedLightingState());

  CHECK_TRUE(!coordinator.apply(command(
      sozo::ControlCommandType::SetEffect,
      static_cast<int32_t>(sozo::EffectMode::Off))));
  CHECK_EQ(sozo::EffectMode::Static, coordinator.snapshot().lighting.mode);
  CHECK_EQ(1U, coordinator.snapshot().revision);
}

void test_local_strip_layout_is_node_configuration_not_space_scene() {
  sozo::SpaceSceneCoordinator coordinator;
  coordinator.begin(sozo::makeDefaultPersistedLightingState());
  const spatial_light::SpatialLayout localLayout{
      spatial_light::LayoutProfile::Continuous, 144U, 71U, 0U, 144U, 0U,
      true};

  CHECK_TRUE(coordinator.apply(setLayout(localLayout)));
  CHECK_EQ(1U, coordinator.snapshot().revision);
  CHECK_EQ(2U, coordinator.localLightConfiguration().revision);
  CHECK_EQ(144U, coordinator.localLightConfiguration().layout.activeCount);
  CHECK_EQ(71U, coordinator.localLightConfiguration().layout.centerIndex);
  CHECK_TRUE(coordinator.localLightConfiguration().layout.reversed);
  CHECK_EQ(144U, coordinator.lightingState().layout.activeCount);
}

void test_render_controls_change_the_space_scene() {
  sozo::SpaceSceneCoordinator coordinator;
  coordinator.begin(sozo::makeDefaultPersistedLightingState());

  CHECK_TRUE(coordinator.apply(
      command(sozo::ControlCommandType::SetEffect,
              static_cast<int32_t>(sozo::EffectMode::Comet))));
  CHECK_TRUE(coordinator.apply(
      setParameter(sozo::LightingParameter::FlowSpeed, 91)));
  CHECK_TRUE(coordinator.apply(setParameter(
      sozo::LightingParameter::PrimaryColor, 0x0C2238, {12U, 34U, 56U})));

  CHECK_EQ(sozo::EffectMode::Comet, coordinator.snapshot().lighting.mode);
  CHECK_EQ(91U, coordinator.snapshot().lighting.settings.flowSpeed);
  CHECK_EQ(12U, coordinator.snapshot().lighting.primaryColor.red);
  CHECK_EQ(34U, coordinator.snapshot().lighting.primaryColor.green);
  CHECK_EQ(56U, coordinator.snapshot().lighting.primaryColor.blue);
  CHECK_EQ(4U, coordinator.snapshot().revision);
}

void test_manual_pixel_intent_is_not_limited_by_the_hub_local_strip() {
  sozo::PersistedLightingState initial =
      sozo::makeDefaultPersistedLightingState();
  initial.layout.activeCount = 144U;
  initial.layout.centerIndex = 71U;
  initial.layout.centerCount = 144U;
  sozo::SpaceSceneCoordinator coordinator;
  coordinator.begin(initial);

  CHECK_TRUE(coordinator.apply(setParameter(
      sozo::LightingParameter::ManualLitPixelCount, 512)));
  CHECK_EQ(512, coordinator.snapshot().lighting.manualLitPixelCount);
}

}  // namespace

int main(int, char **) {
  test_space_scene_changes_without_a_local_light_node();
  test_turn_off_keeps_the_last_active_scene_for_restart_and_turn_on();
  test_off_cannot_be_selected_as_an_effect();
  test_local_strip_layout_is_node_configuration_not_space_scene();
  test_render_controls_change_the_space_scene();
  test_manual_pixel_intent_is_not_limited_by_the_hub_local_strip();
  return sozo::test::finish("space scene core tests");
}
