#include <NodeSceneRuntime.h>

namespace sozo::c3 {
namespace {

float fromFixed100(const uint16_t value) {
  return static_cast<float>(value) / 100.0F;
}

LightingScene fromProtocolScene(const node::SceneSnapshotPayload &payload) {
  LightingScene scene{};
  scene.mode = static_cast<EffectMode>(payload.effectMode);
  scene.brightness = payload.brightness;
  scene.primaryColor = {payload.primaryRed, payload.primaryGreen,
                        payload.primaryBlue};
  scene.audioColorStyle = payload.audioColorStyle;
  scene.cometColorStyle = payload.cometColorStyle;
  scene.settings.rainbowStyle = payload.rainbowStyle;
  scene.settings.flowSpeed = payload.flowSpeed;
  scene.settings.cometTail = payload.cometTail;
  scene.settings.cometSpeed = payload.cometSpeed;
  scene.settings.cometDensity = payload.cometDensity;
  scene.settings.cometBackground = payload.cometBackground;
  scene.settings.cometRandom = payload.cometRandom;
  scene.settings.audioSensitivityX100 = payload.audioSensitivityX100;
  scene.settings.audioColorGainX100 = payload.audioColorGainX100;
  scene.settings.audioHueDrive = payload.audioHueDrive;
  scene.settings.breathFloorPercent = payload.breathFloorPercent;
  scene.settings.secondaryRed = payload.secondaryRed;
  scene.settings.secondaryGreen = payload.secondaryGreen;
  scene.settings.secondaryBlue = payload.secondaryBlue;
  scene.settings.pulseAmplitudePercent = payload.pulseAmplitudePercent;
  scene.settings.pulseHeightPercent = payload.pulseHeightPercent;
  scene.settings.animationBrightness = payload.animationBrightness;
  scene.manualLitPixelCount = payload.manualLitPixelCount;
  return scene;
}

AudioFrame fromProtocolAudio(const node::AudioFeaturesPayload &payload) {
  return {fromFixed100(payload.volumeX100),
          0.0F,
          fromFixed100(payload.fastEnergyX100),
          fromFixed100(payload.slowEnergyX100),
          fromFixed100(payload.beatPulseX100),
          payload.framesRead,
          payload.available};
}

}  // namespace

NodeSceneRuntime::NodeSceneRuntime(NodeLightingSink &lighting)
    : runtime_(lighting) {}

SceneApplyResult NodeSceneRuntime::applyScene(
    const node::SceneSnapshotPayload &scene, const uint32_t sceneRevision,
    const uint32_t coordinatorTimestampMs, const uint32_t localNowMs,
    const SceneTarget target) {
  return runtime_.applyScene(fromProtocolScene(scene), sceneRevision,
                             coordinatorTimestampMs, localNowMs, target);
}

bool NodeSceneRuntime::setLocalLedCount(const uint16_t ledCount) {
  return runtime_.setLocalLedCount(ledCount);
}

bool NodeSceneRuntime::setControlMode(const node::NodeControlMode mode) {
  return runtime_.setControlMode(mode);
}

node::NodeControlMode NodeSceneRuntime::controlMode() const {
  return runtime_.controlMode();
}

NodeControlState NodeSceneRuntime::controlState() const {
  return runtime_.controlState();
}

bool NodeSceneRuntime::restoreControlState(const NodeControlState &state) {
  return runtime_.restoreControlState(state);
}

bool NodeSceneRuntime::applyAudioFeatures(
    const node::AudioFeaturesPayload &features, const uint32_t sequence) {
  return runtime_.applyAudioFrame(fromProtocolAudio(features), sequence);
}

void NodeSceneRuntime::tick(const uint32_t localNowMs) {
  runtime_.tick(localNowMs);
}

void NodeSceneRuntime::onDisconnected() { runtime_.onDisconnected(); }

uint32_t NodeSceneRuntime::lastAppliedSceneRevision() const {
  return runtime_.lastAppliedSceneRevision();
}

uint32_t NodeSceneRuntime::synchronizedNow(const uint32_t localNowMs) const {
  return runtime_.synchronizedNow(localNowMs);
}

}  // namespace sozo::c3
