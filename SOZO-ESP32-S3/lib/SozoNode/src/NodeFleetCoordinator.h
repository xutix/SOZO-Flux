#pragma once

#include <stddef.h>
#include <stdint.h>

#include <NodeCoordinator.h>
#include <NodeFleetTransport.h>

namespace sozo {

class NodeFleetCoordinator {
 public:
  static constexpr size_t kMaxConcurrentNodes = 4U;
  static constexpr uint32_t kDefaultPairingWindowMs = 60000U;

  explicit NodeFleetCoordinator(NodeFleetTransport &transport);
  ~NodeFleetCoordinator();

  bool begin();
  void tick(uint32_t nowMs, const SpaceSceneSnapshot &scene,
            const AudioFrame &audioFrame);

  const NodeRegistry &registry() const;
  size_t onlineCount() const;
  size_t capacity() const;
  bool nodeReady() const;
  NodeTransportState transportState() const;
  const char *operationName() const;
  bool workerBusy() const;
  uint32_t timeoutCount() const;

  bool openPairingWindow(
      uint32_t nowMs, uint32_t durationMs = kDefaultPairingWindowMs);
  bool pairingWindowOpen(uint32_t nowMs) const;
  uint32_t pairingRemainingMs(uint32_t nowMs) const;
  bool scanning() const;

  bool requestNodeControlMode(node::NodeId nodeId,
                              node::NodeControlMode mode, uint32_t nowMs);
  bool requestIndependentScene(node::NodeId nodeId,
                               const PersistedLightingState &state,
                               uint32_t nowMs);
  bool requestNodeLedCount(node::NodeId nodeId, uint16_t ledCount,
                           uint32_t nowMs);
  bool requestNodeFirmwareUpdate(node::NodeId nodeId, const uint8_t *image,
                                 size_t imageSize, const uint8_t sha256[32],
                                 uint32_t nowMs);
  NodeFirmwareTransferStatus firmwareUpdateStatus() const;

 private:
  NodeCoordinator *sessionFor(node::NodeId nodeId) const;

  NodeFleetTransport &transport_;
  NodeRegistry registry_{};
  NodeCoordinator *sessions_[kMaxConcurrentNodes]{};
  size_t sessionCount_{0};
  node::NodeId firmwareNodeId_{0U};
};

}  // namespace sozo
