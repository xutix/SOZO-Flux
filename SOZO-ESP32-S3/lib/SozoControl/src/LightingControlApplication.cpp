#include "LightingControlApplication.h"

namespace sozo {

LightingControlApplication::LightingControlApplication(
    LightingConfigurationRepository &configuration,
    LightingSceneRepository &scenes)
    : configurationRepository_(configuration), sceneRepository_(scenes) {}

void LightingControlApplication::begin(
    const PersistedLightingState &persisted, const bool localLightEnabled) {
  localConfiguration_.layout = spatial_light::normalizeLayout(persisted.layout);
  localConfiguration_.startupColor = persisted.startupColor;
  localConfiguration_.startupAnimationSpeed = persisted.startupAnimationSpeed;
  localConfiguration_.revision = 1U;
  audio_ = persisted.audio;
  modeBeforeOff_ = persisted.mode == EffectMode::Off ? EffectMode::Static
                                                      : persisted.mode;

  if (!sceneRepository_.loadLightingScenes(scenes_)) {
    if (localLightEnabled) {
      NamedLightingScene initial{};
      initial.id = 1U;
      initial.setName("Default space");
      initial.assignments[0] = {kLocalLightingTargetId,
                                makeLightingScene(persisted)};
      initial.assignmentCount = 1U;
      scenes_.saveScene(initial);
      scenes_.activateScene(initial.id);
    }
    sceneRepository_.saveLightingScenes(scenes_);
  } else if (localLightEnabled &&
             scenes_.desiredFor(kLocalLightingTargetId) == nullptr) {
    scenes_.applyDirect(kLocalLightingTargetId, makeLightingScene(persisted));
    sceneRepository_.saveLightingScenes(scenes_);
  }
}

CommandResult LightingControlApplication::dispatch(
    const ControlCommand &command, const uint32_t nowMs) {
  if (!isCommandWellFormed(command)) {
    return {CommandResultCode::InvalidCommand, false};
  }
  if (!isSourceAllowedForCommand(command.source, command.type)) {
    return {CommandResultCode::SourceNotAllowed, false};
  }
  if (!applyLegacyLightingCommand(command, nowMs)) {
    return {CommandResultCode::Unsupported, false};
  }
  return {CommandResultCode::Applied, true};
}

LightingApplicationSnapshot LightingControlApplication::snapshot() const {
  const DesiredLightingState *desired =
      scenes_.desiredFor(kLocalLightingTargetId);
  const LightingScene scene = desired == nullptr ? LightingScene{}
                                                  : desired->scene;
  return {applySceneToLocalState(scene, localConfiguration_, audio_),
          scene.manualLitPixelCount,
          desired == nullptr ? 0U : desired->revision};
}

const LocalLightConfiguration &
LightingControlApplication::localLightConfiguration() const {
  return localConfiguration_;
}

void LightingControlApplication::setAudioTuning(const AudioTuning &tuning,
                                                const uint32_t nowMs) {
  audio_ = tuning;
  markConfigurationDirty(nowMs);
}

bool LightingControlApplication::saveScene(const NamedLightingScene &scene,
                                           const uint32_t nowMs) {
  if (!scenes_.saveScene(scene)) return false;
  markScenesDirty(nowMs);
  return true;
}

bool LightingControlApplication::upsertScene(
    LightingSceneId requestedId, const char *name,
    const LightingTargetId *targets, const size_t targetCount,
    const LightingScene &fallback, const uint32_t nowMs,
    LightingSceneId &savedId) {
  if (name == nullptr || targets == nullptr || targetCount == 0U ||
      targetCount > NamedLightingScene::kMaxAssignments) {
    return false;
  }
  if (requestedId == 0U) {
    for (size_t index = 0U; index < scenes_.sceneCount(); ++index) {
      const NamedLightingScene *stored = scenes_.sceneAt(index);
      if (stored != nullptr && stored->id >= requestedId) {
        requestedId = stored->id + 1U;
      }
    }
    if (requestedId == 0U) requestedId = 1U;
  }

  NamedLightingScene definition{};
  definition.id = requestedId;
  if (!definition.setName(name)) return false;
  const NamedLightingScene *stored = scenes_.sceneById(requestedId);
  for (size_t index = 0U; index < targetCount; ++index) {
    if (targets[index] == 0U) return false;
    LightingScene targetScene = fallback;
    if (stored != nullptr) {
      for (size_t existing = 0U; existing < stored->assignmentCount;
           ++existing) {
        if (stored->assignments[existing].targetId == targets[index]) {
          targetScene = stored->assignments[existing].scene;
          break;
        }
      }
    }
    definition.assignments[definition.assignmentCount++] = {targets[index],
                                                              targetScene};
  }
  if (!saveScene(definition, nowMs)) return false;
  savedId = requestedId;
  return true;
}

bool LightingControlApplication::updateSceneAssignment(
    const LightingSceneId sceneId, const LightingTargetId targetId,
    const LightingScene &scene, const uint32_t nowMs) {
  const NamedLightingScene *stored = scenes_.sceneById(sceneId);
  if (stored == nullptr) return false;
  NamedLightingScene updated = *stored;
  for (size_t index = 0U; index < updated.assignmentCount; ++index) {
    if (updated.assignments[index].targetId != targetId) continue;
    updated.assignments[index].scene = scene;
    return saveScene(updated, nowMs);
  }
  return false;
}

bool LightingControlApplication::eraseScene(const LightingSceneId sceneId,
                                            const uint32_t nowMs) {
  if (!scenes_.eraseScene(sceneId)) return false;
  markScenesDirty(nowMs);
  return true;
}

bool LightingControlApplication::activateScene(
    const LightingSceneId sceneId, const uint32_t nowMs) {
  if (!scenes_.activateScene(sceneId)) return false;
  markScenesDirty(nowMs);
  return true;
}

bool LightingControlApplication::applyDirect(
    const LightingTargetId targetId, const LightingScene &scene,
    const uint32_t nowMs) {
  if (!scenes_.applyDirect(targetId, scene)) return false;
  markScenesDirty(nowMs);
  return true;
}

const NamedLightingScene *LightingControlApplication::sceneById(
    const LightingSceneId sceneId) const {
  return scenes_.sceneById(sceneId);
}

const NamedLightingScene *LightingControlApplication::sceneAt(
    const size_t index) const {
  return scenes_.sceneAt(index);
}

size_t LightingControlApplication::sceneCount() const {
  return scenes_.sceneCount();
}

const DesiredLightingState *LightingControlApplication::desiredFor(
    const LightingTargetId targetId) const {
  return scenes_.desiredFor(targetId);
}

const DesiredLightingState *LightingControlApplication::desiredAt(
    const size_t index) const {
  return scenes_.desiredAt(index);
}

size_t LightingControlApplication::desiredCount() const {
  return scenes_.desiredCount();
}

bool LightingControlApplication::markDelivered(
    const LightingTargetId targetId, const uint32_t revision) {
  return scenes_.markDelivered(targetId, revision);
}

void LightingControlApplication::tick(const uint32_t nowMs) {
  configurationRepository_.persistLightingConfiguration(nowMs,
                                                        snapshot().lighting);
  sceneRepository_.persistLightingScenes(nowMs, scenes_);
}

LightingScene LightingControlApplication::currentLocalScene() const {
  const DesiredLightingState *desired =
      scenes_.desiredFor(kLocalLightingTargetId);
  return desired == nullptr ? LightingScene{} : desired->scene;
}

bool LightingControlApplication::applyLegacyLightingCommand(
    const ControlCommand &command, const uint32_t nowMs) {
  if (command.type == ControlCommandType::SetLayout) {
    localConfiguration_.layout = spatial_light::normalizeLayout(command.layout);
    bumpLocalConfigurationRevision();
    markConfigurationDirty(nowMs);
    return true;
  }

  LightingScene scene = currentLocalScene();
  switch (command.type) {
    case ControlCommandType::SetEffect:
      scene.mode = static_cast<EffectMode>(command.value);
      modeBeforeOff_ = scene.mode;
      scene.manualLitPixelCount = -1;
      break;
    case ControlCommandType::SetParameter:
      if (!applyLegacyParameter(scene, command)) return false;
      if (command.parameter == LightingParameter::StartupColor ||
          command.parameter == LightingParameter::StartupAnimationSpeed) {
        markConfigurationDirty(nowMs);
        return true;
      }
      break;
    case ControlCommandType::TurnOff:
      if (scene.mode != EffectMode::Off) modeBeforeOff_ = scene.mode;
      scene.mode = EffectMode::Off;
      scene.manualLitPixelCount = -1;
      break;
    case ControlCommandType::TurnOn:
      scene.mode = modeBeforeOff_;
      scene.manualLitPixelCount = -1;
      break;
    default:
      return false;
  }
  return applyDirect(kLocalLightingTargetId, scene, nowMs);
}

bool LightingControlApplication::applyLegacyParameter(
    LightingScene &scene, const ControlCommand &command) {
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
      scene.settings.cometBackground = static_cast<uint8_t>(command.value);
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
      scene.settings.breathFloorPercent = static_cast<uint8_t>(command.value);
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
      scene.settings.pulseHeightPercent = static_cast<uint8_t>(command.value);
      break;
    case LightingParameter::AnimationBrightness:
      scene.settings.animationBrightness = static_cast<uint8_t>(command.value);
      break;
    case LightingParameter::AudioColorStyle:
      scene.audioColorStyle = static_cast<uint8_t>(command.value);
      break;
    case LightingParameter::CometColorStyle:
      scene.cometColorStyle = static_cast<uint8_t>(command.value);
      break;
    case LightingParameter::StartupColor:
      localConfiguration_.startupColor = command.color;
      bumpLocalConfigurationRevision();
      return true;
    case LightingParameter::StartupAnimationSpeed:
      localConfiguration_.startupAnimationSpeed = command.value / 100.0F;
      bumpLocalConfigurationRevision();
      return true;
    case LightingParameter::ManualLitPixelCount:
      scene.mode = EffectMode::Static;
      modeBeforeOff_ = EffectMode::Static;
      scene.manualLitPixelCount = static_cast<int16_t>(command.value);
      return true;
    case LightingParameter::None:
    default:
      return false;
  }
  scene.manualLitPixelCount = -1;
  return true;
}

void LightingControlApplication::markConfigurationDirty(const uint32_t nowMs) {
  configurationRepository_.markLightingConfigurationDirty(nowMs);
}

void LightingControlApplication::markScenesDirty(const uint32_t nowMs) {
  sceneRepository_.markLightingScenesDirty(nowMs);
}

void LightingControlApplication::bumpLocalConfigurationRevision() {
  ++localConfiguration_.revision;
  if (localConfiguration_.revision == 0U) ++localConfiguration_.revision;
}

}  // namespace sozo
