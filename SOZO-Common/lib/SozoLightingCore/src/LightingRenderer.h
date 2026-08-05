#pragma once

#include <LightingFrame.h>

namespace sozo {

constexpr Rgb scaleRgb(const Rgb color, const uint8_t scale) {
  return {static_cast<uint8_t>(static_cast<uint16_t>(color.red) * scale / 255U),
          static_cast<uint8_t>(static_cast<uint16_t>(color.green) * scale / 255U),
          static_cast<uint8_t>(static_cast<uint16_t>(color.blue) * scale / 255U)};
}

constexpr Rgb blendRgb(const Rgb from, const Rgb to, const uint8_t amount) {
  return {static_cast<uint8_t>((static_cast<uint16_t>(from.red) * (255U - amount) +
                                static_cast<uint16_t>(to.red) * amount) / 255U),
          static_cast<uint8_t>((static_cast<uint16_t>(from.green) * (255U - amount) +
                                static_cast<uint16_t>(to.green) * amount) / 255U),
          static_cast<uint8_t>((static_cast<uint16_t>(from.blue) * (255U - amount) +
                                static_cast<uint16_t>(to.blue) * amount) / 255U)};
}

constexpr uint16_t resolvePhysicalIndex(const uint16_t logicalIndex,
                                        const uint16_t activeCount,
                                        const bool reversed) {
  return logicalIndex >= activeCount || activeCount == 0U
             ? 0U
             : (reversed ? static_cast<uint16_t>(activeCount - 1U - logicalIndex)
                         : logicalIndex);
}

constexpr uint8_t rainbowCycleCount(const uint8_t rainbowStyle) {
  return rainbowStyle == 0U ? 1U : (rainbowStyle == 1U ? 2U : 4U);
}

constexpr uint16_t rainbowHueForPixel(const uint16_t firstHue,
                                      const uint16_t pixelIndex,
                                      const uint16_t activeCount,
                                      const uint8_t cycleCount) {
  return activeCount == 0U
             ? firstHue
             : static_cast<uint16_t>(
                   static_cast<uint32_t>(firstHue) +
                   static_cast<uint32_t>(pixelIndex) * cycleCount * 65536UL /
                       activeCount);
}

class LightingRenderer {
 public:
  explicit LightingRenderer(uint16_t logicalLedCount =
                                spatial_light::kDefaultLedCount);

  void setLogicalLedCount(uint16_t logicalLedCount);
  void setState(const PersistedLightingState &state);
  const PersistedLightingState &state() const;
  bool render(uint32_t nowMs, const AudioFrame &audio, LightingFrame &frame);

 private:
  static Rgb hsv(uint16_t hue, uint8_t saturation, uint8_t value);
  static Rgb gamma(Rgb color);
  static uint32_t mapRange(int32_t value, int32_t inputMinimum,
                           int32_t inputMaximum, int32_t outputMinimum,
                           int32_t outputMaximum);
  static float clampFloat(float value, float minimum, float maximum);
  uint32_t nextRandom();
  void clear(LightingFrame &frame) const;
  void setPixel(LightingFrame &frame, uint16_t index, Rgb color) const;
  bool due(uint32_t nowMs, uint32_t &lastUpdateMs, uint16_t intervalMs,
           bool force) const;
  void renderStatic(LightingFrame &frame) const;
  void renderRainbow(LightingFrame &frame);
  void renderBreathe(uint32_t nowMs, LightingFrame &frame) const;
  void renderMusic(const AudioFrame &audio, LightingFrame &frame) const;
  void renderComet(LightingFrame &frame);
  void renderAurora(LightingFrame &frame);
  void renderFlameAudio(const AudioFrame &audio, LightingFrame &frame);
  void renderGlassFlow(LightingFrame &frame);
  void renderCornerPulse(LightingFrame &frame);
  void renderBassRipple(const AudioFrame &audio, LightingFrame &frame);
  void renderFocus(LightingFrame &frame) const;

  uint16_t logicalLedCount_;
  PersistedLightingState state_{makeDefaultPersistedLightingState()};
  bool forceRender_{true};
  bool resetFrame_{true};
  uint32_t rainbowLastUpdate_{0};
  uint16_t rainbowFirstHue_{0};
  uint32_t breatheLastUpdate_{0};
  uint32_t musicLastUpdate_{0};
  uint32_t cometLastUpdate_{0};
  float cometPhase_{0.0F};
  uint16_t cometHue_{0};
  uint32_t auroraLastUpdate_{0};
  uint16_t auroraPhase_{0};
  uint32_t flameLastUpdate_{0};
  uint16_t flamePhase_{0};
  uint32_t glassFlowLastUpdate_{0};
  uint16_t glassFlowPhase_{0};
  uint32_t cornerPulseLastUpdate_{0};
  uint16_t cornerPulsePhase_{0};
  uint32_t bassRippleLastUpdate_{0};
  uint16_t bassRipplePhase_{0};
  uint32_t focusLastUpdate_{0};
  uint32_t randomState_{0x534F5A4FUL};
};

}  // namespace sozo
