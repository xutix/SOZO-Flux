#include "LightingRenderer.h"

#include <algorithm>
#include <cmath>

namespace sozo {
namespace {

constexpr float kPi = 3.14159265358979323846F;

constexpr uint16_t clampCount(const uint16_t value) {
  return value == 0U ? 1U
       : value > spatial_light::kMaxLedCount ? spatial_light::kMaxLedCount
                                              : value;
}

const uint8_t kGammaTable[256] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,
    1,1,1,1,2,2,2,2,2,2,2,2,3,3,3,3,3,3,4,4,4,4,5,5,5,5,5,6,6,6,6,7,
    7,7,8,8,8,9,9,9,10,10,10,11,11,11,12,12,13,13,13,14,14,15,15,16,16,17,
    17,18,18,19,19,20,20,21,21,22,22,23,24,24,25,25,26,27,27,28,29,29,30,31,
    31,32,33,34,34,35,36,37,38,38,39,40,41,42,42,43,44,45,46,47,48,49,50,51,
    52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,68,69,70,71,72,73,75,76,77,
    78,80,81,82,84,85,86,88,89,90,92,93,94,96,97,99,100,102,103,105,106,108,
    109,111,112,114,115,117,119,120,122,124,125,127,129,130,132,134,136,137,
    139,141,143,145,146,148,150,152,154,156,158,160,162,164,166,168,170,172,
    174,176,178,180,182,184,186,188,191,193,195,197,199,202,204,206,209,211,
    213,215,218,220,223,225,227,230,232,235,237,240,242,245,247,250,252,255};

bool sameLayout(const spatial_light::SpatialLayout &left,
                const spatial_light::SpatialLayout &right) {
  return left.profile == right.profile && left.activeCount == right.activeCount &&
         left.centerIndex == right.centerIndex && left.leftCount == right.leftCount &&
         left.centerCount == right.centerCount && left.rightCount == right.rightCount &&
         left.reversed == right.reversed;
}

bool sameColor(const Rgb left, const Rgb right) {
  return left.red == right.red && left.green == right.green &&
         left.blue == right.blue;
}

}  // namespace

LightingRenderer::LightingRenderer(const uint16_t logicalLedCount)
    : logicalLedCount_(clampCount(logicalLedCount)) {
  state_.layout.activeCount = logicalLedCount_;
  state_.layout = spatial_light::normalizeLayout(state_.layout);
}

void LightingRenderer::setLogicalLedCount(const uint16_t logicalLedCount) {
  const uint16_t next = clampCount(logicalLedCount);
  if (next == logicalLedCount_) return;
  logicalLedCount_ = next;
  state_.layout.activeCount = logicalLedCount_;
  state_.layout = spatial_light::normalizeLayout(state_.layout);
  forceRender_ = true;
  resetFrame_ = true;
}

void LightingRenderer::setState(const PersistedLightingState &state) {
  PersistedLightingState next = state;
  next.layout.activeCount = logicalLedCount_;
  next.layout = spatial_light::normalizeLayout(next.layout);
  if (!isKnownEffect(next.mode) && next.mode != EffectMode::Off) {
    next.mode = EffectMode::Static;
  }
  resetFrame_ = resetFrame_ || next.mode != state_.mode ||
                next.brightness != state_.brightness ||
                !sameColor(next.primaryColor, state_.primaryColor) ||
                !sameLayout(next.layout, state_.layout);
  forceRender_ = true;
  state_ = next;
}

const PersistedLightingState &LightingRenderer::state() const { return state_; }

