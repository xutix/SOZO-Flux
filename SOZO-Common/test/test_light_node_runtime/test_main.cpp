#include <LightNodeRuntime.h>

#include "../TestHarness.h"

namespace {

class FakeLightNodeSink final : public sozo::LightNodeSink {
 public:
  explicit FakeLightNodeSink(const sozo::PersistedLightingState &initial)
      : state_(initial) {}

  void begin(const sozo::PersistedLightingState &state) override {
    state_ = state;
    ++beginCalls;
  }
  const sozo::PersistedLightingState &state() const override { return state_; }
  void applyState(const sozo::PersistedLightingState &state) override {
    state_ = state;
    ++applyCalls;
  }
  void setLitPixelCount(const uint16_t count) override {
    lastLitPixelCount = count;
  }
  void tick(const uint32_t now, const sozo::AudioFrame &audio) override {
    lastTickNow = now;
    lastAudio = audio;
  }

  sozo::PersistedLightingState state_{};
  sozo::AudioFrame lastAudio{};
  uint32_t lastTickNow{0U};
  uint16_t lastLitPixelCount{0U};
  int beginCalls{0};
  int applyCalls{0};
};

void test_space_scene_changes_rendering_but_preserves_local_configuration() {
  sozo::PersistedLightingState local =
      sozo::makeDefaultPersistedLightingState();
  local.layout = {spatial_light::LayoutProfile::Continuous, 144U, 71U, 0U,
                  144U, 0U, true};
  local.startupColor = {70U, 80U, 90U};
  local.startupAnimationSpeed = 1.25F;
  local.audio.gain = 3.5F;
  FakeLightNodeSink sink(local);
  sozo::LightNodeRuntime runtime(sink);
  runtime.begin(local);

  sozo::LightingScene scene{};
  scene.mode = sozo::EffectMode::GlassFlow;
  scene.brightness = 180U;
  scene.primaryColor = {12U, 34U, 56U};
  scene.settings.flowSpeed = 88U;

  CHECK_EQ(sozo::LightSceneApplyResult::Applied,
           runtime.applyScene(scene, 5U, 1000U, 900U));
  CHECK_EQ(sozo::EffectMode::GlassFlow, sink.state().mode);
  CHECK_EQ(180U, sink.state().brightness);
  CHECK_EQ(88U, sink.state().lighting.flowSpeed);
  CHECK_EQ(144U, sink.state().layout.activeCount);
  CHECK_EQ(71U, sink.state().layout.centerIndex);
  CHECK_TRUE(sink.state().layout.reversed);
  CHECK_EQ(70U, sink.state().startupColor.red);
  CHECK_EQ(1.25F, sink.state().startupAnimationSpeed);
  CHECK_EQ(3.5F, sink.state().audio.gain);
  CHECK_EQ(5U, runtime.lastAppliedSceneRevision());
}

void test_node_clamps_manual_pixel_intent_to_its_local_strip() {
  sozo::PersistedLightingState local =
      sozo::makeDefaultPersistedLightingState();
  local.layout = {spatial_light::LayoutProfile::Continuous, 144U, 71U, 0U,
                  144U, 0U, false};
  FakeLightNodeSink sink(local);
  sozo::LightNodeRuntime runtime(sink);
  runtime.begin(local);

  sozo::LightingScene scene{};
  scene.manualLitPixelCount = 512;
  CHECK_EQ(sozo::LightSceneApplyResult::Applied,
           runtime.applyScene(scene, 1U, 10U, 10U));
  CHECK_EQ(144U, sink.lastLitPixelCount);

  sozo::LocalLightConfiguration configuration{};
  configuration.layout = {spatial_light::LayoutProfile::Continuous, 60U, 29U,
                          0U, 60U, 0U, false};
  runtime.updateLocalConfiguration(configuration, local.audio);
  CHECK_EQ(60U, sink.lastLitPixelCount);
}

void test_restoring_follow_mode_reapplies_the_follow_scene() {
  sozo::PersistedLightingState local =
      sozo::makeDefaultPersistedLightingState();
  FakeLightNodeSink sink(local);
  sozo::LightNodeRuntime runtime(sink);
  runtime.begin(local);

  sozo::LightingScene follow{};
  follow.primaryColor = {10U, 20U, 30U};
  CHECK_EQ(sozo::LightSceneApplyResult::Applied,
           runtime.applyScene(follow, 1U, 10U, 10U));
  CHECK_TRUE(runtime.setControlMode(
      sozo::LightControlMode::Independent));
  sozo::LightingScene independent = follow;
  independent.primaryColor = {88U, 77U, 66U};
  CHECK_EQ(sozo::LightSceneApplyResult::Applied,
           runtime.applyScene(independent, 1U, 11U, 11U,
                              sozo::LightSceneTarget::Independent));
  CHECK_EQ(88U, sink.state().primaryColor.red);

  sozo::LightNodeControlState rollback = runtime.controlState();
  rollback.controlMode = sozo::LightControlMode::FollowScene;
  CHECK_TRUE(runtime.restoreControlState(rollback));
  CHECK_EQ(sozo::LightControlMode::FollowScene, runtime.controlMode());
  CHECK_EQ(10U, sink.state().primaryColor.red);
}

void test_first_mode_change_after_restart_can_roll_back_to_local_follow_output() {
  sozo::PersistedLightingState local =
      sozo::makeDefaultPersistedLightingState();
  local.primaryColor = {42U, 20U, 10U};
  FakeLightNodeSink sink(local);
  sozo::LightNodeRuntime runtime(sink);
  runtime.begin(local);

  sozo::LightNodeControlState restored{};
  restored.controlMode = sozo::LightControlMode::FollowScene;
  restored.hasIndependentScene = true;
  restored.independentState = local;
  restored.independentState.primaryColor = {88U, 77U, 66U};
  restored.independentRevision = 3U;
  CHECK_TRUE(runtime.restoreControlState(restored));
  const sozo::LightNodeControlState before = runtime.controlState();

  CHECK_TRUE(runtime.setControlMode(
      sozo::LightControlMode::Independent));
  CHECK_EQ(88U, sink.state().primaryColor.red);
  CHECK_TRUE(runtime.restoreControlState(before));
  CHECK_EQ(sozo::LightControlMode::FollowScene, runtime.controlMode());
  CHECK_EQ(42U, sink.state().primaryColor.red);
}

}  // namespace

int main(int, char **) {
  test_space_scene_changes_rendering_but_preserves_local_configuration();
  test_node_clamps_manual_pixel_intent_to_its_local_strip();
  test_restoring_follow_mode_reapplies_the_follow_scene();
  test_first_mode_change_after_restart_can_roll_back_to_local_follow_output();
  return sozo::test::finish("shared light node runtime tests");
}
