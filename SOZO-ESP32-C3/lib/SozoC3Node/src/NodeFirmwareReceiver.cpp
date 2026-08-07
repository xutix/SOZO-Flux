#include <NodeFirmwareReceiver.h>

namespace sozo::c3 {

NodeFirmwareReceiver::NodeFirmwareReceiver(FirmwareWriterPort &writer,
                                           const uint32_t maxImageBytes,
                                           const uint32_t inactivityTimeoutMs)
    : writer_(writer),
      maxImageBytes_(maxImageBytes),
      inactivityTimeoutMs_(inactivityTimeoutMs) {
  status_.state = node::FirmwareUpdateState::Idle;
}

bool NodeFirmwareReceiver::begin(const node::FirmwareBeginPayload &payload,
                                 const uint32_t nowMs) {
  if (active_) {
    bool sameImage = payload.imageSize == status_.imageSize;
    for (size_t i = 0U; sameImage && i < sizeof(expectedSha256_); ++i) {
      sameImage = payload.sha256[i] == expectedSha256_[i];
    }
    if (sameImage) {
      lastActivityMs_ = nowMs;
      status_.error = node::FirmwareUpdateError::None;
      return true;
    }
    status_.error = node::FirmwareUpdateError::Busy;
    return false;
  }
  if (payload.imageSize == 0U || payload.imageSize > maxImageBytes_) {
    status_ = {};
    status_.state = node::FirmwareUpdateState::Failed;
    status_.imageSize = payload.imageSize;
    status_.error = payload.imageSize > maxImageBytes_
                        ? node::FirmwareUpdateError::ImageTooLarge
                        : node::FirmwareUpdateError::InvalidImage;
    return false;
  }
  if (!writer_.begin(payload.imageSize)) {
    status_ = {};
    status_.state = node::FirmwareUpdateState::Failed;
    status_.imageSize = payload.imageSize;
    status_.error = node::FirmwareUpdateError::WriteFailed;
    return false;
  }

  status_ = {};
  status_.state = node::FirmwareUpdateState::Receiving;
  status_.imageSize = payload.imageSize;
  for (size_t i = 0U; i < sizeof(expectedSha256_); ++i) {
    expectedSha256_[i] = payload.sha256[i];
  }
  lastActivityMs_ = nowMs;
  active_ = true;
  restartRequested_ = false;
  return true;
}

bool NodeFirmwareReceiver::append(const node::FirmwareChunkPayload &payload,
                                  const uint32_t nowMs) {
  if (!active_) {
    status_.error = node::FirmwareUpdateError::NotStarted;
    return false;
  }
  if (payload.dataLength == 0U ||
      payload.dataLength > node::kFirmwareChunkDataBytes) {
    status_.error = node::FirmwareUpdateError::InvalidImage;
    return false;
  }

  const uint32_t chunkEnd = payload.offset + payload.dataLength;
  if (chunkEnd < payload.offset || chunkEnd > status_.imageSize) {
    status_.error = node::FirmwareUpdateError::InvalidImage;
    return false;
  }
  if (payload.offset < status_.nextOffset && chunkEnd <= status_.nextOffset) {
    lastActivityMs_ = nowMs;
    status_.error = node::FirmwareUpdateError::None;
    return true;
  }
  if (payload.offset != status_.nextOffset) {
    status_.error = node::FirmwareUpdateError::UnexpectedOffset;
    return false;
  }
  if (!writer_.write(payload.data, payload.dataLength)) {
    fail(node::FirmwareUpdateError::WriteFailed);
    return false;
  }

  status_.nextOffset = chunkEnd;
  status_.error = node::FirmwareUpdateError::None;
  lastActivityMs_ = nowMs;
  return true;
}

bool NodeFirmwareReceiver::finish(const uint32_t nowMs) {
  if (!active_) {
    status_.error = node::FirmwareUpdateError::NotStarted;
    return false;
  }
  lastActivityMs_ = nowMs;
  if (status_.nextOffset != status_.imageSize) {
    status_.error = node::FirmwareUpdateError::InvalidImage;
    return false;
  }

  status_.state = node::FirmwareUpdateState::Verifying;
  const FirmwareCommitResult result = writer_.finish(expectedSha256_);
  if (result != FirmwareCommitResult::Ok) {
    fail(result == FirmwareCommitResult::HashMismatch
             ? node::FirmwareUpdateError::HashMismatch
             : node::FirmwareUpdateError::WriteFailed);
    return false;
  }

  active_ = false;
  restartRequested_ = true;
  status_.state = node::FirmwareUpdateState::ReadyToRestart;
  status_.error = node::FirmwareUpdateError::None;
  return true;
}

void NodeFirmwareReceiver::tick(const uint32_t nowMs) {
  if (active_ && nowMs - lastActivityMs_ > inactivityTimeoutMs_) {
    fail(node::FirmwareUpdateError::Timeout);
  }
}

void NodeFirmwareReceiver::onDisconnected() {
  if (active_) fail(node::FirmwareUpdateError::LinkLost);
}

bool NodeFirmwareReceiver::active() const { return active_; }

bool NodeFirmwareReceiver::restartRequested() const {
  return restartRequested_;
}

const node::FirmwareStatusPayload &NodeFirmwareReceiver::status() const {
  return status_;
}

void NodeFirmwareReceiver::fail(const node::FirmwareUpdateError error) {
  if (active_) writer_.abort();
  active_ = false;
  restartRequested_ = false;
  status_.state = node::FirmwareUpdateState::Failed;
  status_.error = error;
}

}  // namespace sozo::c3
