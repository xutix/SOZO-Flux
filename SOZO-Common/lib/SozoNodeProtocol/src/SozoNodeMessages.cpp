#include <SozoNodeMessages.h>

namespace sozo::node {
namespace {

class PayloadWriter {
 public:
  explicit PayloadWriter(Envelope &envelope) : envelope_(envelope) {
    envelope_.payloadLength = 0;
  }

  void u8(const uint8_t value) {
    envelope_.payload[envelope_.payloadLength++] = value;
  }

  void boolean(const bool value) { u8(value ? 1U : 0U); }

  void u16(const uint16_t value) {
    u8(static_cast<uint8_t>(value & 0xFFU));
    u8(static_cast<uint8_t>((value >> 8U) & 0xFFU));
  }

  void i16(const int16_t value) { u16(static_cast<uint16_t>(value)); }

  void u32(const uint32_t value) {
    u16(static_cast<uint16_t>(value & 0xFFFFU));
    u16(static_cast<uint16_t>((value >> 16U) & 0xFFFFU));
  }

 private:
  Envelope &envelope_;
};

class PayloadReader {
 public:
  explicit PayloadReader(const Envelope &envelope)
      : data_(envelope.payload), position_(0) {}

  uint8_t u8() { return data_[position_++]; }
  bool boolean() { return u8() != 0; }

  uint16_t u16() {
    const uint16_t low = u8();
    return static_cast<uint16_t>(low | (static_cast<uint16_t>(u8()) << 8U));
  }

  int16_t i16() { return static_cast<int16_t>(u16()); }

  uint32_t u32() {
    const uint32_t low = u16();
    return low | (static_cast<uint32_t>(u16()) << 16U);
  }

