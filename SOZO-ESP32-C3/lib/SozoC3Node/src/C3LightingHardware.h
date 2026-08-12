#pragma once

#include <FastLED.h>
#include <LedOutput.h>
#include <LightingControllerNodeSink.h>

namespace sozo::c3 {

class C3LightingOutput final : public lighting::LedOutput {
 public:
  C3LightingOutput(uint16_t capacity, uint8_t pin);
  ~C3LightingOutput() override;
  C3LightingOutput(const C3LightingOutput &) = delete;
  C3LightingOutput &operator=(const C3LightingOutput &) = delete;

  void begin(const lighting::LedGeometry &geometry) override;
  void present(const lighting::LedGeometry &geometry,
               const LightingFrame &frame) override;

 private:
  uint16_t capacity_;
  uint16_t transmittedCount_{0};
  uint8_t pin_;
  bool initialized_{false};
  CRGB *pixels_;
  CLEDController *controller_{nullptr};
};

using LightingControllerSink = ::sozo::LightingControllerNodeSink;

}  // namespace sozo::c3
