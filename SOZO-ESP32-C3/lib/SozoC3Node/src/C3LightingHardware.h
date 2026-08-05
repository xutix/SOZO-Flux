#pragma once

#include <Adafruit_NeoPixel.h>
#include <LedOutput.h>
#include <LightingController.h>
#include <NodeSceneRuntime.h>

namespace sozo::c3 {

class C3LightingOutput final : public lighting::LedOutput {
 public:
  C3LightingOutput(uint16_t capacity, uint8_t pin);

  void begin(const lighting::LedGeometry &geometry) override;
  void present(const lighting::LedGeometry &geometry,
               const LightingFrame &frame) override;

 private:
  uint16_t capacity_;
  uint16_t transmittedCount_{0};
  bool initialized_{false};
  Adafruit_NeoPixel strip_;
};

class LightingControllerSink final : public NodeLightingSink {
 public:
  explicit LightingControllerSink(LightingController &controller);

  void begin(const PersistedLightingState &state) override;
  const PersistedLightingState &state() const override;
  void applyState(const PersistedLightingState &state) override;
  void setLitPixelCount(uint16_t count) override;
  void tick(uint32_t now, const AudioFrame &audio) override;

 private:
  LightingController &controller_;
};

}  // namespace sozo::c3