bool LightingRenderer::render(const uint32_t nowMs, const AudioFrame &audio,
                              LightingFrame &frame) {
  frame.count = logicalLedCount_;
  if (resetFrame_) clear(frame);
  const bool force = forceRender_;
  bool changed = false;
  switch (state_.mode) {
    case EffectMode::Static:
      if (force) { clear(frame); renderStatic(frame); changed = true; }
      break;
    case EffectMode::Off:
      if (force) { clear(frame); changed = true; }
      break;
    case EffectMode::Rainbow:
      if (due(nowMs, rainbowLastUpdate_,
              static_cast<uint16_t>(mapRange(state_.lighting.flowSpeed, 1, 100, 58, 12)), force)) {
        renderRainbow(frame); changed = true;
      }
      break;
    case EffectMode::Breathe:
      if (due(nowMs, breatheLastUpdate_, 20U, force)) {
        renderBreathe(nowMs, frame); changed = true;
      }
      break;
    case EffectMode::Music:
      if (due(nowMs, musicLastUpdate_, 25U, force)) {
        renderMusic(audio, frame); changed = true;
      }
      break;
    case EffectMode::Comet:
      if (due(nowMs, cometLastUpdate_,
              static_cast<uint16_t>(mapRange(state_.lighting.cometSpeed, 1, 50, 75, 12)), force)) {
        renderComet(frame); changed = true;
      }
      break;
    case EffectMode::Aurora:
      if (due(nowMs, auroraLastUpdate_,
              static_cast<uint16_t>(mapRange(state_.lighting.flowSpeed, 1, 100, 58, 14)), force)) {
        renderAurora(frame); changed = true;
      }
      break;
    case EffectMode::FlameAudio:
      if (due(nowMs, flameLastUpdate_, 25U, force)) {
        renderFlameAudio(audio, frame); changed = true;
      }
      break;
    case EffectMode::GlassFlow:
      if (due(nowMs, glassFlowLastUpdate_,
              static_cast<uint16_t>(mapRange(state_.lighting.flowSpeed, 1, 100, 58, 14)), force)) {
        renderGlassFlow(frame); changed = true;
      }
      break;
    case EffectMode::CornerPulse:
      if (due(nowMs, cornerPulseLastUpdate_, 25U, force)) {
        renderCornerPulse(frame); changed = true;
      }
      break;
    case EffectMode::BassRipple:
      if (due(nowMs, bassRippleLastUpdate_, 25U, force)) {
        renderBassRipple(audio, frame); changed = true;
      }
      break;
    case EffectMode::Focus:
      if (due(nowMs, focusLastUpdate_, 120U, force)) {
        renderFocus(frame); changed = true;
      }
      break;
  }
  forceRender_ = false;
  resetFrame_ = false;
  return changed;
}

Rgb LightingRenderer::hsv(uint16_t hue, const uint8_t saturation,
                          const uint8_t value) {
  hue = static_cast<uint16_t>((static_cast<uint32_t>(hue) * 1530UL + 32768UL) / 65536UL);
  uint8_t red = 0U, green = 0U, blue = 0U;
  if (hue < 510U) {
    if (hue < 255U) { red = 255U; green = static_cast<uint8_t>(hue); }
    else { red = static_cast<uint8_t>(510U - hue); green = 255U; }
  } else if (hue < 1020U) {
    if (hue < 765U) { green = 255U; blue = static_cast<uint8_t>(hue - 510U); }
    else { green = static_cast<uint8_t>(1020U - hue); blue = 255U; }
  } else if (hue < 1530U) {
    if (hue < 1275U) { red = static_cast<uint8_t>(hue - 1020U); blue = 255U; }
    else { red = 255U; blue = static_cast<uint8_t>(1530U - hue); }
  } else {
    red = 255U;
  }
  const uint32_t valueScale = 1U + value;
  const uint16_t saturationScale = static_cast<uint16_t>(1U + saturation);
  const uint8_t inverseSaturation = static_cast<uint8_t>(255U - saturation);
  const auto channel = [&](const uint8_t input) {
    return static_cast<uint8_t>((((input * saturationScale) >> 8U) +
                                 inverseSaturation) * valueScale >> 8U);
  };
  return {channel(red), channel(green), channel(blue)};
}

Rgb LightingRenderer::gamma(const Rgb color) {
  return {kGammaTable[color.red], kGammaTable[color.green], kGammaTable[color.blue]};
}

uint32_t LightingRenderer::mapRange(const int32_t value, const int32_t inMin,
                                    const int32_t inMax, const int32_t outMin,
                                    const int32_t outMax) {
  return static_cast<uint32_t>((value - inMin) * (outMax - outMin) /
                               (inMax - inMin) + outMin);
}

float LightingRenderer::clampFloat(const float value, const float minimum,
                                   const float maximum) {
  return std::max(minimum, std::min(value, maximum));
}

uint32_t LightingRenderer::nextRandom() {
  randomState_ = randomState_ * 1664525UL + 1013904223UL;
  return randomState_;
}

