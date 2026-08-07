#include <SozoNodeMessages.h>
#include <LedGeometry.h>

#include "../TestHarness.h"

namespace {

using sozo::node::AudioFeaturesPayload;
using sozo::node::BindRequestPayload;
using sozo::node::BindResultPayload;
using sozo::node::CapabilitiesPayload;
using sozo::node::CommandReceiptPayload;
using sozo::node::CodecResult;
using sozo::node::ControlModePayload;
using sozo::node::Envelope;
using sozo::node::FirmwareBeginPayload;
using sozo::node::FirmwareChunkPayload;
using sozo::node::FirmwareStatusPayload;
using sozo::node::FirmwareUpdateError;
using sozo::node::FirmwareUpdateState;
using sozo::node::HeartbeatPayload;
using sozo::node::LedCountPayload;
using sozo::node::MessageType;
using sozo::node::NodeControlMode;
using sozo::node::SceneSnapshotPayload;
using sozo::node::StatusSnapshotPayload;

template <typename Payload>
Payload roundTrip(const MessageType type, const Payload &input,
                  CodecResult (*write)(Envelope &, const Payload &),
                  CodecResult (*read)(const Envelope &, Payload &)) {
  Envelope envelope{};
  envelope.messageType = type;
  CHECK_EQ(CodecResult::Ok, write(envelope, input));
  CHECK_TRUE(envelope.payloadLength > 0);

  Payload output{};
  CHECK_EQ(CodecResult::Ok, read(envelope, output));
  Envelope encodedAgain{};
  encodedAgain.messageType = type;
  CHECK_EQ(CodecResult::Ok, write(encodedAgain, output));
  CHECK_EQ(envelope.payloadLength, encodedAgain.payloadLength);
  CHECK_MEMORY_EQ(envelope.payload, encodedAgain.payload,
                  envelope.payloadLength);
  return output;
}

void test_capabilities_round_trip() {
  CapabilitiesPayload input{};
  input.capabilityBits = 0x0FU;
  input.maxLedCount = 512;
  input.maxPacketBytes = sozo::node::kMaxPacketBytes;
  input.protocolMin = 1;
  input.protocolMax = 1;
  input.firmwareMajor = 2;
  input.firmwareMinor = 3;
  input.firmwarePatch = 4;
  input.controlMode = NodeControlMode::FollowMain;
  input.hardwareProfile = 7;
  input.bound = true;
  const CapabilitiesPayload output =
      roundTrip(MessageType::Capabilities, input,
                sozo::node::writeCapabilities,
                sozo::node::readCapabilities);
  CHECK_EQ(input.capabilityBits, output.capabilityBits);
  CHECK_EQ(input.maxLedCount, output.maxLedCount);
  CHECK_EQ(input.maxPacketBytes, output.maxPacketBytes);
  CHECK_EQ(input.firmwarePatch, output.firmwarePatch);
  CHECK_EQ(input.controlMode, output.controlMode);
  CHECK_EQ(input.hardwareProfile, output.hardwareProfile);
  CHECK_EQ(input.bound, output.bound);
}

void test_heartbeat_round_trip() {
  HeartbeatPayload input{};
  input.uptimeMs = 123456U;
  input.lastAppliedSceneRevision = 77U;
  input.freeHeapBytes = 54321U;
  input.errorFlags = 0x1234U;
  input.controlMode = NodeControlMode::OfflineHold;
  input.pairingWindowOpen = true;
  const HeartbeatPayload output =
      roundTrip(MessageType::Heartbeat, input, sozo::node::writeHeartbeat,
                sozo::node::readHeartbeat);
  CHECK_EQ(input.uptimeMs, output.uptimeMs);
  CHECK_EQ(input.lastAppliedSceneRevision,
           output.lastAppliedSceneRevision);
  CHECK_EQ(input.freeHeapBytes, output.freeHeapBytes);
  CHECK_EQ(input.errorFlags, output.errorFlags);
  CHECK_EQ(input.controlMode, output.controlMode);
  CHECK_EQ(input.pairingWindowOpen, output.pairingWindowOpen);
}

void test_scene_snapshot_round_trips_every_effect_parameter() {
  SceneSnapshotPayload input{};
  input.effectMode = 10;
  input.brightness = 201;
  input.primaryRed = 1;
  input.primaryGreen = 2;
  input.primaryBlue = 3;
  input.rainbowStyle = 2;
  input.flowSpeed = 91;
  input.cometTail = 72;
  input.cometSpeed = 49;
  input.cometDensity = 8;
  input.cometBackground = 55;
  input.cometRandom = true;
  input.audioSensitivityX100 = 499;
  input.audioColorGainX100 = 401;
  input.audioHueDrive = 3;
  input.breathFloorPercent = 59;
  input.secondaryRed = 4;
  input.secondaryGreen = 5;
  input.secondaryBlue = 6;
  input.pulseAmplitudePercent = 99;
  input.pulseHeightPercent = 98;
  input.animationBrightness = 233;
  input.audioColorStyle = 4;
  input.cometColorStyle = 3;
  input.manualLitPixelCount = 321;
  input.spatialProfile = 1;
  input.spatialFlags = 0x03;
  const SceneSnapshotPayload output =
      roundTrip(MessageType::SceneSnapshot, input,
                sozo::node::writeSceneSnapshot,
                sozo::node::readSceneSnapshot);
  CHECK_EQ(input.effectMode, output.effectMode);
  CHECK_EQ(input.brightness, output.brightness);
  CHECK_EQ(input.primaryRed, output.primaryRed);
  CHECK_EQ(input.primaryGreen, output.primaryGreen);
  CHECK_EQ(input.primaryBlue, output.primaryBlue);
  CHECK_EQ(input.rainbowStyle, output.rainbowStyle);
  CHECK_EQ(input.flowSpeed, output.flowSpeed);
  CHECK_EQ(input.cometTail, output.cometTail);
  CHECK_EQ(input.cometSpeed, output.cometSpeed);
  CHECK_EQ(input.cometDensity, output.cometDensity);
  CHECK_EQ(input.cometBackground, output.cometBackground);
  CHECK_EQ(input.cometRandom, output.cometRandom);
  CHECK_EQ(input.audioSensitivityX100, output.audioSensitivityX100);
  CHECK_EQ(input.audioColorGainX100, output.audioColorGainX100);
  CHECK_EQ(input.audioHueDrive, output.audioHueDrive);
  CHECK_EQ(input.breathFloorPercent, output.breathFloorPercent);
  CHECK_EQ(input.secondaryRed, output.secondaryRed);
  CHECK_EQ(input.secondaryGreen, output.secondaryGreen);
  CHECK_EQ(input.secondaryBlue, output.secondaryBlue);
  CHECK_EQ(input.pulseAmplitudePercent, output.pulseAmplitudePercent);
  CHECK_EQ(input.pulseHeightPercent, output.pulseHeightPercent);
  CHECK_EQ(input.animationBrightness, output.animationBrightness);
  CHECK_EQ(input.audioColorStyle, output.audioColorStyle);
  CHECK_EQ(input.cometColorStyle, output.cometColorStyle);
  CHECK_EQ(input.manualLitPixelCount, output.manualLitPixelCount);
  CHECK_EQ(input.spatialProfile, output.spatialProfile);
  CHECK_EQ(input.spatialFlags, output.spatialFlags);
}

void test_scene_snapshot_contains_no_hardware_gpio() {
  CHECK_TRUE(sizeof(SceneSnapshotPayload) <= 40);
  CHECK_TRUE(sozo::node::kSceneSnapshotWireBytes <= 40);
}

void test_audio_features_round_trip() {
  AudioFeaturesPayload input{};
  input.volumeX100 = 22050;
  input.fastEnergyX100 = 19000;
  input.slowEnergyX100 = 12340;
  input.beatPulseX100 = 25500;
  input.framesRead = 9001U;
  input.available = true;
  const AudioFeaturesPayload output =
      roundTrip(MessageType::AudioFeatures, input,
                sozo::node::writeAudioFeatures,
                sozo::node::readAudioFeatures);
  CHECK_EQ(input.volumeX100, output.volumeX100);
  CHECK_EQ(input.fastEnergyX100, output.fastEnergyX100);
  CHECK_EQ(input.slowEnergyX100, output.slowEnergyX100);
  CHECK_EQ(input.beatPulseX100, output.beatPulseX100);
  CHECK_EQ(input.framesRead, output.framesRead);
  CHECK_EQ(input.available, output.available);
}

void test_binding_messages_round_trip() {
  BindRequestPayload request{};
  request.coordinatorNodeId = 0xAABBCCDDU;
  request.bindingNonce = 0x10203040U;
  const BindRequestPayload requestOutput =
      roundTrip(MessageType::BindRequest, request,
                sozo::node::writeBindRequest,
                sozo::node::readBindRequest);
  CHECK_EQ(request.coordinatorNodeId, requestOutput.coordinatorNodeId);
  CHECK_EQ(request.bindingNonce, requestOutput.bindingNonce);

  BindResultPayload result{};
  result.accepted = true;
  result.nodeId = 0x01020304U;
  result.bindingNonce = request.bindingNonce;
  result.errorCode = 0;
  const BindResultPayload resultOutput =
      roundTrip(MessageType::BindResult, result,
                sozo::node::writeBindResult,
                sozo::node::readBindResult);
  CHECK_EQ(result.accepted, resultOutput.accepted);
  CHECK_EQ(result.nodeId, resultOutput.nodeId);
  CHECK_EQ(result.bindingNonce, resultOutput.bindingNonce);
  CHECK_EQ(result.errorCode, resultOutput.errorCode);
}

void test_reader_rejects_wrong_message_type_and_payload_length() {
  Envelope envelope{};
  envelope.messageType = MessageType::Heartbeat;
  SceneSnapshotPayload scene{};
  CHECK_EQ(CodecResult::InvalidPayload,
           sozo::node::readSceneSnapshot(envelope, scene));

  CHECK_EQ(CodecResult::Ok, sozo::node::writeSceneSnapshot(envelope, scene));
  --envelope.payloadLength;
  CHECK_EQ(CodecResult::InvalidPayload,
           sozo::node::readSceneSnapshot(envelope, scene));
}

void test_command_receipt_round_trip_confirms_scene_application() {
  CommandReceiptPayload input{};
  input.accepted = true;
  input.lastAppliedSceneRevision = 7U;
  input.errorCode = 0U;
  Envelope envelope{};
  envelope.correlationId = 41U;
  CHECK_EQ(CodecResult::Ok, sozo::node::writeCommandReceipt(envelope, input));
  CHECK_EQ(MessageType::Ack, envelope.messageType);
  CHECK_TRUE((envelope.flags & sozo::node::kFlagIsResponse) != 0U);

  CommandReceiptPayload output{};
  CHECK_EQ(CodecResult::Ok, sozo::node::readCommandReceipt(envelope, output));
  CHECK_TRUE(output.accepted);
  CHECK_EQ(7U, output.lastAppliedSceneRevision);
  CHECK_EQ(0U, output.errorCode);
}

void test_rejected_command_receipt_preserves_error_code() {
  CommandReceiptPayload input{};
  input.accepted = false;
  input.lastAppliedSceneRevision = 6U;
  input.errorCode = 3U;
  Envelope envelope{};
  CHECK_EQ(CodecResult::Ok, sozo::node::writeCommandReceipt(envelope, input));

  CommandReceiptPayload output{};
  CHECK_EQ(CodecResult::Ok, sozo::node::readCommandReceipt(envelope, output));
  CHECK_TRUE(!output.accepted);
  CHECK_EQ(6U, output.lastAppliedSceneRevision);
  CHECK_EQ(3U, output.errorCode);
}

void test_status_snapshot_round_trip_is_separate_from_receipt() {
  StatusSnapshotPayload input{};
  input.lastAppliedSceneRevision = 9U;
  input.freeHeapBytes = 45678U;
  input.errorFlags = 0x12U;
  input.controlMode = NodeControlMode::OfflineHold;
  input.pairingWindowOpen = true;
  input.ledCount = 60U;
  Envelope envelope{};
  CHECK_EQ(CodecResult::Ok, sozo::node::writeStatusSnapshot(envelope, input));
  CHECK_EQ(MessageType::StatusSnapshot, envelope.messageType);

  StatusSnapshotPayload output{};
  CHECK_EQ(CodecResult::Ok, sozo::node::readStatusSnapshot(envelope, output));
  CHECK_EQ(9U, output.lastAppliedSceneRevision);
  CHECK_EQ(45678U, output.freeHeapBytes);
  CHECK_EQ(0x12U, output.errorFlags);
  CHECK_EQ(NodeControlMode::OfflineHold, output.controlMode);
  CHECK_TRUE(output.pairingWindowOpen);
  CHECK_EQ(60U, output.ledCount);
}

void test_led_count_request_round_trips_and_rejects_zero() {
  LedCountPayload input{};
  input.ledCount = 512U;
  const LedCountPayload output =
      roundTrip(MessageType::LedCountRequest, input,
                sozo::node::writeLedCountRequest,
                sozo::node::readLedCountRequest);
  CHECK_EQ(512U, output.ledCount);

  Envelope invalid{};
  invalid.messageType = MessageType::LedCountRequest;
  invalid.payloadLength = sozo::node::kLedCountWireBytes;
  invalid.payload[0] = 0U;
  invalid.payload[1] = 0U;
  LedCountPayload rejected{};
  CHECK_EQ(CodecResult::InvalidPayload,
           sozo::node::readLedCountRequest(invalid, rejected));
}

void test_status_request_has_no_payload_and_is_validated() {
  Envelope request{};
  CHECK_EQ(CodecResult::Ok, sozo::node::writeStatusRequest(request));
  CHECK_EQ(MessageType::StatusRequest, request.messageType);
  CHECK_EQ(0U, request.payloadLength);
  CHECK_EQ(CodecResult::Ok, sozo::node::readStatusRequest(request));

  request.payloadLength = 1U;
  CHECK_EQ(CodecResult::InvalidPayload,
           sozo::node::readStatusRequest(request));
}

void test_control_mode_request_round_trips_and_rejects_invalid_mode() {
  ControlModePayload input{};
  input.controlMode = NodeControlMode::Independent;
  const ControlModePayload output =
      roundTrip(MessageType::ControlModeRequest, input,
                sozo::node::writeControlModeRequest,
                sozo::node::readControlModeRequest);
  CHECK_EQ(NodeControlMode::Independent, output.controlMode);

  Envelope invalid{};
  invalid.messageType = MessageType::ControlModeRequest;
  invalid.payloadLength = 1U;
  invalid.payload[0] = 99U;
  ControlModePayload rejected{};
  CHECK_EQ(CodecResult::InvalidPayload,
           sozo::node::readControlModeRequest(invalid, rejected));
}

void test_firmware_update_messages_preserve_hash_offset_and_progress() {
  FirmwareBeginPayload begin{};
  begin.imageSize = 552544U;
  for (uint8_t index = 0; index < 32U; ++index) {
    begin.sha256[index] = static_cast<uint8_t>(0xA0U + index);
  }
  const FirmwareBeginPayload decodedBegin =
      roundTrip(MessageType::FirmwareBegin, begin,
                sozo::node::writeFirmwareBegin,
                sozo::node::readFirmwareBegin);
  CHECK_EQ(552544U, decodedBegin.imageSize);
  CHECK_MEMORY_EQ(begin.sha256, decodedBegin.sha256, 32U);

  FirmwareChunkPayload chunk{};
  chunk.offset = 304U;
  chunk.dataLength = 7U;
  const uint8_t expectedData[7] = {0xE9U, 1U, 2U, 3U, 4U, 5U, 6U};
  for (uint16_t index = 0; index < chunk.dataLength; ++index) {
    chunk.data[index] = expectedData[index];
  }
  const FirmwareChunkPayload decodedChunk =
      roundTrip(MessageType::FirmwareChunk, chunk,
                sozo::node::writeFirmwareChunk,
                sozo::node::readFirmwareChunk);
  CHECK_EQ(304U, decodedChunk.offset);
  CHECK_EQ(7U, decodedChunk.dataLength);
  CHECK_MEMORY_EQ(expectedData, decodedChunk.data, 7U);

  FirmwareStatusPayload status{};
  status.state = FirmwareUpdateState::Receiving;
  status.nextOffset = 311U;
  status.imageSize = 552544U;
  status.error = FirmwareUpdateError::None;
  const FirmwareStatusPayload decodedStatus =
      roundTrip(MessageType::FirmwareStatus, status,
                sozo::node::writeFirmwareStatus,
                sozo::node::readFirmwareStatus);
  CHECK_EQ(FirmwareUpdateState::Receiving, decodedStatus.state);
  CHECK_EQ(311U, decodedStatus.nextOffset);
  CHECK_EQ(552544U, decodedStatus.imageSize);
  CHECK_EQ(FirmwareUpdateError::None, decodedStatus.error);

  Envelope end{};
  CHECK_EQ(CodecResult::Ok, sozo::node::writeFirmwareEnd(end));
  CHECK_EQ(MessageType::FirmwareEnd, end.messageType);
  CHECK_EQ(0U, end.payloadLength);
  CHECK_EQ(CodecResult::Ok, sozo::node::readFirmwareEnd(end));
}

void test_led_geometry_maps_only_active_physical_pixels() {
  const sozo::lighting::LedGeometry geometry{60U, 62U, false};
  CHECK_TRUE(sozo::lighting::isValidLedGeometry(geometry));
  CHECK_EQ(0, sozo::lighting::logicalIndexForPhysical(geometry, 0U));
  CHECK_EQ(59, sozo::lighting::logicalIndexForPhysical(geometry, 59U));
  CHECK_EQ(-1, sozo::lighting::logicalIndexForPhysical(geometry, 60U));
  CHECK_EQ(-1, sozo::lighting::logicalIndexForPhysical(geometry, 61U));
}

void test_led_geometry_reverses_active_pixels_without_mapping_tail() {
  const sozo::lighting::LedGeometry geometry{60U, 62U, true};
  CHECK_TRUE(sozo::lighting::isValidLedGeometry(geometry));
  CHECK_EQ(59, sozo::lighting::logicalIndexForPhysical(geometry, 0U));
  CHECK_EQ(0, sozo::lighting::logicalIndexForPhysical(geometry, 59U));
  CHECK_EQ(-1, sozo::lighting::logicalIndexForPhysical(geometry, 60U));
}

void test_led_geometry_rejects_empty_and_oversized_active_ranges() {
  CHECK_TRUE(!sozo::lighting::isValidLedGeometry({0U, 62U, false}));
  CHECK_TRUE(!sozo::lighting::isValidLedGeometry({63U, 62U, false}));
}

}  // namespace

int main(int, char **) {
  test_capabilities_round_trip();
  test_heartbeat_round_trip();
  test_scene_snapshot_round_trips_every_effect_parameter();
  test_scene_snapshot_contains_no_hardware_gpio();
  test_audio_features_round_trip();
  test_binding_messages_round_trip();
  test_reader_rejects_wrong_message_type_and_payload_length();
  test_command_receipt_round_trip_confirms_scene_application();
  test_rejected_command_receipt_preserves_error_code();
  test_status_snapshot_round_trip_is_separate_from_receipt();
  test_led_count_request_round_trips_and_rejects_zero();
  test_status_request_has_no_payload_and_is_validated();
  test_control_mode_request_round_trips_and_rejects_invalid_mode();
  test_firmware_update_messages_preserve_hash_offset_and_progress();
  test_led_geometry_maps_only_active_physical_pixels();
  test_led_geometry_reverses_active_pixels_without_mapping_tail();
  test_led_geometry_rejects_empty_and_oversized_active_ranges();
  return sozo::test::finish("message tests");
}
