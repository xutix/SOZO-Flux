#include <LightingRenderer.h>
#include <LedOutput.h>
#include <LightingController.h>

#include "../TestHarness.h"

namespace {

bool sameColor(const sozo::Rgb left, const sozo::Rgb right) {
  return left.red == right.red && left.green == right.green &&
         left.blue == right.blue;
}

bool anyLit(const sozo::LightingFrame &frame) {
  for (uint16_t index = 0; index < frame.count; ++index) {
    if (!sameColor(frame.pixels[index], {0U, 0U, 0U})) return true;
  }
  return false;
}

bool isNonUniform(const sozo::LightingFrame &frame) {
  for (uint16_t index = 1; index < frame.count; ++index) {
    if (!sameColor(frame.pixels[0], frame.pixels[index])) return true;
  }
  return false;
}

class FakeLedOutput final : public sozo::lighting::LedOutput {
 public:
  void begin(const sozo::lighting::LedGeometry &geometry) override {
    lastGeometry = geometry;
    ++beginCalls;
  }

  void present(const sozo::lighting::LedGeometry &geometry,
               const sozo::LightingFrame &frame) override {
    lastGeometry = geometry;
    lastFrame = frame;
    ++presentCalls;
  }

  sozo::lighting::LedGeometry lastGeometry{};
  sozo::LightingFrame lastFrame{};
  int beginCalls{0};
  int presentCalls{0};
};

void test_static_scene_renders_full_logical_frame() {
  sozo::LightingRenderer renderer(60U);
  sozo::PersistedLightingState scene =
      sozo::makeDefaultPersistedLightingState();
  scene.mode = sozo::EffectMode::Static;
  scene.brightness = 255U;
  scene.primaryColor = {12U, 34U, 56U};
  renderer.setState(scene);

  sozo::LightingFrame frame{};
  renderer.render(0U, {}, frame);
  CHECK_EQ(60U, frame.count);
  CHECK_EQ(12U, frame.pixels[0].red);
  CHECK_EQ(34U, frame.pixels[30].green);
  CHECK_EQ(56U, frame.pixels[59].blue);
}

void test_zero_brightness_renders_black_without_hardware_output() {
  sozo::LightingRenderer renderer(60U);
  sozo::PersistedLightingState scene =
      sozo::makeDefaultPersistedLightingState();
  scene.mode = sozo::EffectMode::Static;
  scene.brightness = 0U;
  scene.primaryColor = {255U, 120U, 20U};
  renderer.setState(scene);

  sozo::LightingFrame frame{};
  renderer.render(0U, {}, frame);
  CHECK_EQ(60U, frame.count);
  CHECK_EQ(0U, frame.pixels[0].red);
  CHECK_EQ(0U, frame.pixels[30].green);
  CHECK_EQ(0U, frame.pixels[59].blue);
}

void test_rainbow_frames_change_with_time_but_keep_logical_count() {
  sozo::LightingRenderer renderer(60U);
  sozo::PersistedLightingState scene =
      sozo::makeDefaultPersistedLightingState();
  scene.mode = sozo::EffectMode::Rainbow;
  scene.brightness = 255U;
  scene.lighting.flowSpeed = 100U;
  renderer.setState(scene);

  sozo::LightingFrame first{};
  sozo::LightingFrame second{};
  renderer.render(0U, {}, first);
  renderer.render(20U, {}, second);
  CHECK_EQ(60U, first.count);
  CHECK_EQ(60U, second.count);
  CHECK_TRUE(first.pixels[0].red != second.pixels[0].red ||
             first.pixels[0].green != second.pixels[0].green ||
             first.pixels[0].blue != second.pixels[0].blue);
}

void test_every_dynamic_effect_has_its_own_rendering_behavior() {
  sozo::LightingRenderer renderer(60U);
  sozo::PersistedLightingState scene =
      sozo::makeDefaultPersistedLightingState();
  scene.brightness = 255U;
  scene.primaryColor = {180U, 70U, 30U};
  scene.lighting.secondaryRed = 20U;
  scene.lighting.secondaryGreen = 180U;
  scene.lighting.secondaryBlue = 220U;
  scene.lighting.flowSpeed = 100U;
  scene.lighting.cometSpeed = 50U;
  scene.lighting.cometDensity = 3U;
  scene.lighting.pulseHeightPercent = 20U;

  sozo::LightingFrame first{};
  sozo::LightingFrame second{};

  scene.mode = sozo::EffectMode::Breathe;
  renderer.setState(scene);
  renderer.render(0U, {}, first);
  renderer.render(1500U, {}, second);
  CHECK_TRUE(!sameColor(first.pixels[0], second.pixels[0]));

  scene.mode = sozo::EffectMode::Music;
  renderer.setState(scene);
  renderer.render(2300U, {}, first);
  renderer.render(2350U,
                  {220.0F, 0.0F, 160.0F, 90.0F, 180.0F, 1U, true},
                  second);
  CHECK_TRUE(!anyLit(first));
  CHECK_TRUE(anyLit(second));

  scene.mode = sozo::EffectMode::Comet;
  renderer.setState(scene);
  renderer.render(2400U, {}, first);
  CHECK_TRUE(isNonUniform(first));

  scene.mode = sozo::EffectMode::Aurora;
  renderer.setState(scene);
  renderer.render(2500U, {}, first);
  CHECK_TRUE(isNonUniform(first));

  scene.mode = sozo::EffectMode::FlameAudio;
  renderer.setState(scene);
  renderer.render(2600U,
                  {230.0F, 0.0F, 180.0F, 100.0F, 200.0F, 1U, true},
                  first);
  CHECK_TRUE(anyLit(first));
  CHECK_TRUE(isNonUniform(first));

  scene.mode = sozo::EffectMode::GlassFlow;
  renderer.setState(scene);
  renderer.render(2700U, {}, first);
  CHECK_TRUE(isNonUniform(first));

  scene.mode = sozo::EffectMode::CornerPulse;
  renderer.setState(scene);
  renderer.render(2800U, {}, first);
  CHECK_TRUE(anyLit(first));
  CHECK_TRUE(sameColor(first.pixels[30], {0U, 0U, 0U}));

  scene.mode = sozo::EffectMode::BassRipple;
  renderer.setState(scene);
  renderer.render(2900U,
                  {220.0F, 0.0F, 160.0F, 90.0F, 180.0F, 1U, true},
                  first);
  CHECK_TRUE(anyLit(first));
  CHECK_TRUE(isNonUniform(first));

  scene.mode = sozo::EffectMode::Focus;
  renderer.setState(scene);
  renderer.render(3000U, {}, first);
  CHECK_TRUE(anyLit(first));
  CHECK_TRUE(!sameColor(first.pixels[0], scene.primaryColor));
}

void test_shared_controller_delivers_only_changed_logical_frames() {
  FakeLedOutput output;
  sozo::LightingController controller(output);
  sozo::PersistedLightingState state =
      sozo::makeDefaultPersistedLightingState();
  state.layout.activeCount = 12U;
  state.layout.centerIndex = 5U;
  state.layout.reversed = true;
  state.startupAnimationSpeed = 100.0F;
  state.mode = sozo::EffectMode::Rainbow;
  state.brightness = 255U;
  state.lighting.flowSpeed = 100U;
  controller.begin(state);
  CHECK_EQ(1, output.beginCalls);
  CHECK_EQ(12U, output.lastGeometry.activeLedCount);
  CHECK_TRUE(output.lastGeometry.reversed);

  controller.tick(25U, {});
  const int afterStartup = output.presentCalls;
  CHECK_TRUE(afterStartup >= 1);
  controller.tick(26U, {});
  CHECK_EQ(afterStartup + 1, output.presentCalls);
  CHECK_EQ(12U, output.lastFrame.count);
  CHECK_TRUE(isNonUniform(output.lastFrame));

  controller.tick(27U, {});
  CHECK_EQ(afterStartup + 1, output.presentCalls);
  controller.tick(40U, {});
  CHECK_EQ(afterStartup + 2, output.presentCalls);
}

void test_static_color_command_forces_a_new_frame() {
  FakeLedOutput output;
  sozo::LightingController controller(output);
  sozo::PersistedLightingState state =
      sozo::makeDefaultPersistedLightingState();
  state.layout.activeCount = 10U;
  state.startupAnimationSpeed = 100.0F;
  state.mode = sozo::EffectMode::Static;
  state.brightness = 255U;
  state.primaryColor = {10U, 20U, 30U};
  controller.begin(state);
  controller.tick(25U, {});
  controller.tick(26U, {});
  const int beforeChange = output.presentCalls;

  state.primaryColor = {80U, 90U, 100U};
  controller.setState(state);
  controller.tick(27U, {});
  CHECK_EQ(beforeChange + 1, output.presentCalls);
  CHECK_EQ(80U, output.lastFrame.pixels[0].red);
  CHECK_EQ(90U, output.lastFrame.pixels[0].green);
  CHECK_EQ(100U, output.lastFrame.pixels[0].blue);
}

}  // namespace

int main(int, char **) {
  test_static_scene_renders_full_logical_frame();
  test_zero_brightness_renders_black_without_hardware_output();
  test_rainbow_frames_change_with_time_but_keep_logical_count();
  test_every_dynamic_effect_has_its_own_rendering_behavior();
  test_shared_controller_delivers_only_changed_logical_frames();
  test_static_color_command_forces_a_new_frame();
  return sozo::test::finish("lighting core tests");
}