void LightingRenderer::clear(LightingFrame &frame) const {
  frame.count = logicalLedCount_;
  for (uint16_t i = 0; i < logicalLedCount_; ++i) frame.pixels[i] = {0U, 0U, 0U};
}

void LightingRenderer::setPixel(LightingFrame &frame, const uint16_t index,
                                const Rgb color) const {
  if (index < logicalLedCount_) frame.pixels[index] = scaleRgb(color, state_.brightness);
}

bool LightingRenderer::due(const uint32_t nowMs, uint32_t &lastUpdateMs,
                           const uint16_t intervalMs, const bool force) const {
  if (!force && static_cast<uint32_t>(nowMs - lastUpdateMs) < intervalMs) return false;
  lastUpdateMs = nowMs;
  return true;
}

void LightingRenderer::renderStatic(LightingFrame &frame) const {
  for (uint16_t i = 0; i < logicalLedCount_; ++i) setPixel(frame, i, state_.primaryColor);
}

void LightingRenderer::renderRainbow(LightingFrame &frame) {
  const uint8_t saturation = state_.lighting.rainbowStyle == 0U ? 180U : 255U;
  const uint8_t cycles = rainbowCycleCount(state_.lighting.rainbowStyle);
  for (uint16_t i = 0; i < logicalLedCount_; ++i) {
    setPixel(frame, i, gamma(hsv(rainbowHueForPixel(rainbowFirstHue_, i,
                                                    logicalLedCount_, cycles),
                                 saturation, 255U)));
  }
  rainbowFirstHue_ = static_cast<uint16_t>(rainbowFirstHue_ +
      mapRange(state_.lighting.flowSpeed, 1, 100, 64, 700));
}

void LightingRenderer::renderBreathe(const uint32_t nowMs,
                                     LightingFrame &frame) const {
  const float phase = static_cast<float>(nowMs % 3000UL) / 3000.0F;
  const float wave = (std::sin(phase * 2.0F * kPi - kPi * 0.5F) + 1.0F) * 0.5F;
  const float floor = state_.lighting.breathFloorPercent / 100.0F;
  const float amount = floor + wave * (1.0F - floor);
  const Rgb color{static_cast<uint8_t>(state_.primaryColor.red * amount),
                  static_cast<uint8_t>(state_.primaryColor.green * amount),
                  static_cast<uint8_t>(state_.primaryColor.blue * amount)};
  for (uint16_t i = 0; i < logicalLedCount_; ++i) setPixel(frame, i, color);
}

void LightingRenderer::renderMusic(const AudioFrame &audio,
                                   LightingFrame &frame) const {
  const int center = state_.layout.centerIndex;
  const int maxRadius = std::max(center + 1, static_cast<int>(logicalLedCount_) - center);
  const float sensitivity = state_.lighting.audioSensitivityX100 / 100.0F;
  const float reactive = std::max(audio.volume * sensitivity, audio.beatPulse);
  const int radius = static_cast<int>(mapRange(static_cast<int32_t>(clampFloat(reactive, 0.0F, 255.0F)), 0, 255, 0, maxRadius));
  float hueDrive = audio.volume;
  if (state_.lighting.audioHueDrive == 1U) hueDrive = audio.slowEnergy;
  else if (state_.lighting.audioHueDrive == 2U) hueDrive = (audio.fastEnergy + audio.slowEnergy) * 0.5F;
  else if (state_.lighting.audioHueDrive == 3U) hueDrive = std::fabs(audio.fastEnergy - audio.slowEnergy) * 3.0F;
  const uint8_t mix = static_cast<uint8_t>(clampFloat(
      hueDrive * state_.lighting.audioColorGainX100 / 100.0F, 0.0F, 255.0F));
  clear(frame);
  for (int distance = 0; distance < radius; ++distance) {
    Rgb color{};
    switch (state_.audioColorStyle) {
      case 1: color = state_.primaryColor; break;
      case 2: color = gamma(hsv(static_cast<uint16_t>(mapRange(mix, 0, 255, 9000, 0)), 240U, 230U)); break;
      case 3: color = gamma(hsv(static_cast<uint16_t>(mapRange(mix, 0, 255, 35000, 44000)), 220U, 230U)); break;
      case 4: color = gamma(hsv(static_cast<uint16_t>(mapRange(mix, 0, 255, 48000, 58000)), 210U, 230U)); break;
      default: color = gamma(hsv(static_cast<uint16_t>(mapRange(mix, 0, 255, 24500, 0)), 220U, 230U)); break;
    }
    if (audio.beatPulse > 0.0F) color = blendRgb(color, {255U,255U,255U}, static_cast<uint8_t>(clampFloat(audio.beatPulse * 0.20F, 0.0F, 255.0F)));
    const int left = center - distance;
    const int right = center + distance;
    if (left >= 0) setPixel(frame, static_cast<uint16_t>(left), color);
    if (right < logicalLedCount_) setPixel(frame, static_cast<uint16_t>(right), color);
  }
}

