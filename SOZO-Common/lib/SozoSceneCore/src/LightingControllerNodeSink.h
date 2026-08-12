#pragma once

#include <LightNodeRuntime.h>
#include <LightingController.h>

namespace sozo {

class LightingControllerNodeSink final : public LightNodeSink {
 public:
  explicit LightingControllerNodeSink(LightingController &controller);

  void begin(const PersistedLightingState &state) override;
  const PersistedLightingState &state() const override;
  void applyState(const PersistedLightingState &state) override;
  void setLitPixelCount(uint16_t count) override;
  void tick(uint32_t now, const AudioFrame &audio) override;

 private:
  LightingController &controller_;
};

}  // namespace sozo
