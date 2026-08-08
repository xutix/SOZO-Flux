#include "LightingControllerNodeSink.h"

namespace sozo {

LightingControllerNodeSink::LightingControllerNodeSink(
    LightingController &controller)
    : controller_(controller) {}

void LightingControllerNodeSink::begin(const PersistedLightingState &state) {
  controller_.begin(state);
}

const PersistedLightingState &LightingControllerNodeSink::state() const {
  return controller_.state();
}

void LightingControllerNodeSink::applyState(
    const PersistedLightingState &state) {
  controller_.setState(state);
}

void LightingControllerNodeSink::setLitPixelCount(const uint16_t count) {
  controller_.setLitPixelCount(count);
}

void LightingControllerNodeSink::tick(const uint32_t now,
                                      const AudioFrame &audio) {
  controller_.tick(now, audio);
}

}  // namespace sozo