void LightingRenderer::renderComet(LightingFrame &frame) {
  const uint8_t fade = static_cast<uint8_t>(mapRange(state_.lighting.cometTail, 5, 80, 90, 12));
  const uint8_t retain = static_cast<uint8_t>(255U - fade);
  const Rgb background = scaleRgb(state_.primaryColor,
      static_cast<uint8_t>(mapRange(state_.lighting.cometBackground, 0, 60, 0, 48)));
  for (uint16_t i = 0; i < logicalLedCount_; ++i) {
    const Rgb faded = scaleRgb(frame.pixels[i], retain);
    frame.pixels[i] = {std::max(faded.red, scaleRgb(background, state_.brightness).red),
                       std::max(faded.green, scaleRgb(background, state_.brightness).green),
                       std::max(faded.blue, scaleRgb(background, state_.brightness).blue)};
  }
  Rgb cometColor = state_.primaryColor;
  if (state_.cometColorStyle == 1U) { cometColor = gamma(hsv(cometHue_, 220U, 255U)); cometHue_ += 512U; }
  else if (state_.cometColorStyle == 2U) cometColor = {255U,80U,24U};
  else if (state_.cometColorStyle == 3U) cometColor = {40U,170U,255U};
  else if (state_.cometColorStyle == 4U) cometColor = {190U,80U,255U};
  const uint8_t density = std::max<uint8_t>(1U, state_.lighting.cometDensity);
  for (uint8_t particle = 0; particle < density; ++particle) {
    const uint16_t spacing = std::max<uint16_t>(1U, logicalLedCount_ / density);
    const uint16_t position = static_cast<uint16_t>((static_cast<uint16_t>(cometPhase_) + particle * spacing) % logicalLedCount_);
    Rgb color = cometColor;
    if (state_.lighting.cometRandom) color = blendRgb(cometColor, hsv(static_cast<uint16_t>(nextRandom()), 180U, 255U), 52U);
    setPixel(frame, position, color);
  }
  cometPhase_ += 0.03F + (state_.lighting.cometSpeed - 1U) * 3.47F / 49.0F;
  while (cometPhase_ >= logicalLedCount_) cometPhase_ -= logicalLedCount_;
}

void LightingRenderer::renderAurora(LightingFrame &frame) {
  auroraPhase_ += static_cast<uint16_t>(mapRange(state_.lighting.flowSpeed, 1, 100, 1, 5));
  const Rgb aqua = blendRgb(state_.primaryColor, {35U,220U,185U}, 105U);
  const Rgb violet{125U,90U,235U};
  for (uint16_t i = 0; i < logicalLedCount_; ++i) {
    const float position = static_cast<float>(i) * 2.0F * kPi / logicalLedCount_;
    const float wave = (std::sin(position + auroraPhase_ * 0.025F) + 1.0F) * 0.5F;
    setPixel(frame, i, scaleRgb(blendRgb(aqua, violet, static_cast<uint8_t>(wave * 255.0F)), static_cast<uint8_t>(75.0F + wave * 180.0F)));
  }
}

