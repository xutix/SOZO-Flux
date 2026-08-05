#pragma once

#include <Adafruit_NeoPixel.h>
#include <LedOutput.h>

namespace sozo::s3 {

class S3LightingOutput final : public lighting::LedOutput {
 public:
  S3LightingOutput(uint16_t capacity, uint8_t pin);

  void begin(const lighting::LedGeometry &geometry) override;
  void present(const lighting::LedGeometry &geometry,
               const LightingFrame &frame) override;

 private:
  uint16_t capacity_;
  uint16_t transmittedCount_{0};
  bool initialized_{false};
  Adafruit_NeoPixel strip_;
};

}  // namespace sozo::s3