 private:
  const uint8_t *data_;
  uint16_t position_;
};

CodecResult validate(const Envelope &envelope, const MessageType expectedType,
                     const uint16_t expectedLength) {
  return envelope.messageType == expectedType &&
                 envelope.payloadLength == expectedLength
             ? CodecResult::Ok
             : CodecResult::InvalidPayload;
}

}  // namespace

CodecResult writeCapabilities(Envelope &envelope,
                              const CapabilitiesPayload &payload) {
  envelope.messageType = MessageType::Capabilities;
  PayloadWriter writer(envelope);
  writer.u32(payload.capabilityBits);
  writer.u16(payload.maxLedCount);
  writer.u16(payload.maxPacketBytes);
  writer.u8(payload.protocolMin);
  writer.u8(payload.protocolMax);
  writer.u8(payload.firmwareMajor);
  writer.u8(payload.firmwareMinor);
  writer.u8(payload.firmwarePatch);
  writer.u8(static_cast<uint8_t>(payload.controlMode));
  writer.u16(payload.hardwareProfile);
  writer.boolean(payload.bound);
  return CodecResult::Ok;
}

CodecResult readCapabilities(const Envelope &envelope,
                             CapabilitiesPayload &payload) {
  const CodecResult result =
      validate(envelope, MessageType::Capabilities, kCapabilitiesWireBytes);
  if (result != CodecResult::Ok) return result;
  payload = CapabilitiesPayload{};
  PayloadReader reader(envelope);
  payload.capabilityBits = reader.u32();
  payload.maxLedCount = reader.u16();
  payload.maxPacketBytes = reader.u16();
  payload.protocolMin = reader.u8();
  payload.protocolMax = reader.u8();
  payload.firmwareMajor = reader.u8();
  payload.firmwareMinor = reader.u8();
  payload.firmwarePatch = reader.u8();
  payload.controlMode = static_cast<NodeControlMode>(reader.u8());
  payload.hardwareProfile = reader.u16();
  payload.bound = reader.boolean();
  return CodecResult::Ok;
}

CodecResult writeHeartbeat(Envelope &envelope,
                           const HeartbeatPayload &payload) {
  envelope.messageType = MessageType::Heartbeat;
  PayloadWriter writer(envelope);
  writer.u32(payload.uptimeMs);
  writer.u32(payload.lastAppliedSceneRevision);
  writer.u32(payload.freeHeapBytes);
  writer.u16(payload.errorFlags);
  writer.u8(static_cast<uint8_t>(payload.controlMode));
  writer.boolean(payload.pairingWindowOpen);
  return CodecResult::Ok;
}

CodecResult readHeartbeat(const Envelope &envelope, HeartbeatPayload &payload) {
  const CodecResult result =
      validate(envelope, MessageType::Heartbeat, kHeartbeatWireBytes);
  if (result != CodecResult::Ok) return result;
  payload = HeartbeatPayload{};
  PayloadReader reader(envelope);
  payload.uptimeMs = reader.u32();
  payload.lastAppliedSceneRevision = reader.u32();
  payload.freeHeapBytes = reader.u32();
  payload.errorFlags = reader.u16();
  payload.controlMode = static_cast<NodeControlMode>(reader.u8());
  payload.pairingWindowOpen = reader.boolean();
  return CodecResult::Ok;
}

CodecResult writeSceneSnapshot(Envelope &envelope,
                               const SceneSnapshotPayload &payload) {
  envelope.messageType = MessageType::SceneSnapshot;
  PayloadWriter writer(envelope);
  writer.u8(payload.effectMode);
  writer.u8(payload.brightness);
  writer.u8(payload.primaryRed);
  writer.u8(payload.primaryGreen);
  writer.u8(payload.primaryBlue);
  writer.u8(payload.rainbowStyle);
  writer.u8(payload.flowSpeed);
  writer.u8(payload.cometTail);
  writer.u8(payload.cometSpeed);
  writer.u8(payload.cometDensity);
  writer.u8(payload.cometBackground);
  writer.boolean(payload.cometRandom);
  writer.u16(payload.audioSensitivityX100);
  writer.u16(payload.audioColorGainX100);
  writer.u8(payload.audioHueDrive);
  writer.u8(payload.breathFloorPercent);
  writer.u8(payload.secondaryRed);
  writer.u8(payload.secondaryGreen);
  writer.u8(payload.secondaryBlue);
  writer.u8(payload.pulseAmplitudePercent);
  writer.u8(payload.pulseHeightPercent);
  writer.u8(payload.animationBrightness);
  writer.u8(payload.audioColorStyle);
  writer.u8(payload.cometColorStyle);
  writer.i16(payload.manualLitPixelCount);
  writer.u8(payload.spatialProfile);
  writer.u8(payload.spatialFlags);
  return CodecResult::Ok;
}

CodecResult readSceneSnapshot(const Envelope &envelope,
                              SceneSnapshotPayload &payload) {
  const CodecResult result =
      validate(envelope, MessageType::SceneSnapshot, kSceneSnapshotWireBytes);
  if (result != CodecResult::Ok) return result;
  payload = SceneSnapshotPayload{};
  PayloadReader reader(envelope);
  payload.effectMode = reader.u8();
  payload.brightness = reader.u8();
  payload.primaryRed = reader.u8();
  payload.primaryGreen = reader.u8();
  payload.primaryBlue = reader.u8();
  payload.rainbowStyle = reader.u8();
  payload.flowSpeed = reader.u8();
  payload.cometTail = reader.u8();
  payload.cometSpeed = reader.u8();
  payload.cometDensity = reader.u8();
  payload.cometBackground = reader.u8();
  payload.cometRandom = reader.boolean();
  payload.audioSensitivityX100 = reader.u16();
  payload.audioColorGainX100 = reader.u16();
  payload.audioHueDrive = reader.u8();
  payload.breathFloorPercent = reader.u8();
  payload.secondaryRed = reader.u8();
  payload.secondaryGreen = reader.u8();
  payload.secondaryBlue = reader.u8();
  payload.pulseAmplitudePercent = reader.u8();
  payload.pulseHeightPercent = reader.u8();
  payload.animationBrightness = reader.u8();
  payload.audioColorStyle = reader.u8();
  payload.cometColorStyle = reader.u8();
  payload.manualLitPixelCount = reader.i16();
  payload.spatialProfile = reader.u8();
  payload.spatialFlags = reader.u8();
  return CodecResult::Ok;
}

CodecResult writeAudioFeatures(Envelope &envelope,
                               const AudioFeaturesPayload &payload) {
  envelope.messageType = MessageType::AudioFeatures;
  PayloadWriter writer(envelope);
  writer.u16(payload.volumeX100);
  writer.u16(payload.fastEnergyX100);
  writer.u16(payload.slowEnergyX100);
  writer.u16(payload.beatPulseX100);
  writer.u32(payload.framesRead);
  writer.boolean(payload.available);
  return CodecResult::Ok;
}

CodecResult readAudioFeatures(const Envelope &envelope,
                              AudioFeaturesPayload &payload) {
  const CodecResult result = validate(envelope, MessageType::AudioFeatures,
                                      kAudioFeaturesWireBytes);
  if (result != CodecResult::Ok) return result;
  payload = AudioFeaturesPayload{};
  PayloadReader reader(envelope);
  payload.volumeX100 = reader.u16();
  payload.fastEnergyX100 = reader.u16();
  payload.slowEnergyX100 = reader.u16();
  payload.beatPulseX100 = reader.u16();
  payload.framesRead = reader.u32();
  payload.available = reader.boolean();
  return CodecResult::Ok;
}

CodecResult writeBindRequest(Envelope &envelope,
                             const BindRequestPayload &payload) {
  envelope.messageType = MessageType::BindRequest;
  PayloadWriter writer(envelope);
  writer.u32(payload.coordinatorNodeId);
  writer.u32(payload.bindingNonce);
  return CodecResult::Ok;
}

CodecResult readBindRequest(const Envelope &envelope,
                            BindRequestPayload &payload) {
  const CodecResult result =
      validate(envelope, MessageType::BindRequest, kBindRequestWireBytes);
  if (result != CodecResult::Ok) return result;
  payload = BindRequestPayload{};
  PayloadReader reader(envelope);
  payload.coordinatorNodeId = reader.u32();
  payload.bindingNonce = reader.u32();
  return CodecResult::Ok;
}

CodecResult writeBindResult(Envelope &envelope,
                            const BindResultPayload &payload) {
  envelope.messageType = MessageType::BindResult;
  PayloadWriter writer(envelope);
  writer.boolean(payload.accepted);
  writer.u32(payload.nodeId);
  writer.u32(payload.bindingNonce);
  writer.u16(payload.errorCode);
  return CodecResult::Ok;
}

CodecResult readBindResult(const Envelope &envelope,
                           BindResultPayload &payload) {
  const CodecResult result =
      validate(envelope, MessageType::BindResult, kBindResultWireBytes);
  if (result != CodecResult::Ok) return result;
  payload = BindResultPayload{};
  PayloadReader reader(envelope);
  payload.accepted = reader.boolean();
  payload.nodeId = reader.u32();
  payload.bindingNonce = reader.u32();
  payload.errorCode = reader.u16();
  return CodecResult::Ok;
}

CodecResult writeCommandReceipt(Envelope &envelope,
                                const CommandReceiptPayload &payload) {
  envelope.messageType = MessageType::Ack;
  envelope.flags = kFlagIsResponse |
                   (payload.accepted ? 0U : kFlagIsError);
  PayloadWriter writer(envelope);
  writer.boolean(payload.accepted);
  writer.u32(payload.lastAppliedSceneRevision);
  writer.u16(payload.errorCode);
  return CodecResult::Ok;
}

CodecResult readCommandReceipt(const Envelope &envelope,
                               CommandReceiptPayload &payload) {
  const CodecResult result =
      validate(envelope, MessageType::Ack, kCommandReceiptWireBytes);
  if (result != CodecResult::Ok) return result;
  payload = CommandReceiptPayload{};
  PayloadReader reader(envelope);
  payload.accepted = reader.boolean();
  payload.lastAppliedSceneRevision = reader.u32();
  payload.errorCode = reader.u16();
  return CodecResult::Ok;
}

CodecResult writeStatusSnapshot(Envelope &envelope,
                                const StatusSnapshotPayload &payload) {
  envelope.messageType = MessageType::StatusSnapshot;
  envelope.flags = kFlagIsResponse;
  PayloadWriter writer(envelope);
  writer.u32(payload.lastAppliedSceneRevision);
  writer.u32(payload.freeHeapBytes);
  writer.u16(payload.errorFlags);
  writer.u8(static_cast<uint8_t>(payload.controlMode));
  writer.boolean(payload.pairingWindowOpen);
  writer.u16(payload.ledCount);
  writer.u8(payload.layoutProfile);
  writer.u16(payload.centerIndex);
  writer.u16(payload.leftCount);
  writer.u16(payload.centerCount);
  writer.u16(payload.rightCount);
  writer.u8(payload.spatialFlags);
  return CodecResult::Ok;
}

CodecResult readStatusSnapshot(const Envelope &envelope,
                               StatusSnapshotPayload &payload) {
  const CodecResult result = validate(envelope, MessageType::StatusSnapshot,
                                      kStatusSnapshotWireBytes);
  if (result != CodecResult::Ok) return result;
  payload = StatusSnapshotPayload{};
  PayloadReader reader(envelope);
  payload.lastAppliedSceneRevision = reader.u32();
  payload.freeHeapBytes = reader.u32();
  payload.errorFlags = reader.u16();
  payload.controlMode = static_cast<NodeControlMode>(reader.u8());
  payload.pairingWindowOpen = reader.boolean();
  payload.ledCount = reader.u16();
  payload.layoutProfile = reader.u8();
  payload.centerIndex = reader.u16();
  payload.leftCount = reader.u16();
  payload.centerCount = reader.u16();
  payload.rightCount = reader.u16();
  payload.spatialFlags = reader.u8();
  return CodecResult::Ok;
}

CodecResult writeStatusRequest(Envelope &envelope) {
  envelope.messageType = MessageType::StatusRequest;
  envelope.payloadLength = kStatusRequestWireBytes;
  return CodecResult::Ok;
}

CodecResult readStatusRequest(const Envelope &envelope) {
  return validate(envelope, MessageType::StatusRequest,
                  kStatusRequestWireBytes);
}

CodecResult writeControlModeRequest(Envelope &envelope,
                                    const ControlModePayload &payload) {
  if (payload.controlMode != NodeControlMode::FollowMain &&
      payload.controlMode != NodeControlMode::Independent) {
    return CodecResult::InvalidPayload;
  }
  envelope.messageType = MessageType::ControlModeRequest;
  PayloadWriter writer(envelope);
  writer.u8(static_cast<uint8_t>(payload.controlMode));
  return CodecResult::Ok;
}

CodecResult readControlModeRequest(const Envelope &envelope,
                                   ControlModePayload &payload) {
  const CodecResult result = validate(envelope, MessageType::ControlModeRequest,
                                      kControlModeWireBytes);
  if (result != CodecResult::Ok) return result;
  PayloadReader reader(envelope);
  const auto mode = static_cast<NodeControlMode>(reader.u8());
  if (mode != NodeControlMode::FollowMain &&
      mode != NodeControlMode::Independent) {
    return CodecResult::InvalidPayload;
  }
  payload = ControlModePayload{};
  payload.controlMode = mode;
  return CodecResult::Ok;
}

CodecResult writeLedCountRequest(Envelope &envelope,
                                 const LedCountPayload &payload) {
  if (payload.ledCount == 0U) return CodecResult::InvalidPayload;
  envelope.messageType = MessageType::LedCountRequest;
  PayloadWriter writer(envelope);
  writer.u16(payload.ledCount);
  return CodecResult::Ok;
}

CodecResult readLedCountRequest(const Envelope &envelope,
                                LedCountPayload &payload) {
  const CodecResult result =
      validate(envelope, MessageType::LedCountRequest, kLedCountWireBytes);
  if (result != CodecResult::Ok) return result;
  PayloadReader reader(envelope);
  payload = LedCountPayload{};
  payload.ledCount = reader.u16();
  return payload.ledCount == 0U ? CodecResult::InvalidPayload
                                : CodecResult::Ok;
}

CodecResult writeLedGeometryRequest(Envelope &envelope,
                                    const LedGeometryPayload &payload) {
  const uint32_t segmentedTotal = static_cast<uint32_t>(payload.leftCount) +
                                  payload.centerCount + payload.rightCount;
  if (payload.activeCount == 0U || payload.layoutProfile > 1U ||
      (payload.layoutProfile == 0U &&
       payload.centerIndex >= payload.activeCount) ||
      (payload.layoutProfile == 1U &&
       (payload.centerCount == 0U || segmentedTotal != payload.activeCount))) {
    return CodecResult::InvalidPayload;
  }
  envelope.messageType = MessageType::LedGeometryRequest;
  PayloadWriter writer(envelope);
  writer.u8(payload.layoutProfile);
  writer.u16(payload.activeCount);
  writer.u16(payload.centerIndex);
  writer.u16(payload.leftCount);
  writer.u16(payload.centerCount);
  writer.u16(payload.rightCount);
  writer.u8(payload.spatialFlags);
  return CodecResult::Ok;
}

CodecResult readLedGeometryRequest(const Envelope &envelope,
                                   LedGeometryPayload &payload) {
  const CodecResult result = validate(envelope, MessageType::LedGeometryRequest,
                                      kLedGeometryWireBytes);
  if (result != CodecResult::Ok) return result;
  PayloadReader reader(envelope);
  payload = LedGeometryPayload{};
  payload.layoutProfile = reader.u8();
  payload.activeCount = reader.u16();
  payload.centerIndex = reader.u16();
  payload.leftCount = reader.u16();
  payload.centerCount = reader.u16();
  payload.rightCount = reader.u16();
  payload.spatialFlags = reader.u8();
  Envelope validationEnvelope{};
  return writeLedGeometryRequest(validationEnvelope, payload);
}

CodecResult writeFirmwareBegin(Envelope &envelope,
                               const FirmwareBeginPayload &payload) {
  if (payload.imageSize == 0U) return CodecResult::InvalidPayload;
  envelope.messageType = MessageType::FirmwareBegin;
  PayloadWriter writer(envelope);
  writer.u32(payload.imageSize);
  for (const uint8_t value : payload.sha256) writer.u8(value);
  return CodecResult::Ok;
}

CodecResult readFirmwareBegin(const Envelope &envelope,
                              FirmwareBeginPayload &payload) {
  const CodecResult result = validate(envelope, MessageType::FirmwareBegin,
                                      kFirmwareBeginWireBytes);
  if (result != CodecResult::Ok) return result;
  payload = FirmwareBeginPayload{};
  PayloadReader reader(envelope);
  payload.imageSize = reader.u32();
  for (uint8_t &value : payload.sha256) value = reader.u8();
  return payload.imageSize == 0U ? CodecResult::InvalidPayload
                                 : CodecResult::Ok;
}

CodecResult writeFirmwareChunk(Envelope &envelope,
                               const FirmwareChunkPayload &payload) {
  if (payload.dataLength == 0U ||
      payload.dataLength > kFirmwareChunkDataBytes) {
    return CodecResult::InvalidPayload;
  }
  envelope.messageType = MessageType::FirmwareChunk;
  PayloadWriter writer(envelope);
  writer.u32(payload.offset);
  for (uint16_t index = 0; index < payload.dataLength; ++index) {
    writer.u8(payload.data[index]);
  }
  return CodecResult::Ok;
}

CodecResult readFirmwareChunk(const Envelope &envelope,
                              FirmwareChunkPayload &payload) {
  if (envelope.messageType != MessageType::FirmwareChunk ||
      envelope.payloadLength <= kFirmwareChunkHeaderWireBytes ||
      envelope.payloadLength > kMaxPayloadBytes) {
    return CodecResult::InvalidPayload;
  }
  payload = FirmwareChunkPayload{};
  PayloadReader reader(envelope);
  payload.offset = reader.u32();
  payload.dataLength = static_cast<uint16_t>(
      envelope.payloadLength - kFirmwareChunkHeaderWireBytes);
  for (uint16_t index = 0; index < payload.dataLength; ++index) {
    payload.data[index] = reader.u8();
  }
  return CodecResult::Ok;
}

CodecResult writeFirmwareEnd(Envelope &envelope) {
  envelope.messageType = MessageType::FirmwareEnd;
  envelope.payloadLength = 0U;
  return CodecResult::Ok;
}

CodecResult readFirmwareEnd(const Envelope &envelope) {
  return validate(envelope, MessageType::FirmwareEnd, 0U);
}

CodecResult writeFirmwareStatus(Envelope &envelope,
                                const FirmwareStatusPayload &payload) {
  envelope.messageType = MessageType::FirmwareStatus;
  envelope.flags = kFlagIsResponse |
                   (payload.error == FirmwareUpdateError::None
                        ? 0U
                        : kFlagIsError);
  PayloadWriter writer(envelope);
  writer.u8(static_cast<uint8_t>(payload.state));
  writer.u32(payload.nextOffset);
  writer.u32(payload.imageSize);
  writer.u16(static_cast<uint16_t>(payload.error));
  return CodecResult::Ok;
}

CodecResult readFirmwareStatus(const Envelope &envelope,
                               FirmwareStatusPayload &payload) {
  const CodecResult result = validate(envelope, MessageType::FirmwareStatus,
                                      kFirmwareStatusWireBytes);
  if (result != CodecResult::Ok) return result;
  PayloadReader reader(envelope);
  const auto state = static_cast<FirmwareUpdateState>(reader.u8());
  if (state > FirmwareUpdateState::Failed) return CodecResult::InvalidPayload;
  payload = FirmwareStatusPayload{};
  payload.state = state;
  payload.nextOffset = reader.u32();
  payload.imageSize = reader.u32();
  payload.error = static_cast<FirmwareUpdateError>(reader.u16());
  if (payload.error > FirmwareUpdateError::Busy) {
    return CodecResult::InvalidPayload;
  }
  return CodecResult::Ok;
}

}  // namespace sozo::node
