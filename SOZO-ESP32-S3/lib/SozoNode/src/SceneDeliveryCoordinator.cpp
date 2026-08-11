#include "SceneDeliveryCoordinator.h"

namespace sozo {

SceneDeliveryCoordinator::SceneDeliveryCoordinator(
    LightingSceneOrchestrator &scenes, LocalLightingTarget &localTarget,
    NodeFleetCoordinator &nodes)
    : scenes_(scenes), localTarget_(localTarget), nodes_(nodes) {}

void SceneDeliveryCoordinator::tick(const uint32_t nowMs) {
  deliverLocal(nowMs);
  const NodeRegistry &registry = nodes_.registry();
  for (size_t index = 0U; index < NodeRegistry::kCapacity; ++index) {
    const NodeRecord *record = registry.recordAt(index);
    if (record != nullptr) deliverRemote(*record, nowMs);
  }
}

SceneDeliveryCoordinator::Attempt *SceneDeliveryCoordinator::attemptFor(
    const LightingTargetId targetId) {
  Attempt *empty = nullptr;
  for (Attempt &attempt : attempts_) {
    if (attempt.targetId == targetId) return &attempt;
    if (attempt.targetId == 0U && empty == nullptr) empty = &attempt;
  }
  if (empty != nullptr) empty->targetId = targetId;
  return empty;
}

void SceneDeliveryCoordinator::deliverLocal(const uint32_t nowMs) {
  const DesiredLightingState *desired =
      scenes_.desiredFor(kLocalLightingTargetId);
  if (desired == nullptr || !desired->pending() || !localTarget_.available()) {
    return;
  }
  if (localTarget_.apply(desired->scene, desired->revision, nowMs)) {
    scenes_.markDelivered(kLocalLightingTargetId, desired->revision);
  }
}

void SceneDeliveryCoordinator::deliverRemote(const NodeRecord &record,
                                               const uint32_t nowMs) {
  if (record.connectionState != NodeConnectionState::Ready ||
      !record.capabilities.bound ||
      (record.capabilities.capabilityBits &
       node::capabilityMask(node::Capability::LightOutput)) == 0U) {
    return;
  }
  if (record.status.controlMode != node::NodeControlMode::Independent) {
    nodes_.requestNodeControlMode(record.nodeId,
                                  node::NodeControlMode::Independent, nowMs);
    return;
  }

  const DesiredLightingState *desired = scenes_.desiredFor(record.nodeId);
  if (desired == nullptr || !desired->pending()) return;
  if (record.lastAppliedSceneRevision == desired->revision &&
      record.lastCommandError == 0U) {
    scenes_.markDelivered(record.nodeId, desired->revision);
    return;
  }

  Attempt *attempt = attemptFor(record.nodeId);
  if (attempt == nullptr) return;
  if (attempt->revision == desired->revision) {
    if (record.lastReceiptMs != attempt->receiptBefore &&
        record.lastCommandError != 0U) {
      return;
    }
    if (static_cast<uint32_t>(nowMs - attempt->attemptedAtMs) <
        kRetryDelayMs) {
      return;
    }
  }

  if (!nodes_.requestDesiredScene(record.nodeId, desired->scene,
                                  desired->revision, nowMs)) {
    return;
  }
  attempt->revision = desired->revision;
  attempt->receiptBefore = record.lastReceiptMs;
  attempt->attemptedAtMs = nowMs;
}

}  // namespace sozo
