#include <SozoNodeProtocol.h>

#include <string.h>

namespace sozo::node {
namespace {

void writeU16(uint8_t *destination, const uint16_t value) {
  destination[0] = static_cast<uint8_t>(value & 0xFFU);
  destination[1] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
}

void writeU32(uint8_t *destination, const uint32_t value) {
  destination[0] = static_cast<uint8_t>(value & 0xFFU);
  destination[1] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
  destination[2] = static_cast<uint8_t>((value >> 16U) & 0xFFU);
  destination[3] = static_cast<uint8_t>((value >> 24U) & 0xFFU);
}

uint16_t readU16(const uint8_t *source) {
  return static_cast<uint16_t>(source[0]) |
         (static_cast<uint16_t>(source[1]) << 8U);
}

uint32_t readU32(const uint8_t *source) {
  return static_cast<uint32_t>(source[0]) |
         (static_cast<uint32_t>(source[1]) << 8U) |
         (static_cast<uint32_t>(source[2]) << 16U) |
         (static_cast<uint32_t>(source[3]) << 24U);
}

}  // namespace

size_t encodedSize(const Envelope &envelope) {
  if (envelope.payloadLength > kMaxPayloadBytes) return 0;
  return static_cast<size_t>(kHeaderBytes) + envelope.payloadLength + kCrcBytes;
}

uint16_t crc16Ccitt(const uint8_t *data, const size_t length) {
  if (data == nullptr && length != 0) return 0;
  uint16_t crc = 0xFFFFU;
  for (size_t index = 0; index < length; ++index) {
    crc ^= static_cast<uint16_t>(data[index]) << 8U;
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x8000U) != 0
                ? static_cast<uint16_t>((crc << 1U) ^ 0x1021U)
                : static_cast<uint16_t>(crc << 1U);
    }
  }
  return crc;
}

CodecResult encodeEnvelope(const Envelope &envelope, uint8_t *destination,
                           const size_t capacity, size_t &written) {
  written = 0;
  if (destination == nullptr) return CodecResult::InvalidArgument;
  if (envelope.protocolVersion != kProtocolVersion) {
    return CodecResult::UnsupportedVersion;
  }
  if (!isKnownMessageType(envelope.messageType)) {
    return CodecResult::InvalidMessageType;
  }
  if (envelope.payloadLength > kMaxPayloadBytes) {
    return CodecResult::PayloadTooLarge;
  }

  const size_t required = encodedSize(envelope);
  if (capacity < required) return CodecResult::BufferTooSmall;

  writeU16(destination, kProtocolMagic);
  destination[2] = envelope.protocolVersion;
  destination[3] = static_cast<uint8_t>(envelope.messageType);
  writeU16(destination + 4, envelope.channelId);
  writeU16(destination + 6, envelope.flags);
  writeU32(destination + 8, envelope.sourceNodeId);
  writeU32(destination + 12, envelope.targetNodeId);
  writeU32(destination + 16, envelope.sequence);
  writeU32(destination + 20, envelope.timestampMs);
  writeU32(destination + 24, envelope.sceneRevision);
  writeU32(destination + 28, envelope.correlationId);
  writeU16(destination + 32, envelope.payloadLength);
  if (envelope.payloadLength != 0) {
    memcpy(destination + kHeaderBytes, envelope.payload,
           envelope.payloadLength);
  }

  const uint16_t crc = crc16Ccitt(destination, required - kCrcBytes);
  writeU16(destination + required - kCrcBytes, crc);
  written = required;
  return CodecResult::Ok;
}

CodecResult decodeEnvelope(const uint8_t *data, const size_t length,
                           Envelope &envelope) {
  if (data == nullptr) return CodecResult::InvalidArgument;
  if (length < static_cast<size_t>(kHeaderBytes + kCrcBytes)) {
    return CodecResult::LengthMismatch;
  }
  if (readU16(data) != kProtocolMagic) return CodecResult::InvalidMagic;
  if (data[2] != kProtocolVersion) return CodecResult::UnsupportedVersion;

  const auto messageType = static_cast<MessageType>(data[3]);
  if (!isKnownMessageType(messageType)) {
    return CodecResult::InvalidMessageType;
  }

  const uint16_t payloadLength = readU16(data + 32);
  if (payloadLength > kMaxPayloadBytes) return CodecResult::PayloadTooLarge;
  const size_t expectedLength =
      static_cast<size_t>(kHeaderBytes) + payloadLength + kCrcBytes;
  if (length != expectedLength) return CodecResult::LengthMismatch;

  const uint16_t expectedCrc = readU16(data + length - kCrcBytes);
  if (crc16Ccitt(data, length - kCrcBytes) != expectedCrc) {
    return CodecResult::CrcMismatch;
  }

  envelope = Envelope{};
  envelope.protocolVersion = data[2];
  envelope.messageType = messageType;
  envelope.channelId = readU16(data + 4);
  envelope.flags = readU16(data + 6);
  envelope.sourceNodeId = readU32(data + 8);
  envelope.targetNodeId = readU32(data + 12);
  envelope.sequence = readU32(data + 16);
  envelope.timestampMs = readU32(data + 20);
  envelope.sceneRevision = readU32(data + 24);
  envelope.correlationId = readU32(data + 28);
  envelope.payloadLength = payloadLength;
  if (payloadLength != 0) {
    memcpy(envelope.payload, data + kHeaderBytes, payloadLength);
  }
  return CodecResult::Ok;
}

}  // namespace sozo::node
