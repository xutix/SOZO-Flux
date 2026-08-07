#pragma once

#include <NodeTransport.h>

#include <cstddef>
#include <cstdint>

namespace sozo {

enum class NodeFirmwareTransferState : uint8_t {
  Idle = 0,
  Starting,
  Sending,
  Finalizing,
  Succeeded,
  Failed,
};

struct NodeFirmwareTransferStatus {
  NodeFirmwareTransferState state{NodeFirmwareTransferState::Idle};
  node::NodeId nodeId{0U};
  uint32_t imageSize{0U};
  uint32_t confirmedBytes{0U};
  node::FirmwareUpdateError error{node::FirmwareUpdateError::None};
};

class NodeFirmwareTransfer {
 public:
  NodeFirmwareTransfer(NodeTransport &transport, uint32_t responseTimeoutMs,
                       uint8_t maxRetries);

  bool start(node::NodeId nodeId, const uint8_t *image, size_t imageSize,
             const uint8_t sha256[32], uint32_t nowMs);
  void tick(uint32_t nowMs);
  bool handleInbound(const node::Envelope &envelope);
  void onDisconnected();
  void reset();

  bool active() const;
  const NodeFirmwareTransferStatus &status() const;

 private:
  bool sendBegin(uint32_t nowMs);
  bool sendChunk(uint32_t nowMs);
  bool sendEnd(uint32_t nowMs);
  bool sendPending(uint32_t nowMs, bool retry);
  void fail(node::FirmwareUpdateError error);

  NodeTransport &transport_;
  const uint8_t *image_{nullptr};
  uint8_t sha256_[32]{};
  node::Envelope pending_{};
  NodeFirmwareTransferStatus status_{};
  uint32_t responseTimeoutMs_{0U};
  uint32_t responseDeadlineMs_{0U};
  uint32_t sequence_{1U};
  uint32_t correlation_{1U};
  uint32_t pendingChunkEnd_{0U};
  uint8_t retryCount_{0U};
  uint8_t maxRetries_{0U};
  bool awaitingResponse_{false};
};

}  // namespace sozo
