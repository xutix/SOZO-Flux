#include <NodeFirmwareTransfer.h>

#include <limits>

namespace sozo {

NodeFirmwareTransfer::NodeFirmwareTransfer(NodeTransport &transport,
                                           const uint32_t responseTimeoutMs,
                                           const uint8_t maxRetries)
    : transport_(transport),
      responseTimeoutMs_(responseTimeoutMs),
      maxRetries_(maxRetries) {}

bool NodeFirmwareTransfer::start(const node::NodeId nodeId,
                                 const uint8_t *image, const size_t imageSize,
                                 const uint8_t sha256[32],
                                 const uint32_t) {
  if (active() || image == nullptr || sha256 == nullptr || imageSize == 0U ||
      imageSize > std::numeric_limits<uint32_t>::max() ||
      !transport_.ready() || transport_.remoteNodeId() != nodeId) {
    return false;
  }
  image_ = image;
  for (size_t i = 0U; i < sizeof(sha256_); ++i) sha256_[i] = sha256[i];
  status_ = {};
  status_.state = NodeFirmwareTransferState::Starting;
  status_.nodeId = nodeId;
  status_.imageSize = static_cast<uint32_t>(imageSize);
  pending_ = {};
  pendingChunkEnd_ = 0U;
  retryCount_ = 0U;
  awaitingResponse_ = false;
  return true;
}

void NodeFirmwareTransfer::tick(const uint32_t nowMs) {
  if (!active()) return;
  if (!transport_.ready() || transport_.remoteNodeId() != status_.nodeId) {
    fail(node::FirmwareUpdateError::LinkLost);
    return;
  }
  if (awaitingResponse_) {
    if (static_cast<int32_t>(nowMs - responseDeadlineMs_) <= 0) return;
    if (retryCount_ >= maxRetries_) {
      fail(node::FirmwareUpdateError::Timeout);
      return;
    }
    sendPending(nowMs, true);
    return;
  }

  switch (status_.state) {
    case NodeFirmwareTransferState::Starting:
      sendBegin(nowMs);
      break;
    case NodeFirmwareTransferState::Sending:
      if (status_.confirmedBytes < status_.imageSize) {
        sendChunk(nowMs);
      } else {
        status_.state = NodeFirmwareTransferState::Finalizing;
        sendEnd(nowMs);
      }
      break;
    case NodeFirmwareTransferState::Finalizing:
      sendEnd(nowMs);
      break;
    default:
      break;
  }
}

bool NodeFirmwareTransfer::handleInbound(const node::Envelope &envelope) {
  if (!awaitingResponse_ || envelope.messageType != node::MessageType::FirmwareStatus ||
      envelope.sourceNodeId != status_.nodeId ||
      envelope.targetNodeId != node::kCoordinatorNodeId ||
      envelope.correlationId != pending_.correlationId) {
    return false;
  }

  node::FirmwareStatusPayload response{};
  if (node::readFirmwareStatus(envelope, response) != node::CodecResult::Ok ||
      response.imageSize != status_.imageSize) {
    fail(node::FirmwareUpdateError::InvalidImage);
    return true;
  }
  awaitingResponse_ = false;
  retryCount_ = 0U;
  if (response.error != node::FirmwareUpdateError::None) {
    fail(response.error);
    return true;
  }

  switch (pending_.messageType) {
    case node::MessageType::FirmwareBegin:
      if (response.state != node::FirmwareUpdateState::Receiving ||
          response.nextOffset > status_.imageSize) {
        fail(node::FirmwareUpdateError::InvalidImage);
      } else {
        status_.confirmedBytes = response.nextOffset;
        status_.state = NodeFirmwareTransferState::Sending;
      }
      break;
    case node::MessageType::FirmwareChunk:
      if (response.state != node::FirmwareUpdateState::Receiving ||
          response.nextOffset != pendingChunkEnd_) {
        fail(node::FirmwareUpdateError::UnexpectedOffset);
      } else {
        status_.confirmedBytes = response.nextOffset;
      }
      break;
    case node::MessageType::FirmwareEnd:
      if (response.state != node::FirmwareUpdateState::ReadyToRestart ||
          response.nextOffset != status_.imageSize) {
        fail(node::FirmwareUpdateError::InvalidImage);
      } else {
        status_.confirmedBytes = response.nextOffset;
        status_.state = NodeFirmwareTransferState::Succeeded;
        status_.error = node::FirmwareUpdateError::None;
      }
      break;
    default:
      fail(node::FirmwareUpdateError::InvalidImage);
      break;
  }
  return true;
}

void NodeFirmwareTransfer::onDisconnected() {
  if (active()) fail(node::FirmwareUpdateError::LinkLost);
}

void NodeFirmwareTransfer::reset() {
  image_ = nullptr;
  pending_ = {};
  status_ = {};
  awaitingResponse_ = false;
  retryCount_ = 0U;
}

bool NodeFirmwareTransfer::active() const {
  return status_.state == NodeFirmwareTransferState::Starting ||
         status_.state == NodeFirmwareTransferState::Sending ||
         status_.state == NodeFirmwareTransferState::Finalizing;
}

const NodeFirmwareTransferStatus &NodeFirmwareTransfer::status() const {
  return status_;
}

bool NodeFirmwareTransfer::sendBegin(const uint32_t nowMs) {
  node::FirmwareBeginPayload payload{};
  payload.imageSize = status_.imageSize;
  for (size_t i = 0U; i < sizeof(payload.sha256); ++i) {
    payload.sha256[i] = sha256_[i];
  }
  pending_ = {};
  pending_.channelId = static_cast<uint16_t>(node::ServiceId::UpdateFirmware);
  pending_.flags = node::kFlagRequiresAck;
  pending_.sourceNodeId = node::kCoordinatorNodeId;
  pending_.targetNodeId = status_.nodeId;
  pending_.sequence = sequence_++;
  pending_.timestampMs = nowMs;
  pending_.correlationId = correlation_++;
  if (node::writeFirmwareBegin(pending_, payload) != node::CodecResult::Ok) {
    fail(node::FirmwareUpdateError::InvalidImage);
    return false;
  }
  return sendPending(nowMs, false);
}

bool NodeFirmwareTransfer::sendChunk(const uint32_t nowMs) {
  node::FirmwareChunkPayload payload{};
  payload.offset = status_.confirmedBytes;
  const uint32_t remaining = status_.imageSize - status_.confirmedBytes;
  payload.dataLength = static_cast<uint16_t>(
      remaining < node::kFirmwareChunkDataBytes ? remaining
                                               : node::kFirmwareChunkDataBytes);
  for (uint16_t i = 0U; i < payload.dataLength; ++i) {
    payload.data[i] = image_[payload.offset + i];
  }
  pendingChunkEnd_ = payload.offset + payload.dataLength;
  pending_ = {};
  pending_.channelId = static_cast<uint16_t>(node::ServiceId::UpdateFirmware);
  pending_.flags = node::kFlagRequiresAck;
  pending_.sourceNodeId = node::kCoordinatorNodeId;
  pending_.targetNodeId = status_.nodeId;
  pending_.sequence = sequence_++;
  pending_.timestampMs = nowMs;
  pending_.correlationId = correlation_++;
  if (node::writeFirmwareChunk(pending_, payload) != node::CodecResult::Ok) {
    fail(node::FirmwareUpdateError::InvalidImage);
    return false;
  }
  return sendPending(nowMs, false);
}

bool NodeFirmwareTransfer::sendEnd(const uint32_t nowMs) {
  pending_ = {};
  pending_.channelId = static_cast<uint16_t>(node::ServiceId::UpdateFirmware);
  pending_.flags = node::kFlagRequiresAck;
  pending_.sourceNodeId = node::kCoordinatorNodeId;
  pending_.targetNodeId = status_.nodeId;
  pending_.sequence = sequence_++;
  pending_.timestampMs = nowMs;
  pending_.correlationId = correlation_++;
  if (node::writeFirmwareEnd(pending_) != node::CodecResult::Ok) {
    fail(node::FirmwareUpdateError::InvalidImage);
    return false;
  }
  return sendPending(nowMs, false);
}

bool NodeFirmwareTransfer::sendPending(const uint32_t nowMs,
                                       const bool retry) {
  if (!transport_.send(pending_)) return false;
  awaitingResponse_ = true;
  responseDeadlineMs_ = nowMs + responseTimeoutMs_;
  if (retry) ++retryCount_;
  return true;
}

void NodeFirmwareTransfer::fail(const node::FirmwareUpdateError error) {
  status_.state = NodeFirmwareTransferState::Failed;
  status_.error = error;
  awaitingResponse_ = false;
}

}  // namespace sozo
