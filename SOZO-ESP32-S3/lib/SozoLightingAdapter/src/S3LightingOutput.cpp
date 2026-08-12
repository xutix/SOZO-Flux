#include "S3LightingOutput.h"

#include <algorithm>
#include <new>

namespace sozo::s3 {

namespace {

constexpr EOrder kMainStripColorOrder = RGB;

}  // namespace

S3LightingOutput::S3LightingOutput(const uint16_t capacity, const uint8_t pin)
    : capacity_(capacity),
      pin_(pin),
      pixels_(new (std::nothrow) CRGB[capacity]) {}

S3LightingOutput::~S3LightingOutput() { delete[] pixels_; }

void S3LightingOutput::begin(const lighting::LedGeometry &geometry) {
  const uint16_t nextCount = std::min(geometry.physicalLedCount, capacity_);
  if (!initialized_) {
    if (pin_ != SOZO_S3_LED_PIN || pixels_ == nullptr) return;
    controller_ = &FastLED.addLeds<WS2812, SOZO_S3_LED_PIN,
                                   kMainStripColorOrder>(pixels_, capacity_);
    FastLED.setBrightness(255U);
    fill_solid(pixels_, capacity_, CRGB::Black);
    FastLED.show();
    transmittedCount_ = capacity_;
    initialized_ = true;
  }
  if (nextCount < transmittedCount_) {
    fill_solid(pixels_, transmittedCount_, CRGB::Black);
    FastLED.show();
  }
  if (nextCount != transmittedCount_) {
    controller_->setLeds(pixels_, nextCount);
    transmittedCount_ = nextCount;
  }
  fill_solid(pixels_, transmittedCount_, CRGB::Black);
  FastLED.show();
}

void S3LightingOutput::present(const lighting::LedGeometry &geometry,
                               const LightingFrame &frame) {
  const uint16_t count = std::min(transmittedCount_, geometry.physicalLedCount);
  for (uint16_t physical = 0; physical < count; ++physical) {
    const int32_t logical = lighting::logicalIndexForPhysical(geometry, physical);
    const Rgb color = logical >= 0 && logical < frame.count
                          ? frame.pixels[logical]
                          : Rgb{0U, 0U, 0U};
    pixels_[physical] = CRGB(color.red, color.green, color.blue);
  }
  FastLED.show();
}

}  // namespace sozo::s3
