#include "LightNodeRuntime.h"

namespace sozo {

LightNodeRuntime::LightNodeRuntime(LightNodeSink &lighting)
    : lighting_(lighting) {}

void LightNodeRuntime::begin(const PersistedLightingState &localState) {
  lighting_.begin(localState);
  audio_ = {};
  followScene_ = {};
  independentScene_ = {};
  controlMode_ = node::NodeControlMode::FollowMain;
  lastAudioSequence_ = 0U;
  clockOffsetMs_ = 0U;
  hasAudioSequence_ = false;
}

LightSceneApplyResult LightNodeRuntime::applyScene(
    const LightingScene &scene, const uint32_t sceneRevision,
    const uint32_t coordinatorTimestampMs, const uint32_t localNowMs,
    const LightSceneTarget target) {
  if (sceneRevision == 0U ||
      static_cast<uint8_t>(scene.mode) >
          static_cast<uint8_t>(EffectMode::Off)) {
    return LightSceneApplyResult::Invalid;
  }
  SceneSlot &slot = slotFor(target);
  if (target == LightSceneTarget::FollowSpace) {
    if (sceneRevision == slot.revision) {
      return LightSceneApplyResult::Duplicate;
    }
    if (slot.revision != 0U &&
        static_cast<int32_t>(sceneRevision - slot.revision) < 0) {
      return LightSceneApplyResult::Stale;
    }
  }

  slot.state = applyLightingSceneToNodeState(
      slot.available ? slot.state : lighting_.state(), scene);
  slot.manualLitPixelCount = scene.manualLitPixelCount;
  slot.revision = sceneRevision;
  slot.available = true;
  clockOffsetMs_ = coordinatorTimestampMs - localNowMs;
  if ((target == LightSceneTarget::FollowSpace &&
       controlMode_ == node::NodeControlMode::FollowMain) ||
      (target == LightSceneTarget::Independent &&
       controlMode_ == node::NodeControlMode::Independent)) {
    applySlot(slot);
  }
  return LightSceneApplyResult::Applied;
}

bool LightNodeRuntime::setLocalLedCount(const uint16_t ledCount) {
  if (ledCount == 0U) return false;
  const spatial_light::SpatialLayout layout{
      spatial_light::LayoutProfile::Continuous,
      ledCount,
      static_cast<uint16_t>((ledCount - 1U) / 2U),
      0U,
      ledCount,
      0U,
      false};
  PersistedLightingState next = lighting_.state();
  next.layout = layout;
  if (followScene_.available) followScene_.state.layout = layout;
  if (independentScene_.available) independentScene_.state.layout = layout;
  lighting_.applyState(next);
  return true;
}

void LightNodeRuntime::updateLocalConfiguration(
    const LocalLightConfiguration &configuration, const AudioTuning &audio) {
  PersistedLightingState next = lighting_.state();
  applyLocalConfigurationToState(next, configuration, audio);
  if (followScene_.available) {
    applyLocalConfigurationToState(followScene_.state, configuration, audio);
  }
  if (independentScene_.available) {
    applyLocalConfigurationToState(independentScene_.state, configuration,
                                   audio);
  }
  lighting_.applyState(next);
  const SceneSlot &active =
      controlMode_ == node::NodeControlMode::Independent ? independentScene_
                                                          : followScene_;
  if (active.available && active.manualLitPixelCount >= 0) {
    applyManualPixelCount(active.manualLitPixelCount);
  }
}

bool LightNodeRuntime::setControlMode(const node::NodeControlMode mode) {
  if (mode != node::NodeControlMode::FollowMain &&
      mode != node::NodeControlMode::Independent) {
    return false;
  }
  if (mode == controlMode_) return true;
  if (controlMode_ == node::NodeControlMode::FollowMain &&
      mode == node::NodeControlMode::Independent &&
      !followScene_.available) {
    followScene_ = SceneSlot{lighting_.state(), -1, 0U, true};
  }
  if (mode == node::NodeControlMode::Independent &&
      !independentScene_.available) {
    independentScene_ = followScene_.available
                            ? followScene_
                            : SceneSlot{lighting_.state(), -1, 0U, true};
  }
  controlMode_ = mode;
  const SceneSlot &active =
      controlMode_ == node::NodeControlMode::Independent ? independentScene_
                                                          : followScene_;
  if (active.available) applySlot(active);
  return true;
}

node::NodeControlMode LightNodeRuntime::controlMode() const {
  return controlMode_;
}

LightNodeControlState LightNodeRuntime::controlState() const {
  LightNodeControlState state{};
  state.controlMode = controlMode_;
  state.hasIndependentScene = independentScene_.available;
  state.independentState = independentScene_.state;
  state.independentManualLitPixelCount =
      independentScene_.manualLitPixelCount;
  state.independentRevision = independentScene_.revision;
  return state;
}

bool LightNodeRuntime::restoreControlState(
    const LightNodeControlState &state) {
  if (state.schemaVersion != LightNodeControlState::kSchemaVersion ||
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
    independentScene_.state.startupAnimationSpeed =
        local.startupAnimationSpeed;
    independentScene_.state.audio = local.audio;
    independentScene_.manualLitPixelCount =
        state.independentManualLitPixelCount;
    independentScene_.revision = state.independentRevision;
    independentScene_.available = true;
  }
  controlMode_ = state.controlMode;
  if (controlMode_ == node::NodeControlMode::Independent) {
    applySlot(independentScene_);
  } else if (followScene_.available) {
    applySlot(followScene_);
  }
  return true;
}

bool LightNodeRuntime::applyAudioFrame(const AudioFrame &audio,
                                       const uint32_t sequence) {
  if (sequence == 0U ||
      (hasAudioSequence_ &&
       static_cast<int32_t>(sequence - lastAudioSequence_) <= 0)) {
    return false;
  }
  audio_ = audio;
  lastAudioSequence_ = sequence;
  hasAudioSequence_ = true;
  return true;
}

void LightNodeRuntime::tick(const uint32_t localNowMs) {
  lighting_.tick(synchronizedNow(localNowMs), audio_);
}

void LightNodeRuntime::onDisconnected() {
  followScene_.revision = 0U;
  lastAudioSequence_ = 0U;
  hasAudioSequence_ = false;
}

uint32_t LightNodeRuntime::lastAppliedSceneRevision() const {
  const LightSceneTarget target =
      controlMode_ == node::NodeControlMode::Independent
          ? LightSceneTarget::Independent
          : LightSceneTarget::FollowSpace;
  return slotFor(target).revision;
}

uint32_t LightNodeRuntime::synchronizedNow(const uint32_t localNowMs) const {
  return localNowMs + clockOffsetMs_;
}

LightNodeRuntime::SceneSlot &LightNodeRuntime::slotFor(
    const LightSceneTarget target) {
  return target == LightSceneTarget::Independent ? independentScene_
                                                  : followScene_;
}

const LightNodeRuntime::SceneSlot &LightNodeRuntime::slotFor(
    const LightSceneTarget target) const {
  return target == LightSceneTarget::Independent ? independentScene_
                                                  : followScene_;
}

void LightNodeRuntime::applySlot(const SceneSlot &slot) {
  lighting_.applyState(slot.state);
  applyManualPixelCount(slot.manualLitPixelCount);
}

void LightNodeRuntime::applyManualPixelCount(const int16_t count) {
  if (count < 0) return;
  const uint16_t requested = static_cast<uint16_t>(count);
  const uint16_t available = lighting_.state().layout.activeCount;
  lighting_.setLitPixelCount(requested > available ? available : requested);
}

void LightNodeRuntime::applyLocalConfigurationToState(
    PersistedLightingState &state,
    const LocalLightConfiguration &configuration, const AudioTuning &audio) {
  state.layout = configuration.layout;
  state.startupColor = configuration.startupColor;
  state.startupAnimationSpeed = configuration.startupAnimationSpeed;
  state.audio = audio;
}

}  // namespace sozo
