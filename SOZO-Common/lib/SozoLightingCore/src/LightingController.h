#pragma once

#include <LedOutput.h>
#include <LightingRenderer.h>

namespace sozo {

struct LightingSnapshot {
  EffectMode mode;
  uint16_t activeLedCount;
  bool startupAnimationActive;
  int16_t manualLitPixelCount;
};

class LightingController {
 public:
  explicit LightingController(lighting::LedOutput &output);

  void begin(const PersistedLightingState &state);
  bool apply(const ControlCommand &command);
  void setState(const PersistedLightingState &state);
  const PersistedLightingState &state() const;
  PersistedLightingState persistedState() const;
  void setLitPixelCount(uint16_t count);
  void tick(uint32_t nowMs, const AudioFrame &audio);
  LightingSnapshot snapshot() const;

 private:
  static PersistedLightingState normalize(PersistedLightingState state);
  lighting::LedGeometry geometry() const;
  void renderStartupAnimation(uint32_t nowMs);
  void present();
  void clearFrame();

  lighting::LedOutput &output_;
  LightingRenderer renderer_;
  LightingFrame frame_{};
  PersistedLightingState state_{makeDefaultPersistedLightingState()};
  EffectMode modeBeforeOff_{EffectMode::Static};
  bool started_{false};
  bool startupAnimationActive_{true};
  float startupProgress_{0.0F};
  uint32_t startupLastFrame_{0};
  int16_t manualLitPixelCount_{-1};
};

}  // namespace sozo
