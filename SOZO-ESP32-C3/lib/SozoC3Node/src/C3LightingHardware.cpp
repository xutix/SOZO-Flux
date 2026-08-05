#include "C3LightingHardware.h"

#include <algorithm>

namespace sozo::c3 {

C3LightingOutput::C3LightingOutput(const uint16_t capacity, const uint8_t pin)
    : capacity_(capacity), strip_(capacity, pin, NEO_RGB + NEO_KHZ800) {}

void C3LightingOutput::begin(const lighting::LedGeometry &geometry) {
  const uint16_t nextCount = std::min(geometry.physicalLedCount, capacity_);
  if (!initialized_) {
    strip_.begin();
    strip_.clear();
    strip_.show();
    transmittedCount_ = capacity_;
    initialized_ = true;
  }
  if (nextCount < transmittedCount_) {
    strip_.clear();
    strip_.show();
  }
  if (nextCount != transmittedCount_) {
    strip_.updateLength(nextCount);
    transmittedCount_ = nextCount;
  }
  strip_.clear();
  strip_.show();
}

void C3LightingOutput::present(const lighting::LedGeometry &geometry,
                               const LightingFrame &frame) {
  const uint16_t count = std::min(transmittedCount_, geometry.physicalLedCount);
  for (uint16_t physical = 0; physical < count; ++physical) {
    const int32_t logical = lighting::logicalIndexForPhysical(geometry, physical);
    const Rgb color = logical >= 0 && logical < frame.count
                          ? frame.pixels[logical]
                          : Rgb{0U, 0U, 0U};
    strip_.setPixelColor(physical, color.red, color.green, color.blue);
  }
  strip_.show();
}

LightingControllerSink::LightingControllerSink(LightingController &controller)
    : controller_(controller) {}

void LightingControllerSink::begin(const PersistedLightingState &state) {
  controller_.begin(state);
}

const PersistedLightingState &LightingControllerSink::state() const {
  return controller_.state();
}

void LightingControllerSink::applyState(const PersistedLightingState &state) {
  controller_.setState(state);
}

void LightingControllerSink::setLitPixelCount(const uint16_t count) {
  controller_.setLitPixelCount(count);
}

void LightingControllerSink::tick(const uint32_t now,
                                  const AudioFrame &audio) {
  controller_.tick(now, audio);
}

}  // namespace sozo::c3
