#include <NodeSceneRuntime.h>

namespace sozo::c3 {
namespace {

bool isValidEffectMode(const uint8_t value) {
  return value <= static_cast<uint8_t>(EffectMode::Off);
}

float fromFixed100(const uint16_t value) {
  return static_cast<float>(value) / 100.0F;
}

spatial_light::SpatialLayout makeContinuousLayout(const uint16_t ledCount) {
  return {spatial_light::LayoutProfile::Continuous,
          ledCount,
          static_cast<uint16_t>((ledCount - 1U) / 2U),
          0U,
          ledCount,
          0U,
          false};
}

}  // namespace

NodeSceneRuntime::NodeSceneRuntime(NodeLightingSink &lighting)
    : lighting_(lighting) {}

SceneApplyResult NodeSceneRuntime::applyScene(
    const node::SceneSnapshotPayload &scene, const uint32_t sceneRevision,
    const uint32_t coordinatorTimestampMs, const uint32_t localNowMs,
    const SceneTarget target) {
  if (sceneRevision == 0 || !isValidEffectMode(scene.effectMode)) {
    return SceneApplyResult::Invalid;
  }
  SceneSlot &slot = slotFor(target);
  if (target == SceneTarget::FollowMain) {
    if (sceneRevision == slot.revision) return SceneApplyResult::Duplicate;
    if (slot.revision != 0 &&
        static_cast<int32_t>(sceneRevision - slot.revision) < 0) {
      return SceneApplyResult::Stale;
    }
  }

  // Independent scenes are explicit device commands, serialized by the
  // coordinator.  Their revision sequence may restart after the coordinator
  // reboots while this node keeps its local scene in persistent storage.
  // Follow-main scenes remain strictly monotonic above.

  PersistedLightingState next = slot.available ? slot.state : lighting_.state();
  next.mode = static_cast<EffectMode>(scene.effectMode);
  next.brightness = scene.brightness;
  next.primaryColor = {scene.primaryRed, scene.primaryGreen,
                       scene.primaryBlue};
  next.audioColorStyle = scene.audioColorStyle;
  next.cometColorStyle = scene.cometColorStyle;
  next.lighting.rainbowStyle = scene.rainbowStyle;
  next.lighting.flowSpeed = scene.flowSpeed;
  next.lighting.cometTail = scene.cometTail;
  next.lighting.cometSpeed = scene.cometSpeed;
  next.lighting.cometDensity = scene.cometDensity;
  next.lighting.cometBackground = scene.cometBackground;
  next.lighting.cometRandom = scene.cometRandom;
  next.lighting.audioSensitivityX100 = scene.audioSensitivityX100;
  next.lighting.audioColorGainX100 = scene.audioColorGainX100;
  next.lighting.audioHueDrive = scene.audioHueDrive;
  next.lighting.breathFloorPercent = scene.breathFloorPercent;
  next.lighting.secondaryRed = scene.secondaryRed;
  next.lighting.secondaryGreen = scene.secondaryGreen;
  next.lighting.secondaryBlue = scene.secondaryBlue;
  next.lighting.pulseAmplitudePercent = scene.pulseAmplitudePercent;
  next.lighting.pulseHeightPercent = scene.pulseHeightPercent;
  next.lighting.animationBrightness = scene.animationBrightness;

  slot.state = next;
  slot.manualLitPixelCount = scene.manualLitPixelCount;
  slot.revision = sceneRevision;
  slot.available = true;
  clockOffsetMs_ = coordinatorTimestampMs - localNowMs;
  if ((target == SceneTarget::FollowMain &&
       controlMode_ == node::NodeControlMode::FollowMain) ||
      (target == SceneTarget::Independent &&
       controlMode_ == node::NodeControlMode::Independent)) {
    applySlot(slot);
  }
  return SceneApplyResult::Applied;
}

bool NodeSceneRuntime::setLocalLedCount(const uint16_t ledCount) {
  if (ledCount == 0U) return false;
  const spatial_light::SpatialLayout layout = makeContinuousLayout(ledCount);
  PersistedLightingState next = lighting_.state();
  next.layout = layout;
  if (followScene_.available) followScene_.state.layout = layout;
  if (independentScene_.available) independentScene_.state.layout = layout;
  lighting_.applyState(next);
  return true;
}

bool NodeSceneRuntime::setControlMode(const node::NodeControlMode mode) {
  if (mode != node::NodeControlMode::FollowMain &&
      mode != node::NodeControlMode::Independent) {
    return false;
  }
  if (mode == controlMode_) return true;
  if (mode == node::NodeControlMode::Independent &&
      !independentScene_.available) {
    independentScene_ = followScene_.available
                            ? followScene_
                            : SceneSlot{lighting_.state(), -1, 0, true};
  }
  controlMode_ = mode;
  const SceneSlot &active =
      controlMode_ == node::NodeControlMode::Independent ? independentScene_
                                                           : followScene_;
  if (active.available) applySlot(active);
  return true;
}

node::NodeControlMode NodeSceneRuntime::controlMode() const {
  return controlMode_;
}

NodeControlState NodeSceneRuntime::controlState() const {
  NodeControlState state{};
  state.controlMode = controlMode_;
  state.hasIndependentScene = independentScene_.available;
  state.independentState = independentScene_.state;
  state.independentManualLitPixelCount = independentScene_.manualLitPixelCount;
  state.independentRevision = independentScene_.revision;
  return state;
}

bool NodeSceneRuntime::restoreControlState(const NodeControlState &state) {
  if (state.schemaVersion != NodeControlState::kSchemaVersion ||
      (state.controlMode != node::NodeControlMode::FollowMain &&
       state.controlMode != node::NodeControlMode::Independent) ||
      (state.controlMode == node::NodeControlMode::Independent &&
       !state.hasIndependentScene)) {
    return false;
  }

  independentScene_ = {};
  if (state.hasIndependentScene) {
    independentScene_.state = state.independentState;
    const PersistedLightingState &local = lighting_.state();
    independentScene_.state.layout = local.layout;
    independentScene_.state.startupColor = local.startupColor;
    independentScene_.state.startupAnimationSpeed = local.startupAnimationSpeed;
    independentScene_.state.audio = local.audio;
    independentScene_.manualLitPixelCount = state.independentManualLitPixelCount;
    independentScene_.revision = state.independentRevision;
    independentScene_.available = true;
  }
  controlMode_ = state.controlMode;
  if (controlMode_ == node::NodeControlMode::Independent) {
    applySlot(independentScene_);
  }
  return true;
}

bool NodeSceneRuntime::applyAudioFeatures(
    const node::AudioFeaturesPayload &features, const uint32_t sequence) {
  if (sequence == 0 ||
      (hasAudioSequence_ &&
       static_cast<int32_t>(sequence - lastAudioSequence_) <= 0)) {
    return false;
  }
  audio_ = AudioFrame{fromFixed100(features.volumeX100),
                      0.0F,
                      fromFixed100(features.fastEnergyX100),
                      fromFixed100(features.slowEnergyX100),
                      fromFixed100(features.beatPulseX100),
                      features.framesRead,
                      features.available};
  lastAudioSequence_ = sequence;
  hasAudioSequence_ = true;
  return true;
}

void NodeSceneRuntime::tick(const uint32_t localNowMs) {
  lighting_.tick(synchronizedNow(localNowMs), audio_);
}

void NodeSceneRuntime::onDisconnected() {
  // Scene and audio values remain visible during Offline Hold, but their
  // ordering belongs to the coordinator connection that produced them.  A
  // restarted coordinator begins both counters at one again.
  followScene_.revision = 0U;
  lastAudioSequence_ = 0U;
  hasAudioSequence_ = false;
}

uint32_t NodeSceneRuntime::lastAppliedSceneRevision() const {
  return slotFor(controlMode_ == node::NodeControlMode::Independent
                     ? SceneTarget::Independent
                     : SceneTarget::FollowMain)
      .revision;
}

uint32_t NodeSceneRuntime::synchronizedNow(const uint32_t localNowMs) const {
  return localNowMs + clockOffsetMs_;
}

NodeSceneRuntime::SceneSlot &NodeSceneRuntime::slotFor(
    const SceneTarget target) {
  return target == SceneTarget::Independent ? independentScene_ : followScene_;
}

const NodeSceneRuntime::SceneSlot &NodeSceneRuntime::slotFor(
    const SceneTarget target) const {
  return target == SceneTarget::Independent ? independentScene_ : followScene_;
}

void NodeSceneRuntime::applySlot(const SceneSlot &slot) {
  lighting_.applyState(slot.state);
  if (slot.manualLitPixelCount >= 0) {
    lighting_.setLitPixelCount(static_cast<uint16_t>(slot.manualLitPixelCount));
  }
}

}  // namespace sozo::c3
