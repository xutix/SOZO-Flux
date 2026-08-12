#pragma once

#include <SozoNodeMessages.h>

#include <cstddef>
#include <cstdint>

namespace sozo::c3 {

enum class FirmwareCommitResult : uint8_t {
  Ok = 0,
  HashMismatch,
  WriteFailed,
};

class FirmwareWriterPort {
 public:
  virtual ~FirmwareWriterPort() = default;
  virtual bool begin(uint32_t imageSize) = 0;
  virtual bool write(const uint8_t *data, size_t dataLength) = 0;
  virtual FirmwareCommitResult finish(const uint8_t expectedSha256[32]) = 0;
  virtual void abort() = 0;
};

class NodeFirmwareReceiver {
 public:
  NodeFirmwareReceiver(FirmwareWriterPort &writer, uint32_t maxImageBytes,
                       uint32_t inactivityTimeoutMs);

  bool begin(const node::FirmwareBeginPayload &payload, uint32_t nowMs);
  bool append(const node::FirmwareChunkPayload &payload, uint32_t nowMs);
  bool finish(uint32_t nowMs);
  void tick(uint32_t nowMs);
  void onDisconnected();

  bool active() const;
  bool restartRequested() const;
  const node::FirmwareStatusPayload &status() const;

 private:
  void fail(node::FirmwareUpdateError error);

  FirmwareWriterPort &writer_;
  uint32_t maxImageBytes_{0U};
  uint32_t inactivityTimeoutMs_{0U};
  uint32_t lastActivityMs_{0U};
  uint8_t expectedSha256_[32]{};
  node::FirmwareStatusPayload status_{};
  bool active_{false};
  bool restartRequested_{false};
};

}  // namespace sozo::c3
