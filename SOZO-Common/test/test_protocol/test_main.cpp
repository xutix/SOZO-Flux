#include <cstring>

#include <SozoNodeProtocol.h>
#include "../TestHarness.h"

namespace {

using sozo::node::CodecResult;
using sozo::node::Envelope;
using sozo::node::MessageType;
using sozo::node::TopicId;

Envelope makeEnvelope() {
  Envelope envelope{};
  envelope.protocolVersion = sozo::node::kProtocolVersion;
  envelope.messageType = MessageType::SceneSnapshot;
  envelope.channelId = static_cast<uint16_t>(TopicId::SpaceScene);
  envelope.flags = sozo::node::kFlagRequiresAck;
  envelope.sourceNodeId = 0x01020304U;
  envelope.targetNodeId = 0x11223344U;
  envelope.sequence = 17U;
  envelope.timestampMs = 987654U;
  envelope.sceneRevision = 23U;
  envelope.correlationId = 41U;
  envelope.payloadLength = 5;
  envelope.payload[0] = 0x10;
  envelope.payload[1] = 0x20;
  envelope.payload[2] = 0x30;
  envelope.payload[3] = 0x40;
  envelope.payload[4] = 0x50;
  return envelope;
}

void test_protocol_constants_fit_ble_packet_budget() {
  CHECK_EQ(2, sozo::node::kProtocolVersion);
  CHECK_TRUE(sozo::node::kMaxPacketBytes <= 192);
  CHECK_TRUE(sozo::node::kMaxPayloadBytes >= 128);
}

void test_envelope_round_trips_all_header_fields_and_payload() {
  const Envelope input = makeEnvelope();
  uint8_t bytes[sozo::node::kMaxPacketBytes]{};
  size_t written = 0;

  CHECK_EQ(
      static_cast<int>(CodecResult::Ok),
      static_cast<int>(sozo::node::encodeEnvelope(
          input, bytes, sizeof(bytes), written)));
  CHECK_EQ(sozo::node::encodedSize(input), written);

  Envelope output{};
  CHECK_EQ(
      static_cast<int>(CodecResult::Ok),
      static_cast<int>(sozo::node::decodeEnvelope(bytes, written, output)));
  CHECK_EQ(input.protocolVersion, output.protocolVersion);
  CHECK_EQ(static_cast<uint8_t>(input.messageType),
           static_cast<uint8_t>(output.messageType));
  CHECK_EQ(input.channelId, output.channelId);
  CHECK_EQ(input.flags, output.flags);
  CHECK_EQ(input.sourceNodeId, output.sourceNodeId);
  CHECK_EQ(input.targetNodeId, output.targetNodeId);
  CHECK_EQ(input.sequence, output.sequence);
  CHECK_EQ(input.timestampMs, output.timestampMs);
  CHECK_EQ(input.sceneRevision, output.sceneRevision);
  CHECK_EQ(input.correlationId, output.correlationId);
  CHECK_EQ(input.payloadLength, output.payloadLength);
  CHECK_TRUE(std::memcmp(input.payload, output.payload, input.payloadLength) == 0);
}

void test_wire_format_is_little_endian_and_starts_with_magic() {
  const Envelope input = makeEnvelope();
  uint8_t bytes[sozo::node::kMaxPacketBytes]{};
  size_t written = 0;
  CHECK_EQ(
      static_cast<int>(CodecResult::Ok),
      static_cast<int>(sozo::node::encodeEnvelope(
          input, bytes, sizeof(bytes), written)));

  CHECK_EQ(0x5A, bytes[0]);
  CHECK_EQ(0x53, bytes[1]);
  CHECK_EQ(0x04, bytes[8]);
  CHECK_EQ(0x03, bytes[9]);
  CHECK_EQ(0x02, bytes[10]);
  CHECK_EQ(0x01, bytes[11]);
}

void test_decode_rejects_crc_corruption() {
  const Envelope input = makeEnvelope();
  uint8_t bytes[sozo::node::kMaxPacketBytes]{};
  size_t written = 0;
  CHECK_EQ(
      static_cast<int>(CodecResult::Ok),
      static_cast<int>(sozo::node::encodeEnvelope(
          input, bytes, sizeof(bytes), written)));
  bytes[sozo::node::kHeaderBytes] ^= 0x80;

  Envelope output{};
  CHECK_EQ(
      static_cast<int>(CodecResult::CrcMismatch),
      static_cast<int>(sozo::node::decodeEnvelope(bytes, written, output)));
}

void test_decode_rejects_truncated_packet() {
  const Envelope input = makeEnvelope();
  uint8_t bytes[sozo::node::kMaxPacketBytes]{};
  size_t written = 0;
  CHECK_EQ(
      static_cast<int>(CodecResult::Ok),
      static_cast<int>(sozo::node::encodeEnvelope(
          input, bytes, sizeof(bytes), written)));

  Envelope output{};
  CHECK_EQ(
      static_cast<int>(CodecResult::LengthMismatch),
      static_cast<int>(sozo::node::decodeEnvelope(bytes, written - 1, output)));
}

void test_decode_rejects_unknown_protocol_version() {
  Envelope input = makeEnvelope();
  input.protocolVersion = sozo::node::kProtocolVersion + 1;
  uint8_t bytes[sozo::node::kMaxPacketBytes]{};
  size_t written = 0;
  CHECK_EQ(
      static_cast<int>(CodecResult::UnsupportedVersion),
      static_cast<int>(sozo::node::encodeEnvelope(
          input, bytes, sizeof(bytes), written)));
}

void test_encode_rejects_oversized_payload_and_small_buffer() {
  Envelope input = makeEnvelope();
  input.payloadLength = sozo::node::kMaxPayloadBytes + 1;
  uint8_t bytes[sozo::node::kMaxPacketBytes]{};
  size_t written = 99;
  CHECK_EQ(
      static_cast<int>(CodecResult::PayloadTooLarge),
      static_cast<int>(sozo::node::encodeEnvelope(
          input, bytes, sizeof(bytes), written)));
  CHECK_EQ(0U, written);

  input = makeEnvelope();
  CHECK_EQ(
      static_cast<int>(CodecResult::BufferTooSmall),
      static_cast<int>(sozo::node::encodeEnvelope(
          input, bytes, sozo::node::kHeaderBytes, written)));
}

}  // namespace

int main(int, char **) {
  test_protocol_constants_fit_ble_packet_budget();
  test_envelope_round_trips_all_header_fields_and_payload();
  test_wire_format_is_little_endian_and_starts_with_magic();
  test_decode_rejects_crc_corruption();
  test_decode_rejects_truncated_packet();
  test_decode_rejects_unknown_protocol_version();
  test_encode_rejects_oversized_payload_and_small_buffer();
  return sozo::test::finish("protocol tests");
}