void LightingRenderer::renderFlameAudio(const AudioFrame &audio,
                                        LightingFrame &frame) {
  flamePhase_ += 3U;
  const float sensitivity = state_.lighting.audioSensitivityX100 / 100.0F;
  const uint8_t level = static_cast<uint8_t>(clampFloat(std::max(audio.volume * sensitivity, audio.beatPulse), 0.0F, 255.0F));
  const uint16_t center = state_.layout.centerIndex;
  const uint16_t longest = std::max<uint16_t>(center + 1U, logicalLedCount_ - center);
  const uint16_t active = static_cast<uint16_t>(mapRange(level, 0, 255, 0, longest));
  clear(frame);
  for (uint16_t distance = 0; distance < active; ++distance) {
    const float flicker = (std::sin((distance * 17.0F + flamePhase_) * 0.075F) + 1.0F) * 0.5F;
    const uint8_t value = static_cast<uint8_t>(clampFloat(45.0F + level * 0.65F + audio.beatPulse * 0.14F, 0.0F, 255.0F));
    const Rgb color = gamma(hsv(static_cast<uint16_t>(flicker * 35.0F * 256.0F), 245U, value));
    if (distance <= center) setPixel(frame, distance, color);
    const uint16_t right = logicalLedCount_ - 1U - distance;
    if (right >= center) setPixel(frame, right, color);
  }
}

void LightingRenderer::renderGlassFlow(LightingFrame &frame) {
  glassFlowPhase_ += static_cast<uint16_t>(mapRange(state_.lighting.flowSpeed, 1, 100, 1, 5));
  const Rgb secondary{state_.lighting.secondaryRed, state_.lighting.secondaryGreen, state_.lighting.secondaryBlue};
  for (uint16_t i = 0; i < logicalLedCount_; ++i) {
    const float wave = (std::sin(i * 0.22F - glassFlowPhase_ * 0.08F) + 1.0F) * 0.5F;
    setPixel(frame, i, scaleRgb(blendRgb(state_.primaryColor, secondary, static_cast<uint8_t>(wave * 255.0F)), static_cast<uint8_t>(80.0F + wave * 175.0F)));
  }
}

void LightingRenderer::renderCornerPulse(LightingFrame &frame) {
  cornerPulsePhase_ += 3U;
  const uint16_t edge = static_cast<uint16_t>(mapRange(state_.lighting.pulseHeightPercent, 0, 100, 1, std::max<uint16_t>(1U, logicalLedCount_ / 2U)));
  const uint8_t minimum = static_cast<uint8_t>(mapRange(state_.lighting.pulseAmplitudePercent, 0, 100, 255, 0));
  const uint8_t pulse = static_cast<uint8_t>(minimum + (255U - minimum) * ((std::sin(cornerPulsePhase_ * 0.08F) + 1.0F) * 0.5F));
  const Rgb color = scaleRgb(state_.primaryColor, pulse);
  clear(frame);
  for (uint16_t i = 0; i < edge; ++i) { setPixel(frame, i, color); setPixel(frame, logicalLedCount_ - 1U - i, color); }
}

void LightingRenderer::renderBassRipple(const AudioFrame &audio,
                                        LightingFrame &frame) {
  const float sensitivity = state_.lighting.audioSensitivityX100 / 100.0F;
  const uint8_t level = static_cast<uint8_t>(clampFloat(std::max(audio.volume * sensitivity, audio.beatPulse), 0.0F, 255.0F));
  bassRipplePhase_ += static_cast<uint16_t>(2U + audio.beatPulse / 64.0F);
  const int center = state_.layout.centerIndex;
  const int radius = static_cast<int>(mapRange(level, 0, 255, 1, std::max(center + 1, static_cast<int>(logicalLedCount_) - center)));
  const Rgb highlight = blendRgb(state_.primaryColor, {255U,255U,255U}, static_cast<uint8_t>(clampFloat(audio.beatPulse * 0.22F, 0.0F, 255.0F)));
  clear(frame);
  for (uint16_t i = 0; i < logicalLedCount_; ++i) {
    const int distance = std::abs(static_cast<int>(i) - center);
    if (distance > radius) continue;
    const float wave = (std::sin(distance * 0.60F - bassRipplePhase_ * 0.10F) + 1.0F) * 0.5F;
    const uint8_t amount = static_cast<uint8_t>(25.0F + wave *
        (static_cast<int>(state_.lighting.animationBrightness) - 25));
    setPixel(frame, i, scaleRgb(highlight, amount));
  }
}

void LightingRenderer::renderFocus(LightingFrame &frame) const {
  const Rgb color = scaleRgb(blendRgb(state_.primaryColor, {255U,255U,255U}, 28U), 92U);
  for (uint16_t i = 0; i < logicalLedCount_; ++i) setPixel(frame, i, color);
}

}  // namespace sozo
