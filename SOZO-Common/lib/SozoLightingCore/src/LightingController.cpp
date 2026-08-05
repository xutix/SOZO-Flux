#include "LightingController.h"

#include <algorithm>

namespace sozo {
namespace {

bool sameLayout(const spatial_light::SpatialLayout &left,
                const spatial_light::SpatialLayout &right) {
  return left.profile == right.profile && left.activeCount == right.activeCount &&
         left.centerIndex == right.centerIndex && left.leftCount == right.leftCount &&
         left.centerCount == right.centerCount && left.rightCount == right.rightCount &&
         left.reversed == right.reversed;
}

}  // namespace

LightingController::LightingController(lighting::LedOutput &output)
    : output_(output), renderer_(spatial_light::kDefaultLedCount) {}

void LightingController::begin(const PersistedLightingState &state) {
  state_ = normalize(state);
  modeBeforeOff_ = state_.mode == EffectMode::Off ? EffectMode::Static : state_.mode;
  renderer_.setLogicalLedCount(state_.layout.activeCount);
  renderer_.setState(state_);
  output_.begin(geometry());
  frame_.count = state_.layout.activeCount;
  clearFrame();
  started_ = true;
  startupAnimationActive_ = true;
  startupProgress_ = 0.0F;
  startupLastFrame_ = 0U;
  manualLitPixelCount_ = -1;
}

bool LightingController::apply(const ControlCommand &command) {
  if (!isCommandWellFormed(command)) return false;
  PersistedLightingState next = state_;
  switch (command.type) {
    case ControlCommandType::SetEffect:
      next.mode = static_cast<EffectMode>(command.value);
      break;
    case ControlCommandType::SetParameter:
      switch (command.parameter) {
        case LightingParameter::Brightness: next.brightness = static_cast<uint8_t>(command.value); break;
        case LightingParameter::PrimaryColor: next.primaryColor = command.color; break;
        case LightingParameter::RainbowStyle: next.lighting.rainbowStyle = static_cast<uint8_t>(command.value); break;
        case LightingParameter::FlowSpeed: next.lighting.flowSpeed = static_cast<uint8_t>(command.value); break;
        case LightingParameter::CometTail: next.lighting.cometTail = static_cast<uint8_t>(command.value); break;
        case LightingParameter::CometSpeed: next.lighting.cometSpeed = static_cast<uint8_t>(command.value); break;
        case LightingParameter::CometDensity: next.lighting.cometDensity = static_cast<uint8_t>(command.value); break;
        case LightingParameter::CometBackground: next.lighting.cometBackground = static_cast<uint8_t>(command.value); break;
        case LightingParameter::CometRandom: next.lighting.cometRandom = command.value != 0; break;
        case LightingParameter::AudioSensitivity: next.lighting.audioSensitivityX100 = static_cast<uint16_t>(command.value); break;
        case LightingParameter::AudioColorGain: next.lighting.audioColorGainX100 = static_cast<uint16_t>(command.value); break;
        case LightingParameter::AudioHueDrive: next.lighting.audioHueDrive = static_cast<uint8_t>(command.value); break;
        case LightingParameter::BreathFloor: next.lighting.breathFloorPercent = static_cast<uint8_t>(command.value); break;
        case LightingParameter::SecondaryColor:
          next.lighting.secondaryRed = command.color.red;
          next.lighting.secondaryGreen = command.color.green;
          next.lighting.secondaryBlue = command.color.blue;
          break;
        case LightingParameter::PulseAmplitude: next.lighting.pulseAmplitudePercent = static_cast<uint8_t>(command.value); break;
        case LightingParameter::PulseHeight: next.lighting.pulseHeightPercent = static_cast<uint8_t>(command.value); break;
        case LightingParameter::AnimationBrightness: next.lighting.animationBrightness = static_cast<uint8_t>(command.value); break;
        case LightingParameter::AudioColorStyle: next.audioColorStyle = static_cast<uint8_t>(command.value); break;
        case LightingParameter::CometColorStyle: next.cometColorStyle = static_cast<uint8_t>(command.value); break;
        case LightingParameter::StartupColor: next.startupColor = command.color; break;
        case LightingParameter::StartupAnimationSpeed: next.startupAnimationSpeed = command.value / 100.0F; break;
        case LightingParameter::ManualLitPixelCount:
          setLitPixelCount(static_cast<uint16_t>(command.value));
          return true;
        case LightingParameter::None:
        default:
          return false;
      }
      break;
    case ControlCommandType::TurnOff:
      next.mode = EffectMode::Off;
      break;
    case ControlCommandType::TurnOn:
      next.mode = modeBeforeOff_;
      break;
    case ControlCommandType::SetLayout:
      next.layout = command.layout;
      break;
    case ControlCommandType::AdjustParameter:
    case ControlCommandType::DeviceAction:
    default:
      return false;
  }
  setState(next);
  return true;
}

void LightingController::setState(const PersistedLightingState &state) {
  const PersistedLightingState next = normalize(state);
  const bool layoutChanged = !sameLayout(state_.layout, next.layout);
  if (next.mode == EffectMode::Off) {
    if (state_.mode != EffectMode::Off) modeBeforeOff_ = state_.mode;
  } else {
    modeBeforeOff_ = next.mode;
  }
  state_ = next;
  renderer_.setLogicalLedCount(state_.layout.activeCount);
  renderer_.setState(state_);
  manualLitPixelCount_ = -1;
  if (started_ && layoutChanged) {
    frame_.count = state_.layout.activeCount;
    clearFrame();
    output_.begin(geometry());
  }
}

const PersistedLightingState &LightingController::state() const { return state_; }

PersistedLightingState LightingController::persistedState() const {
  PersistedLightingState persisted = state_;
  persisted.mode = state_.mode == EffectMode::Off ? modeBeforeOff_ : state_.mode;
  return persisted;
}

void LightingController::setLitPixelCount(const uint16_t count) {
  const uint16_t limited = std::min(count, state_.layout.activeCount);
  state_.mode = EffectMode::Static;
  modeBeforeOff_ = EffectMode::Static;
  manualLitPixelCount_ = static_cast<int16_t>(limited);
  clearFrame();
  const Rgb color = scaleRgb(state_.primaryColor, state_.brightness);
  for (uint16_t index = 0; index < limited; ++index) frame_.pixels[index] = color;
  renderer_.setState(state_);
  if (started_) present();
}

void LightingController::tick(const uint32_t nowMs, const AudioFrame &audio) {
  if (!started_) return;
  if (startupAnimationActive_) {
    renderStartupAnimation(nowMs);
    return;
  }
  if (manualLitPixelCount_ >= 0 && state_.mode == EffectMode::Static) return;
  if (renderer_.render(nowMs, audio, frame_)) present();
}

LightingSnapshot LightingController::snapshot() const {
  return {state_.mode, state_.layout.activeCount, startupAnimationActive_,
          manualLitPixelCount_};
}

PersistedLightingState LightingController::normalize(PersistedLightingState state) {
  state.layout = spatial_light::normalizeLayout(state.layout);
  if (!isKnownEffect(state.mode) && state.mode != EffectMode::Off) state.mode = EffectMode::Static;
  return state;
}

lighting::LedGeometry LightingController::geometry() const {
  lighting::LedGeometry value{};
  value.activeLedCount = state_.layout.activeCount;
  value.physicalLedCount = state_.layout.activeCount;
  value.reversed = state_.layout.reversed;
  return value;
}

void LightingController::renderStartupAnimation(const uint32_t nowMs) {
  if (static_cast<uint32_t>(nowMs - startupLastFrame_) < 25U) return;
  startupLastFrame_ = nowMs;
  startupProgress_ += state_.startupAnimationSpeed;
  const int center = state_.layout.centerIndex;
  const int radius = std::max(static_cast<int>(startupProgress_), 0);
  const int maxRadius = std::max(center + 1,
      static_cast<int>(state_.layout.activeCount) - center);
  clearFrame();
  const Rgb color = scaleRgb(state_.startupColor, state_.brightness);
  for (int distance = 0; distance < std::min(radius, maxRadius); ++distance) {
    const int left = center - distance;
    const int right = center + distance;
    if (left >= 0) frame_.pixels[left] = color;
    if (right < state_.layout.activeCount) frame_.pixels[right] = color;
  }
  present();
  if (radius >= maxRadius) {
    startupAnimationActive_ = false;
    startupProgress_ = 0.0F;
    clearFrame();
    present();
  }
}

void LightingController::present() { output_.present(geometry(), frame_); }

void LightingController::clearFrame() {
  frame_.count = state_.layout.activeCount;
  for (uint16_t index = 0; index < frame_.count; ++index) {
    frame_.pixels[index] = {0U, 0U, 0U};
  }
}

}  // namespace sozo
