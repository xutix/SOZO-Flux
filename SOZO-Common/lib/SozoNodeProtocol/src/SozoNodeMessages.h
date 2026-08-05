#pragma once

#include <SozoNodeProtocol.h>

namespace sozo::node {

constexpr uint8_t kSpatialFlagReversed = 1U << 0U;

struct CapabilitiesPayload {
  uint32_t capabilityBits{0};
  uint16_t maxLedCount{0};
  uint16_t maxPacketBytes{0};
  uint8_t protocolMin{0};
  uint8_t protocolMax{0};
  uint8_t firmwareMajor{0};
  uint8_t firmwareMinor{0};
  uint8_t firmwarePatch{0};
  NodeControlMode controlMode{NodeControlMode::FollowMain};
  uint16_t hardwareProfile{0};
  bool bound{false};
};

struct HeartbeatPayload {
  uint32_t uptimeMs{0};
  uint32_t lastAppliedSceneRevision{0};
  uint32_t freeHeapBytes{0};
  uint16_t errorFlags{0};
  NodeControlMode controlMode{NodeControlMode::FollowMain};
  bool pairingWindowOpen{false};
};

struct SceneSnapshotPayload {
  uint8_t effectMode{0};
  uint8_t brightness{0};
  uint8_t primaryRed{0};
  uint8_t primaryGreen{0};
  uint8_t primaryBlue{0};
  uint8_t rainbowStyle{0};
  uint8_t flowSpeed{0};
  uint8_t cometTail{0};
  uint8_t cometSpeed{0};
  uint8_t cometDensity{0};
  uint8_t cometBackground{0};
  bool cometRandom{false};
  uint16_t audioSensitivityX100{0};
  uint16_t audioColorGainX100{0};
  uint8_t audioHueDrive{0};
  uint8_t breathFloorPercent{0};
  uint8_t secondaryRed{0};
  uint8_t secondaryGreen{0};
  uint8_t secondaryBlue{0};
  uint8_t pulseAmplitudePercent{0};
  uint8_t pulseHeightPercent{0};
  uint8_t animationBrightness{0};
  uint8_t audioColorStyle{0};
  uint8_t cometColorStyle{0};
  int16_t manualLitPixelCount{-1};
  uint8_t spatialProfile{0};
  uint8_t spatialFlags{0};
};

struct AudioFeaturesPayload {
  uint16_t volumeX100{0};
  uint16_t fastEnergyX100{0};
  uint16_t slowEnergyX100{0};
  uint16_t beatPulseX100{0};
  uint32_t framesRead{0};
  bool available{false};
};

struct BindRequestPayload {
  NodeId coordinatorNodeId{0};
  uint32_t bindingNonce{0};
};

struct BindResultPayload {
  bool accepted{false};
  NodeId nodeId{0};
  uint32_t bindingNonce{0};
  uint16_t errorCode{0};
};

struct CommandReceiptPayload {
  bool accepted{false};
  uint32_t lastAppliedSceneRevision{0};
  uint16_t errorCode{0};
};

struct StatusSnapshotPayload {
  uint32_t lastAppliedSceneRevision{0};
  uint32_t freeHeapBytes{0};
  uint16_t errorFlags{0};
  NodeControlMode controlMode{NodeControlMode::FollowMain};
  bool pairingWindowOpen{false};
  uint16_t ledCount{0};
};

struct ControlModePayload {
  NodeControlMode controlMode{NodeControlMode::FollowMain};
};

struct LedCountPayload {
  uint16_t ledCount{0};
};

constexpr uint16_t kCapabilitiesWireBytes = 17;
constexpr uint16_t kHeartbeatWireBytes = 16;
constexpr uint16_t kSceneSnapshotWireBytes = 30;
constexpr uint16_t kAudioFeaturesWireBytes = 13;
constexpr uint16_t kBindRequestWireBytes = 8;
constexpr uint16_t kBindResultWireBytes = 11;
constexpr uint16_t kCommandReceiptWireBytes = 7;
constexpr uint16_t kStatusSnapshotWireBytes = 14;
constexpr uint16_t kStatusRequestWireBytes = 0;
constexpr uint16_t kControlModeWireBytes = 1;
constexpr uint16_t kLedCountWireBytes = 2;

CodecResult writeCapabilities(Envelope &envelope,
                              const CapabilitiesPayload &payload);
CodecResult readCapabilities(const Envelope &envelope,
                             CapabilitiesPayload &payload);
CodecResult writeHeartbeat(Envelope &envelope,
                           const HeartbeatPayload &payload);
CodecResult readHeartbeat(const Envelope &envelope, HeartbeatPayload &payload);
CodecResult writeSceneSnapshot(Envelope &envelope,
                               const SceneSnapshotPayload &payload);
CodecResult readSceneSnapshot(const Envelope &envelope,
                              SceneSnapshotPayload &payload);
CodecResult writeAudioFeatures(Envelope &envelope,
                               const AudioFeaturesPayload &payload);
CodecResult readAudioFeatures(const Envelope &envelope,
                              AudioFeaturesPayload &payload);
CodecResult writeBindRequest(Envelope &envelope,
                             const BindRequestPayload &payload);
CodecResult readBindRequest(const Envelope &envelope,
                            BindRequestPayload &payload);
CodecResult writeBindResult(Envelope &envelope,
                            const BindResultPayload &payload);
CodecResult readBindResult(const Envelope &envelope,
                           BindResultPayload &payload);
CodecResult writeCommandReceipt(Envelope &envelope,
                                const CommandReceiptPayload &payload);
CodecResult readCommandReceipt(const Envelope &envelope,
                               CommandReceiptPayload &payload);
CodecResult writeStatusSnapshot(Envelope &envelope,
                                const StatusSnapshotPayload &payload);
CodecResult readStatusSnapshot(const Envelope &envelope,
                               StatusSnapshotPayload &payload);
CodecResult writeStatusRequest(Envelope &envelope);
CodecResult readStatusRequest(const Envelope &envelope);
CodecResult writeControlModeRequest(Envelope &envelope,
                                    const ControlModePayload &payload);
CodecResult readControlModeRequest(const Envelope &envelope,
                                   ControlModePayload &payload);
CodecResult writeLedCountRequest(Envelope &envelope,
                                 const LedCountPayload &payload);
CodecResult readLedCountRequest(const Envelope &envelope,
                                LedCountPayload &payload);

}  // namespace sozo::node
