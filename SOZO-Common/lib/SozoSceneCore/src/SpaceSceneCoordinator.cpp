#include "SpaceSceneCoordinator.h"

namespace sozo {

void SpaceSceneCoordinator::begin(const PersistedLightingState &state) {
  PersistedLightingState normalized = state;
  normalized.layout = spatial_light::normalizeLayout(normalized.layout);
  scene_.lighting = makeLightingScene(normalized);
  scene_.revision = 1U;
  localLight_.layout = normalized.layout;
  localLight_.startupColor = normalized.startupColor;
  localLight_.startupAnimationSpeed = normalized.startupAnimationSpeed;
  localLight_.revision = 1U;
  audio_ = normalized.audio;
  modeBeforeOff_ = normalized.mode == EffectMode::Off ? EffectMode::Static
                                                       : normalized.mode;
}

bool SpaceSceneCoordinator::apply(const ControlCommand &command) {
  if (!isCommandWellFormed(command) ||
      !isSourceAllowedForCommand(command.source, command.type)) {
    return false;
  }
  switch (command.type) {
    case ControlCommandType::SetEffect:
      scene_.lighting.mode = static_cast<EffectMode>(command.value);
      modeBeforeOff_ = scene_.lighting.mode;
      scene_.lighting.manualLitPixelCount = -1;
      break;
    case ControlCommandType::SetParameter: {
      LightingScene &scene = scene_.lighting;
      switch (command.parameter) {
        case LightingParameter::Brightness:
          scene.brightness = static_cast<uint8_t>(command.value);
          break;
        case LightingParameter::PrimaryColor:
          scene.primaryColor = command.color;
          break;
        case LightingParameter::RainbowStyle:
          scene.settings.rainbowStyle = static_cast<uint8_t>(command.value);
          break;
        case LightingParameter::FlowSpeed:
          scene.settings.flowSpeed = static_cast<uint8_t>(command.value);
          break;
        case LightingParameter::CometTail:
          scene.settings.cometTail = static_cast<uint8_t>(command.value);
          break;
        case LightingParameter::CometSpeed:
          scene.settings.cometSpeed = static_cast<uint8_t>(command.value);
          break;
        case LightingParameter::CometDensity:
          scene.settings.cometDensity = static_cast<uint8_t>(command.value);
          break;
        case LightingParameter::CometBackground:
          scene.settings.cometBackground =
              static_cast<uint8_t>(command.value);
          break;
        case LightingParameter::CometRandom:
          scene.settings.cometRandom = command.value != 0;
          break;
        case LightingParameter::AudioSensitivity:
          scene.settings.audioSensitivityX100 =
              static_cast<uint16_t>(command.value);
          break;
        case LightingParameter::AudioColorGain:
          scene.settings.audioColorGainX100 =
              static_cast<uint16_t>(command.value);
          break;
        case LightingParameter::AudioHueDrive:
          scene.settings.audioHueDrive = static_cast<uint8_t>(command.value);
          break;
        case LightingParameter::BreathFloor:
          scene.settings.breathFloorPercent =
              static_cast<uint8_t>(command.value);
          break;
        case LightingParameter::SecondaryColor:
          scene.settings.secondaryRed = command.color.red;
          scene.settings.secondaryGreen = command.color.green;
          scene.settings.secondaryBlue = command.color.blue;
          break;
        case LightingParameter::PulseAmplitude:
          scene.settings.pulseAmplitudePercent =
              static_cast<uint8_t>(command.value);
          break;
        case LightingParameter::PulseHeight:
          scene.settings.pulseHeightPercent =
              static_cast<uint8_t>(command.value);
          break;
        case LightingParameter::AnimationBrightness:
          scene.settings.animationBrightness =
              static_cast<uint8_t>(command.value);
          break;
        case LightingParameter::AudioColorStyle:
          scene.audioColorStyle = static_cast<uint8_t>(command.value);
          break;
        case LightingParameter::CometColorStyle:
          scene.cometColorStyle = static_cast<uint8_t>(command.value);
          break;
        case LightingParameter::StartupColor:
          localLight_.startupColor = command.color;
          bumpLocalLightRevision();
          return true;
        case LightingParameter::StartupAnimationSpeed:
          localLight_.startupAnimationSpeed = command.value / 100.0F;
          bumpLocalLightRevision();
          return true;
        case LightingParameter::ManualLitPixelCount:
          scene.mode = EffectMode::Static;
          modeBeforeOff_ = EffectMode::Static;
          scene.manualLitPixelCount = static_cast<int16_t>(command.value);
          bumpSceneRevision();
          return true;
        case LightingParameter::None:
        default:
          return false;
      }
      scene.manualLitPixelCount = -1;
      break;
    }
    case ControlCommandType::TurnOff:
      if (scene_.lighting.mode != EffectMode::Off) {
        modeBeforeOff_ = scene_.lighting.mode;
      }
      scene_.lighting.mode = EffectMode::Off;
      scene_.lighting.manualLitPixelCount = -1;
      break;
    case ControlCommandType::TurnOn:
      scene_.lighting.mode = modeBeforeOff_;
      scene_.lighting.manualLitPixelCount = -1;
      break;
    case ControlCommandType::SetLayout:
      localLight_.layout = spatial_light::normalizeLayout(command.layout);
      bumpLocalLightRevision();
      return true;
    default:
      return false;
  }
  bumpSceneRevision();
  return true;
}

const SpaceSceneSnapshot &SpaceSceneCoordinator::snapshot() const {
  return scene_;
}

const LocalLightConfiguration &
SpaceSceneCoordinator::localLightConfiguration() const {
  return localLight_;
}

PersistedLightingState SpaceSceneCoordinator::lightingState() const {
  return applySceneToLocalState(scene_.lighting, localLight_, audio_);
}

PersistedLightingState SpaceSceneCoordinator::persistedLightingState() const {
  PersistedLightingState persisted = lightingState();
  if (persisted.mode == EffectMode::Off) persisted.mode = modeBeforeOff_;
  return persisted;
}

void SpaceSceneCoordinator::setAudioTuning(const AudioTuning &tuning) {
  audio_ = tuning;
}

void SpaceSceneCoordinator::bumpSceneRevision() {
  ++scene_.revision;
  if (scene_.revision == 0U) ++scene_.revision;
}

void SpaceSceneCoordinator::bumpLocalLightRevision() {
  ++localLight_.revision;
  if (localLight_.revision == 0U) ++localLight_.revision;
}

LightingScene makeLightingScene(const PersistedLightingState &state,
                                const int16_t manualLitPixelCount) {
  LightingScene scene{};
  scene.mode = state.mode;
  scene.brightness = state.brightness;
  scene.primaryColor = state.primaryColor;
  scene.audioColorStyle = state.audioColorStyle;
  scene.cometColorStyle = state.cometColorStyle;
  scene.settings = state.lighting;
  scene.manualLitPixelCount = manualLitPixelCount;
  return scene;
}

PersistedLightingState applySceneToLocalState(
    const LightingScene &scene, const LocalLightConfiguration &configuration,
    const AudioTuning &audio) {
  PersistedLightingState state{};
  state.mode = scene.mode;
  state.brightness = scene.brightness;
  state.primaryColor = scene.primaryColor;
  state.audioColorStyle = scene.audioColorStyle;
  state.cometColorStyle = scene.cometColorStyle;
  state.lighting = scene.settings;
  state.layout = configuration.layout;
  state.startupColor = configuration.startupColor;
  state.startupAnimationSpeed = configuration.startupAnimationSpeed;
  state.audio = audio;
  return state;
}

PersistedLightingState applyLightingSceneToNodeState(
    const PersistedLightingState &localState, const LightingScene &scene) {
  PersistedLightingState state = localState;
  state.mode = scene.mode;
  state.brightness = scene.brightness;
  state.primaryColor = scene.primaryColor;
  state.audioColorStyle = scene.audioColorStyle;
  state.cometColorStyle = scene.cometColorStyle;
  state.lighting = scene.settings;
  return state;
}

}  // namespace sozo
