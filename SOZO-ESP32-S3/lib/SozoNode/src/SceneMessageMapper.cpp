#include <SceneMessageMapper.h>

#include <string.h>

namespace sozo {
namespace {

uint16_t toFixed100(const float value) {
  if (value <= 0.0F) return 0;
  const float scaled = value * 100.0F;
  if (scaled >= 65535.0F) return 65535U;
  return static_cast<uint16_t>(scaled + 0.5F);
}

}  // namespace

node::SceneSnapshotPayload makeSceneSnapshot(
    const PersistedLightingState &state, const LightingSnapshot &runtime) {
  node::SceneSnapshotPayload scene{};
  scene.effectMode = static_cast<uint8_t>(state.mode);
  scene.brightness = state.brightness;
  scene.primaryRed = state.primaryColor.red;
  scene.primaryGreen = state.primaryColor.green;
  scene.primaryBlue = state.primaryColor.blue;
  scene.rainbowStyle = state.lighting.rainbowStyle;
  scene.flowSpeed = state.lighting.flowSpeed;
  scene.cometTail = state.lighting.cometTail;
  scene.cometSpeed = state.lighting.cometSpeed;
  scene.cometDensity = state.lighting.cometDensity;
  scene.cometBackground = state.lighting.cometBackground;
  scene.cometRandom = state.lighting.cometRandom;
  scene.audioSensitivityX100 = state.lighting.audioSensitivityX100;
  scene.audioColorGainX100 = state.lighting.audioColorGainX100;
  scene.audioHueDrive = state.lighting.audioHueDrive;
  scene.breathFloorPercent = state.lighting.breathFloorPercent;
  scene.secondaryRed = state.lighting.secondaryRed;
  scene.secondaryGreen = state.lighting.secondaryGreen;
  scene.secondaryBlue = state.lighting.secondaryBlue;
  scene.pulseAmplitudePercent = state.lighting.pulseAmplitudePercent;
  scene.pulseHeightPercent = state.lighting.pulseHeightPercent;
  scene.animationBrightness = state.lighting.animationBrightness;
  scene.audioColorStyle = state.audioColorStyle;
  scene.cometColorStyle = state.cometColorStyle;
  scene.manualLitPixelCount = runtime.manualLitPixelCount;
  scene.spatialProfile = static_cast<uint8_t>(state.layout.profile);
  scene.spatialFlags =
      state.layout.reversed ? node::kSpatialFlagReversed : 0U;
  return scene;
}

node::AudioFeaturesPayload makeAudioFeatures(const AudioFrame &frame) {
  node::AudioFeaturesPayload features{};
  features.volumeX100 = toFixed100(frame.volume);
  features.fastEnergyX100 = toFixed100(frame.fastEnergy);
  features.slowEnergyX100 = toFixed100(frame.slowEnergy);
  features.beatPulseX100 = toFixed100(frame.beatPulse);
  features.framesRead = frame.framesRead;
  features.available = frame.available;
  return features;
}

bool sameSceneSnapshot(const node::SceneSnapshotPayload &left,
                       const node::SceneSnapshotPayload &right) {
  node::Envelope leftEnvelope{};
  node::Envelope rightEnvelope{};
  if (node::writeSceneSnapshot(leftEnvelope, left) != node::CodecResult::Ok ||
      node::writeSceneSnapshot(rightEnvelope, right) != node::CodecResult::Ok) {
    return false;
  }
  return leftEnvelope.payloadLength == rightEnvelope.payloadLength &&
         memcmp(leftEnvelope.payload, rightEnvelope.payload,
                leftEnvelope.payloadLength) == 0;
}

}  // namespace sozo
