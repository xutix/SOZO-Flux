#pragma once

#include <stddef.h>
#include <stdint.h>

namespace sozo::node {

using NodeId = uint32_t;

constexpr uint16_t kProtocolMagic = 0x535AU;
constexpr uint8_t kProtocolVersion = 1;
constexpr NodeId kCoordinatorNodeId = 1U;
constexpr NodeId kBroadcastNodeId = 0xFFFFFFFFU;
constexpr uint16_t kHeaderBytes = 34;
constexpr uint16_t kCrcBytes = 2;
constexpr uint16_t kMaxPayloadBytes = 156;
constexpr uint16_t kMaxPacketBytes =
    kHeaderBytes + kMaxPayloadBytes + kCrcBytes;

constexpr uint16_t kFlagRequiresAck = 1U << 0U;
constexpr uint16_t kFlagIsResponse = 1U << 1U;
constexpr uint16_t kFlagIsError = 1U << 2U;

enum class MessageType : uint8_t {
  Capabilities = 1,
  Heartbeat = 2,
  SceneSnapshot = 3,
  AudioFeatures = 4,
  BindRequest = 5,
  BindResult = 6,
  TimeSync = 7,
  Ack = 8,
  StatusSnapshot = 9,
  StatusRequest = 10,
  ControlModeRequest = 11,
  LedCountRequest = 12,
};

enum class TopicId : uint16_t {
  SpaceScene = 1,
  SpaceTime = 2,
  SpaceAudioFeatures = 3,
  NodeIndependentScene = 4,
  NodeState = 10,
  NodeInput = 11,
  NodeSensor = 12,
  NodeCapabilities = 13,
};

enum class ServiceId : uint16_t {
  PairNode = 100,
  GetCapabilities = 101,
  GetNodeState = 102,
  SetControlMode = 103,
  RestartNode = 104,
  ClearBinding = 105,
  SetLedCount = 106,
};

enum class NodeControlMode : uint8_t {
  FollowMain = 0,
  Independent = 1,
  OfflineHold = 2,
};

enum class Capability : uint32_t {
  None = 0,
  LightOutput = 1U << 0U,
  ButtonInput = 1U << 1U,
  RotaryInput = 1U << 2U,
  SensorInput = 1U << 3U,
};

constexpr uint32_t capabilityMask(const Capability capability) {
  return static_cast<uint32_t>(capability);
}

struct Envelope {
  uint8_t protocolVersion{kProtocolVersion};
  MessageType messageType{MessageType::Heartbeat};
  uint16_t channelId{0};
  uint16_t flags{0};
  NodeId sourceNodeId{0};
  NodeId targetNodeId{0};
  uint32_t sequence{0};
  uint32_t timestampMs{0};
  uint32_t sceneRevision{0};
  uint32_t correlationId{0};
  uint16_t payloadLength{0};
  uint8_t payload[kMaxPayloadBytes]{};
};

enum class CodecResult : uint8_t {
  Ok = 0,
  InvalidArgument,
  BufferTooSmall,
  InvalidMagic,
  UnsupportedVersion,
  InvalidMessageType,
  InvalidPayload,
  PayloadTooLarge,
  LengthMismatch,
  CrcMismatch,
};

constexpr bool isKnownMessageType(const MessageType type) {
  return type >= MessageType::Capabilities &&
         type <= MessageType::LedCountRequest;
}

size_t encodedSize(const Envelope &envelope);
uint16_t crc16Ccitt(const uint8_t *data, size_t length);

CodecResult encodeEnvelope(const Envelope &envelope, uint8_t *destination,
                           size_t capacity, size_t &written);
CodecResult decodeEnvelope(const uint8_t *data, size_t length,
                           Envelope &envelope);

}  // namespace sozo::node
