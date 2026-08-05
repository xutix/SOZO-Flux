#include <NodeRegistry.h>

namespace sozo {

NodeRegistrationResult NodeRegistry::registerCapabilities(
    const node::NodeId nodeId,
    const node::CapabilitiesPayload &capabilities, const uint32_t nowMs) {
  if (nodeId == 0 || nodeId == node::kBroadcastNodeId) {
    return NodeRegistrationResult::InvalidNode;
  }

  if (NodeRecord *existing = find(nodeId)) {
    existing->capabilities = capabilities;
    existing->connectionState = NodeConnectionState::Ready;
    existing->lastLinkChangeMs = nowMs;
    return NodeRegistrationResult::Updated;
  }

  for (NodeRecord &record : records_) {
    if (record.occupied) continue;
    record = NodeRecord{};
    record.occupied = true;
    record.nodeId = nodeId;
    record.connectionState = NodeConnectionState::Ready;
    record.capabilities = capabilities;
    record.lastLinkChangeMs = nowMs;
    return NodeRegistrationResult::Added;
  }
  return NodeRegistrationResult::Full;
}

bool NodeRegistry::markOffline(const node::NodeId nodeId,
                               const uint32_t nowMs) {
  NodeRecord *record = find(nodeId);
  if (record == nullptr) return false;
  record->connectionState = NodeConnectionState::Offline;
  record->lastLinkChangeMs = nowMs;
  return true;
}

bool NodeRegistry::recordCommandReceipt(
    const node::NodeId nodeId, const node::CommandReceiptPayload &receipt,
    const uint32_t nowMs) {
  NodeRecord *record = find(nodeId);
  if (record == nullptr) return false;
  record->lastAppliedSceneRevision = receipt.lastAppliedSceneRevision;
  record->lastCommandError = receipt.errorCode;
  record->lastReceiptMs = nowMs;
  return true;
}

bool NodeRegistry::updateStatus(const node::NodeId nodeId,
                                const node::StatusSnapshotPayload &status,
                                const uint32_t nowMs) {
  NodeRecord *record = find(nodeId);
  if (record == nullptr) return false;
  record->status = status;
  record->lastAppliedSceneRevision = status.lastAppliedSceneRevision;
  record->lastStatusMs = nowMs;
  return true;
}

const NodeRecord *NodeRegistry::find(const node::NodeId nodeId) const {
  for (const NodeRecord &record : records_) {
    if (record.occupied && record.nodeId == nodeId) return &record;
  }
  return nullptr;
}

NodeRecord *NodeRegistry::find(const node::NodeId nodeId) {
  for (NodeRecord &record : records_) {
    if (record.occupied && record.nodeId == nodeId) return &record;
  }
  return nullptr;
}

const NodeRecord *NodeRegistry::recordAt(const size_t index) const {
  return index < kCapacity && records_[index].occupied ? &records_[index]
                                                       : nullptr;
}

size_t NodeRegistry::size() const {
  size_t count = 0;
  for (const NodeRecord &record : records_) {
    if (record.occupied) ++count;
  }
  return count;
}

}  // namespace sozo
