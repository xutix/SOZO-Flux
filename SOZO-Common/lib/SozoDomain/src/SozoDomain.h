#pragma once

#include <stdint.h>

#include <SpatialLightCore.h>

namespace sozo {

constexpr uint8_t kControlProtocolVersion = 1;
enum class EffectMode : uint8_t { Static, Rainbow, Breathe, Music, Comet, Aurora, FlameAudio, GlassFlow, CornerPulse, BassRipple, Focus, Off };
enum class ControlSource : uint8_t { Web, Serial, WiFiNode, EspNowNode, BlePhone, BleNode, Voice, PcAudio };
enum class ControlCommandType : uint8_t { SetEffect, SetParameter, AdjustParameter, SetLayout, TurnOff, TurnOn, DeviceAction };
enum class LightingParameter : uint8_t { None, Brightness, PrimaryColor, RainbowStyle, FlowSpeed, CometTail, CometSpeed, CometDensity, CometBackground, CometRandom, AudioSensitivity, AudioColorGain, AudioHueDrive, BreathFloor, SecondaryColor, PulseAmplitude, PulseHeight, AnimationBrightness, AudioColorStyle, CometColorStyle, StartupColor, StartupAnimationSpeed, ManualLitPixelCount };

struct Rgb { uint8_t red; uint8_t green; uint8_t blue; };

struct LightingSettings {
  uint8_t rainbowStyle, flowSpeed, cometTail, cometSpeed, cometDensity, cometBackground;
  bool cometRandom;
  uint16_t audioSensitivityX100, audioColorGainX100;
  uint8_t audioHueDrive, breathFloorPercent, secondaryRed, secondaryGreen,
      secondaryBlue, pulseAmplitudePercent, pulseHeightPercent, animationBrightness;
  constexpr LightingSettings() : rainbowStyle(0), flowSpeed(45), cometTail(28), cometSpeed(45), cometDensity(1), cometBackground(0), cometRandom(false), audioSensitivityX100(100), audioColorGainX100(100), audioHueDrive(0), breathFloorPercent(16), secondaryRed(240), secondaryGreen(168), secondaryBlue(90), pulseAmplitudePercent(84), pulseHeightPercent(10), animationBrightness(220) {}
};
constexpr LightingSettings makeDefaultLightingSettings() { return LightingSettings(); }

struct AudioTuning {
  float noiseFloor, fullScale, gain, attack, release, beatSensitivity, beatBoost;
  constexpr AudioTuning() : noiseFloor(10000.0F), fullScale(300000.0F), gain(1.0F), attack(0.90F), release(0.15F), beatSensitivity(1.45F), beatBoost(150.0F) {}
};

struct AudioFrame {
  float volume, rawRms, fastEnergy, slowEnergy, beatPulse;
  uint32_t framesRead;
  bool available;
  constexpr AudioFrame(float volumeValue = 0.0F, float rawRmsValue = 0.0F,
                       float fastEnergyValue = 0.0F, float slowEnergyValue = 0.0F,
                       float beatPulseValue = 0.0F, uint32_t framesReadValue = 0,
                       bool availableValue = false)
      : volume(volumeValue), rawRms(rawRmsValue), fastEnergy(fastEnergyValue),
        slowEnergy(slowEnergyValue), beatPulse(beatPulseValue),
        framesRead(framesReadValue), available(availableValue) {}
};

constexpr spatial_light::SpatialLayout makeDefaultSpatialLayout() {
  return {spatial_light::LayoutProfile::Continuous, spatial_light::kDefaultLedCount,
          static_cast<uint16_t>((spatial_light::kDefaultLedCount - 1) / 2), 0,
          spatial_light::kDefaultLedCount, 0, false};
}

struct PersistedLightingState {
  EffectMode mode;
  uint8_t brightness;
  Rgb primaryColor;
  uint8_t audioColorStyle, cometColorStyle;
  LightingSettings lighting;
  spatial_light::SpatialLayout layout;
  Rgb startupColor;
  float startupAnimationSpeed;
  AudioTuning audio;
  constexpr PersistedLightingState() : mode(EffectMode::Static), brightness(50), primaryColor{255, 120, 0}, audioColorStyle(0), cometColorStyle(0), lighting(), layout(makeDefaultSpatialLayout()), startupColor{50, 120, 220}, startupAnimationSpeed(0.8F), audio() {}
};
constexpr PersistedLightingState makeDefaultPersistedLightingState() { return PersistedLightingState(); }

struct ControlCommand {
  uint8_t protocolVersion;
  ControlSource source;
  uint32_t sourceId;
  ControlCommandType type;
  LightingParameter parameter;
  int32_t value;
  Rgb color;
  spatial_light::SpatialLayout layout;
};

constexpr bool isKnownEffect(const EffectMode mode) { return mode >= EffectMode::Static && mode <= EffectMode::Focus; }
constexpr bool isKnownControlSource(const ControlSource source) { return source >= ControlSource::Web && source <= ControlSource::PcAudio; }
constexpr bool isKnownControlCommandType(const ControlCommandType type) { return type >= ControlCommandType::SetEffect && type <= ControlCommandType::DeviceAction; }
constexpr bool isSourceAllowedForCommand(const ControlSource source, const ControlCommandType type) {
  return !isKnownControlSource(source) || !isKnownControlCommandType(type) ? false
      : (type == ControlCommandType::DeviceAction ? (source == ControlSource::Web || source == ControlSource::Serial)
      : (source == ControlSource::PcAudio ? type == ControlCommandType::SetParameter : true));
}
constexpr int32_t clampValue(const int32_t value, const int32_t minimum, const int32_t maximum) { return value < minimum ? minimum : (value > maximum ? maximum : value); }
constexpr int32_t minimumValueFor(const LightingParameter parameter) {
  return parameter == LightingParameter::FlowSpeed ? 1 : parameter == LightingParameter::CometTail ? 5 : parameter == LightingParameter::CometSpeed ? 1 : parameter == LightingParameter::CometDensity ? 1 : parameter == LightingParameter::AudioSensitivity ? 10 : parameter == LightingParameter::StartupAnimationSpeed ? 10 : 0;
}
constexpr int32_t maximumValueFor(const LightingParameter parameter) {
  return parameter == LightingParameter::Brightness ? 255 : parameter == LightingParameter::PrimaryColor ? 0xFFFFFF : parameter == LightingParameter::RainbowStyle ? 2 : parameter == LightingParameter::FlowSpeed ? 100 : parameter == LightingParameter::CometTail ? 80 : parameter == LightingParameter::CometSpeed ? 50 : parameter == LightingParameter::CometDensity ? 8 : parameter == LightingParameter::CometBackground ? 60 : parameter == LightingParameter::CometRandom ? 1 : parameter == LightingParameter::AudioSensitivity ? 500 : parameter == LightingParameter::AudioColorGain ? 500 : parameter == LightingParameter::AudioHueDrive ? 3 : parameter == LightingParameter::BreathFloor ? 60 : parameter == LightingParameter::SecondaryColor ? 0xFFFFFF : parameter == LightingParameter::PulseAmplitude ? 100 : parameter == LightingParameter::PulseHeight ? 100 : parameter == LightingParameter::AnimationBrightness ? 255 : parameter == LightingParameter::AudioColorStyle ? 4 : parameter == LightingParameter::CometColorStyle ? 4 : parameter == LightingParameter::StartupColor ? 0xFFFFFF : parameter == LightingParameter::StartupAnimationSpeed ? 500 : parameter == LightingParameter::ManualLitPixelCount ? spatial_light::kMaxLedCount : 0;
}
constexpr int32_t clampLightingValue(const LightingParameter parameter, const int32_t value) { return clampValue(value, minimumValueFor(parameter), maximumValueFor(parameter)); }
constexpr bool isKnownLightingParameter(const LightingParameter parameter) { return parameter >= LightingParameter::Brightness && parameter <= LightingParameter::ManualLitPixelCount; }
constexpr bool isLayoutCommandWellFormed(const spatial_light::SpatialLayout &layout) {
  return layout.activeCount > 0 && layout.activeCount <= spatial_light::kMaxLedCount &&
         (layout.profile == spatial_light::LayoutProfile::Continuous ? layout.centerIndex < layout.activeCount : spatial_light::isValidSegmentedLayout(layout.activeCount, layout.leftCount, layout.centerCount, layout.rightCount));
}
constexpr bool isParameterCommandWellFormed(const uint8_t protocolVersion, const ControlSource source, const LightingParameter parameter, const int32_t value) {
  return protocolVersion == kControlProtocolVersion && isKnownControlSource(source) && isKnownLightingParameter(parameter) && value == clampLightingValue(parameter, value);
}
inline bool isCommandWellFormed(const ControlCommand &command) {
  if (command.type == ControlCommandType::SetParameter) return isParameterCommandWellFormed(command.protocolVersion, command.source, command.parameter, command.value);
  if (command.protocolVersion != kControlProtocolVersion || !isKnownControlSource(command.source)) return false;
  if (command.type == ControlCommandType::SetEffect) return command.parameter == LightingParameter::None && isKnownEffect(static_cast<EffectMode>(command.value));
  if (command.type == ControlCommandType::SetLayout) return command.parameter == LightingParameter::None && isLayoutCommandWellFormed(command.layout);
  if (command.type == ControlCommandType::AdjustParameter) return isKnownLightingParameter(command.parameter) && command.value >= -255 && command.value <= 255;
  return command.type == ControlCommandType::TurnOff || command.type == ControlCommandType::TurnOn || command.type == ControlCommandType::DeviceAction;
}

}  // namespace sozo
