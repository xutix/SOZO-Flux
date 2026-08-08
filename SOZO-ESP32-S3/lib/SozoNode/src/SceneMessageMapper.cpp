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

node::SceneSnapshotPayload makeSceneSnapshot(const LightingScene &lighting) {
  node::SceneSnapshotPayload payload{};
  payload.effectMode = static_cast<uint8_t>(lighting.mode);
  payload.brightness = lighting.brightness;
  payload.primaryRed = lighting.primaryColor.red;
  payload.primaryGreen = lighting.primaryColor.green;
  payload.primaryBlue = lighting.primaryColor.blue;
  payload.rainbowStyle = lighting.settings.rainbowStyle;
  payload.flowSpeed = lighting.settings.flowSpeed;
  payload.cometTail = lighting.settings.cometTail;
  payload.cometSpeed = lighting.settings.cometSpeed;
  payload.cometDensity = lighting.settings.cometDensity;
  payload.cometBackground = lighting.settings.cometBackground;
  payload.cometRandom = lighting.settings.cometRandom;
  payload.audioSensitivityX100 = lighting.settings.audioSensitivityX100;
  payload.audioColorGainX100 = lighting.settings.audioColorGainX100;
  payload.audioHueDrive = lighting.settings.audioHueDrive;
  payload.breathFloorPercent = lighting.settings.breathFloorPercent;
  payload.secondaryRed = lighting.settings.secondaryRed;
  payload.secondaryGreen = lighting.settings.secondaryGreen;
  payload.secondaryBlue = lighting.settings.secondaryBlue;
  payload.pulseAmplitudePercent = lighting.settings.pulseAmplitudePercent;
  payload.pulseHeightPercent = lighting.settings.pulseHeightPercent;
  payload.animationBrightness = lighting.settings.animationBrightness;
  payload.audioColorStyle = lighting.audioColorStyle;
  payload.cometColorStyle = lighting.cometColorStyle;
  payload.manualLitPixelCount = lighting.manualLitPixelCount;
  payload.spatialProfile =
      static_cast<uint8_t>(spatial_light::LayoutProfile::Continuous);
  payload.spatialFlags = 0U;
  return payload;
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
