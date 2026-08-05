#pragma once

#include <SozoNodeMessages.h>

namespace sozo {

enum class NodeConnectionState : uint8_t {
  Discovered = 0,
  Connecting,
  Ready,
  Offline,
  Error,
};

enum class NodeRegistrationResult : uint8_t {
  Added = 0,
  Updated,
  InvalidNode,
  Full,
};

struct NodeRecord {
  bool occupied{false};
  node::NodeId nodeId{0};
  NodeConnectionState connectionState{NodeConnectionState::Discovered};
  node::CapabilitiesPayload capabilities{};
  // Link lifecycle, command receipts, and diagnostic snapshots are distinct.
  node::StatusSnapshotPayload status{};
  uint32_t lastAppliedSceneRevision{0};
  uint16_t lastCommandError{0};
  uint32_t lastLinkChangeMs{0};
  uint32_t lastReceiptMs{0};
  uint32_t lastStatusMs{0};
};

class NodeRegistry {
 public:
  static constexpr size_t kCapacity = 8;

  NodeRegistrationResult registerCapabilities(
      node::NodeId nodeId, const node::CapabilitiesPayload &capabilities,
      uint32_t nowMs);
  bool markOffline(node::NodeId nodeId, uint32_t nowMs);
  bool recordCommandReceipt(node::NodeId nodeId,
                            const node::CommandReceiptPayload &receipt,
                            uint32_t nowMs);
  bool updateStatus(node::NodeId nodeId,
                    const node::StatusSnapshotPayload &status,
                    uint32_t nowMs);

  const NodeRecord *find(node::NodeId nodeId) const;
  NodeRecord *find(node::NodeId nodeId);
  const NodeRecord *recordAt(size_t index) const;
  size_t size() const;

 private:
  NodeRecord records_[kCapacity]{};
};

}  // namespace sozo
