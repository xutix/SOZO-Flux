#include <NodeFleetCoordinator.h>

#include <new>

namespace sozo {

NodeFleetCoordinator::NodeFleetCoordinator(NodeFleetTransport &transport)
    : transport_(transport) {}

NodeFleetCoordinator::~NodeFleetCoordinator() {
  for (size_t index = 0; index < sessionCount_; ++index) {
    delete sessions_[index];
    sessions_[index] = nullptr;
  }
}

bool NodeFleetCoordinator::begin() {
  if (!transport_.begin()) return false;
  sessionCount_ = transport_.capacity();
  if (sessionCount_ > kMaxConcurrentNodes) {
    sessionCount_ = kMaxConcurrentNodes;
  }
  for (size_t index = 0; index < sessionCount_; ++index) {
    NodeTransport *link = transport_.linkAt(index);
    if (link == nullptr) return false;
    sessions_[index] = new (std::nothrow) NodeCoordinator(*link, registry_);
    if (sessions_[index] == nullptr || !sessions_[index]->begin()) return false;
  }
  return true;
}

void NodeFleetCoordinator::tick(
    const uint32_t nowMs, const SpaceSceneSnapshot &scene,
    const AudioFrame &audioFrame) {
  transport_.tick(nowMs);
  const bool pairingAllowed = transport_.pairingWindowOpen(nowMs);
  for (size_t index = 0; index < sessionCount_; ++index) {
    if (sessions_[index] != nullptr) {
      sessions_[index]->setBindingAllowed(pairingAllowed);
      sessions_[index]->tick(nowMs, scene, audioFrame);
      const NodeTransport *link = transport_.linkAt(index);
      if (link != nullptr && link->ready() && !link->capabilities().bound &&
          !pairingAllowed && sessions_[index]->activeNodeId() != 0U) {
        transport_.releaseLink(index);
      }
    }
  }
  if (firmwareNodeId_ != 0U) {
    NodeCoordinator *firmwareSession = sessionFor(firmwareNodeId_);
    if (firmwareSession == nullptr ||
        firmwareSession->firmwareUpdateStatus().state ==
            NodeFirmwareTransferState::Idle) {
      firmwareNodeId_ = 0U;
    }
  }
}

const NodeRegistry &NodeFleetCoordinator::registry() const { return registry_; }

size_t NodeFleetCoordinator::onlineCount() const {
  size_t count = 0;
  for (size_t index = 0; index < sessionCount_; ++index) {
    if (sessions_[index] != nullptr && sessions_[index]->nodeReady()) ++count;
  }
  return count;
}

size_t NodeFleetCoordinator::capacity() const { return sessionCount_; }

bool NodeFleetCoordinator::nodeReady() const { return onlineCount() > 0U; }

NodeTransportState NodeFleetCoordinator::transportState() const {
  NodeTransportState aggregate =
      scanning() ? NodeTransportState::Searching : NodeTransportState::Idle;
  for (size_t index = 0; index < sessionCount_; ++index) {
    const NodeTransport *link = transport_.linkAt(index);
    if (link == nullptr) continue;
    if (link->state() == NodeTransportState::Ready) {
      return NodeTransportState::Ready;
    }
    if (link->state() != NodeTransportState::Idle) aggregate = link->state();
  }
  return aggregate;
}

const char *NodeFleetCoordinator::operationName() const {
  for (size_t index = 0; index < sessionCount_; ++index) {
    const NodeTransport *link = transport_.linkAt(index);
    if (link != nullptr && link->workerBusy()) return link->operationName();
  }
  return scanning() ? "scanning" : "idle";
}

bool NodeFleetCoordinator::workerBusy() const {
  for (size_t index = 0; index < sessionCount_; ++index) {
    const NodeTransport *link = transport_.linkAt(index);
    if (link != nullptr && link->workerBusy()) return true;
  }
  return false;
}

uint32_t NodeFleetCoordinator::timeoutCount() const {
  uint32_t count = 0U;
  for (size_t index = 0; index < sessionCount_; ++index) {
    const NodeTransport *link = transport_.linkAt(index);
    if (link != nullptr) count += link->timeoutCount();
  }
  return count;
}

bool NodeFleetCoordinator::openPairingWindow(const uint32_t nowMs,
                                             const uint32_t durationMs) {
  return durationMs > 0U && transport_.openPairingWindow(nowMs, durationMs);
}

bool NodeFleetCoordinator::pairingWindowOpen(const uint32_t nowMs) const {
  return transport_.pairingWindowOpen(nowMs);
}

uint32_t NodeFleetCoordinator::pairingRemainingMs(
    const uint32_t nowMs) const {
  return transport_.pairingRemainingMs(nowMs);
}

bool NodeFleetCoordinator::scanning() const { return transport_.scanning(); }

bool NodeFleetCoordinator::requestNodeControlMode(
    const node::NodeId nodeId, const node::NodeControlMode mode,
    const uint32_t nowMs) {
  NodeCoordinator *session = sessionFor(nodeId);
  return session != nullptr &&
         session->requestNodeControlMode(nodeId, mode, nowMs);
}

bool NodeFleetCoordinator::requestIndependentScene(
    const node::NodeId nodeId, const PersistedLightingState &state,
    const uint32_t nowMs) {
  NodeCoordinator *session = sessionFor(nodeId);
  return session != nullptr &&
         session->requestIndependentScene(nodeId, state, nowMs);
}

bool NodeFleetCoordinator::requestDesiredScene(
    const node::NodeId nodeId, const LightingScene &scene,
    const uint32_t revision, const uint32_t nowMs) {
  NodeCoordinator *session = sessionFor(nodeId);
  return session != nullptr &&
         session->requestDesiredScene(nodeId, scene, revision, nowMs);
}

bool NodeFleetCoordinator::requestNodeLedCount(const node::NodeId nodeId,
                                               const uint16_t ledCount,
                                               const uint32_t nowMs) {
  NodeCoordinator *session = sessionFor(nodeId);
  return session != nullptr &&
         session->requestNodeLedCount(nodeId, ledCount, nowMs);
}

bool NodeFleetCoordinator::requestNodeGeometry(
    const node::NodeId nodeId, const node::LedGeometryPayload &geometry,
    const uint32_t nowMs) {
  NodeCoordinator *session = sessionFor(nodeId);
  return session != nullptr &&
         session->requestNodeGeometry(nodeId, geometry, nowMs);
}

bool NodeFleetCoordinator::requestNodeFirmwareUpdate(
    const node::NodeId nodeId, const uint8_t *image, const size_t imageSize,
    const uint8_t sha256[32], const uint32_t nowMs) {
  if (firmwareNodeId_ != 0U) {
    NodeCoordinator *current = sessionFor(firmwareNodeId_);
    if (current != nullptr &&
        current->firmwareUpdateStatus().state !=
            NodeFirmwareTransferState::Failed &&
        current->firmwareUpdateStatus().state !=
            NodeFirmwareTransferState::Idle) {
      return false;
    }
    firmwareNodeId_ = 0U;
  }
  NodeCoordinator *session = sessionFor(nodeId);
  if (session == nullptr || !session->requestNodeFirmwareUpdate(
                                nodeId, image, imageSize, sha256, nowMs)) {
    return false;
  }
  firmwareNodeId_ = nodeId;
  return true;
}

NodeFirmwareTransferStatus NodeFleetCoordinator::firmwareUpdateStatus() const {
  NodeFirmwareTransferStatus status{};
  NodeCoordinator *session = sessionFor(firmwareNodeId_);
  return session == nullptr ? status : session->firmwareUpdateStatus();
}

NodeCoordinator *NodeFleetCoordinator::sessionFor(
    const node::NodeId nodeId) const {
  if (nodeId == 0U) return nullptr;
  for (size_t index = 0; index < sessionCount_; ++index) {
    NodeCoordinator *session = sessions_[index];
    if (session != nullptr && session->activeNodeId() == nodeId) return session;
  }
  return nullptr;
}

}  // namespace sozo
