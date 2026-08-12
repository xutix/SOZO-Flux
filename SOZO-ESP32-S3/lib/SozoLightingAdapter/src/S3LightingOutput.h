#pragma once

#include <FastLED.h>
#include <LedOutput.h>

namespace sozo::s3 {

class S3LightingOutput final : public lighting::LedOutput {
 public:
  S3LightingOutput(uint16_t capacity, uint8_t pin);
  ~S3LightingOutput() override;
  S3LightingOutput(const S3LightingOutput &) = delete;
  S3LightingOutput &operator=(const S3LightingOutput &) = delete;

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

}  // namespace sozo::s3
