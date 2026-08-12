#include <NodeCoordinator.h>

namespace sozo {
namespace {

constexpr uint32_t kBindTimeoutMs = 5000U;
constexpr uint32_t kSceneReceiptTimeoutMs = 2000U;
constexpr uint32_t kStatusRequestTimeoutMs = 2000U;
constexpr uint32_t kAudioIntervalMs = 33U;

}  // namespace

NodeCoordinator::NodeCoordinator(NodeTransport &transport)
    : transport_(transport), registry_(ownedRegistry_),
      firmwareTransfer_(transport, 3000U, 3U) {}

NodeCoordinator::NodeCoordinator(NodeTransport &transport,
                                 NodeRegistry &registry)
    : transport_(transport), registry_(registry),
      firmwareTransfer_(transport, 3000U, 3U) {}

bool NodeCoordinator::begin() { return transport_.begin(); }

void NodeCoordinator::tick(const uint32_t nowMs,
                           const AudioFrame &audioFrame) {
  lastTickMs_ = nowMs;
  transport_.tick(nowMs);
  observeTransportLifecycle(nowMs);
  handleInbound(nowMs);
  bus_.expireRequests(nowMs);

  firmwareTransfer_.tick(nowMs);
  if (firmwareTransfer_.active() ||
      (firmwareTransfer_.status().state ==
           NodeFirmwareTransferState::Succeeded &&
       transport_.readyGeneration() == firmwareReadyGeneration_)) {
    return;
  }

  if (!transport_.ready()) return;
  handleReadyGeneration(nowMs);
  if (!bindingReady_) {
    if (!transport_.capabilities().bound && !bindingAllowed_) return;
    if (activeNodeId_ != 0U && registry_.find(activeNodeId_) == nullptr) {
      registry_.registerCapabilities(activeNodeId_, transport_.capabilities(),
                                     nowMs);
    }
    if (!bindingRequested_) requestBinding(nowMs);
    return;
  }

  if (statusRequestedGeneration_ != transport_.readyGeneration()) {
    requestStatus(nowMs);
  }
  sendAudio(nowMs, desiredEffectMode_, audioFrame);
}

const NodeRegistry &NodeCoordinator::registry() const { return registry_; }
NodeTransportState NodeCoordinator::transportState() const {
  return transport_.state();
}
bool NodeCoordinator::nodeReady() const {
  return transport_.ready() && bindingReady_;
}
node::NodeId NodeCoordinator::activeNodeId() const { return activeNodeId_; }
void NodeCoordinator::setBindingAllowed(const bool allowed) {
  bindingAllowed_ = allowed;
}
const char *NodeCoordinator::operationName() const {
  return transport_.operationName();
}
bool NodeCoordinator::workerBusy() const { return transport_.workerBusy(); }
uint32_t NodeCoordinator::timeoutCount() const {
  return transport_.timeoutCount();
}

bool NodeCoordinator::requestNodeControlMode(const node::NodeId nodeId,
                                             const node::NodeControlMode mode,
                                             const uint32_t nowMs) {
  if (!isAvailableLightNode(nodeId) || pendingControlModeCorrelation_ != 0 ||
      (mode != node::NodeControlMode::FollowMain &&
       mode != node::NodeControlMode::Independent)) {
    return false;
  }
  node::ControlModePayload payload{};
  payload.controlMode = mode;
  node::Envelope envelope{};
  envelope.channelId = static_cast<uint16_t>(node::ServiceId::SetControlMode);
  envelope.flags = node::kFlagRequiresAck;
  envelope.sourceNodeId = node::kCoordinatorNodeId;
  envelope.targetNodeId = nodeId;
  envelope.sequence = outboundSequence_++;
  envelope.timestampMs = nowMs;
  envelope.correlationId = correlationSequence_++;
  if (node::writeControlModeRequest(envelope, payload) != node::CodecResult::Ok ||
      bus_.awaitResponse(envelope.correlationId,
                         nowMs + kSceneReceiptTimeoutMs,
                         onControlModeReceipt, this) != node::BusResult::Ok) {
    return false;
  }
  if (!transport_.send(envelope)) {
    bus_.cancelResponse(envelope.correlationId);
    return false;
  }
  pendingControlModeCorrelation_ = envelope.correlationId;
  return true;
}

bool NodeCoordinator::requestDesiredScene(
    const node::NodeId nodeId, const LightingScene &desiredScene,
    const uint32_t revision, const uint32_t nowMs) {
  const NodeRecord *record = registry_.find(nodeId);
  if (!isAvailableLightNode(nodeId) || record == nullptr ||
      record->status.controlMode != node::NodeControlMode::Independent ||
      pendingIndependentSceneCorrelation_ != 0 || revision == 0U) {
    return false;
  }
  const node::SceneSnapshotPayload scene = makeSceneSnapshot(desiredScene);
  node::Envelope envelope{};
  envelope.messageType = node::MessageType::SceneSnapshot;
  envelope.channelId =
      static_cast<uint16_t>(node::TopicId::NodeIndependentScene);
  envelope.flags = node::kFlagRequiresAck;
  envelope.sourceNodeId = node::kCoordinatorNodeId;
  envelope.targetNodeId = nodeId;
  envelope.sequence = outboundSequence_++;
  envelope.timestampMs = nowMs;
  envelope.sceneRevision = revision;
  envelope.correlationId = correlationSequence_++;
  if (node::writeSceneSnapshot(envelope, scene) != node::CodecResult::Ok ||
      bus_.awaitResponse(envelope.correlationId,
                         nowMs + kSceneReceiptTimeoutMs,
                         onIndependentSceneReceipt,
                         this) != node::BusResult::Ok) {
    return false;
  }
  if (!transport_.send(envelope)) {
    bus_.cancelResponse(envelope.correlationId);
    return false;
  }
  pendingIndependentSceneCorrelation_ = envelope.correlationId;
  desiredEffectMode_ = desiredScene.mode;
  return true;
}

bool NodeCoordinator::requestNodeLedCount(const node::NodeId nodeId,
                                          const uint16_t ledCount,
                                          const uint32_t nowMs) {
  const NodeRecord *record = registry_.find(nodeId);
  if (!isAvailableLightNode(nodeId) || record == nullptr || ledCount == 0U ||
      ledCount > record->capabilities.maxLedCount ||
      pendingLedCountCorrelation_ != 0U) {
    return false;
  }

  node::LedCountPayload payload{};
  payload.ledCount = ledCount;
  node::Envelope envelope{};
  envelope.channelId = static_cast<uint16_t>(node::ServiceId::SetLedCount);
  envelope.flags = node::kFlagRequiresAck;
  envelope.sourceNodeId = node::kCoordinatorNodeId;
  envelope.targetNodeId = nodeId;
  envelope.sequence = outboundSequence_++;
  envelope.timestampMs = nowMs;
  envelope.correlationId = correlationSequence_++;
  if (node::writeLedCountRequest(envelope, payload) != node::CodecResult::Ok ||
      bus_.awaitResponse(envelope.correlationId,
                         nowMs + kSceneReceiptTimeoutMs, onLedCountReceipt,
                         this) != node::BusResult::Ok) {
    return false;
  }
  if (!transport_.send(envelope)) {
    bus_.cancelResponse(envelope.correlationId);
    return false;
  }
  pendingLedCountCorrelation_ = envelope.correlationId;
  return true;
}

bool NodeCoordinator::requestNodeGeometry(
    const node::NodeId nodeId, const node::LedGeometryPayload &geometry,
    const uint32_t nowMs) {
  const NodeRecord *record = registry_.find(nodeId);
  if (!isAvailableLightNode(nodeId) || record == nullptr ||
      geometry.activeCount > record->capabilities.maxLedCount ||
      pendingLedCountCorrelation_ != 0U) {
    return false;
  }
  node::Envelope envelope{};
  envelope.channelId = static_cast<uint16_t>(node::ServiceId::SetLedGeometry);
  envelope.flags = node::kFlagRequiresAck;
  envelope.sourceNodeId = node::kCoordinatorNodeId;
  envelope.targetNodeId = nodeId;
  envelope.sequence = outboundSequence_++;
  envelope.timestampMs = nowMs;
  envelope.correlationId = correlationSequence_++;
  if (node::writeLedGeometryRequest(envelope, geometry) !=
          node::CodecResult::Ok ||
      bus_.awaitResponse(envelope.correlationId,
                         nowMs + kSceneReceiptTimeoutMs, onLedCountReceipt,
                         this) != node::BusResult::Ok) {
    return false;
  }
  if (!transport_.send(envelope)) {
    bus_.cancelResponse(envelope.correlationId);
    return false;
  }
  pendingLedCountCorrelation_ = envelope.correlationId;
  return true;
}

bool NodeCoordinator::requestNodeFirmwareUpdate(
    const node::NodeId nodeId, const uint8_t *image, const size_t imageSize,
    const uint8_t sha256[32], const uint32_t nowMs) {
  const NodeRecord *record = registry_.find(nodeId);
  if (!isAvailableLightNode(nodeId) || record == nullptr ||
      (record->capabilities.capabilityBits &
       node::capabilityMask(node::Capability::FirmwareUpdate)) == 0U) {
    return false;
  }
  if (!firmwareTransfer_.start(nodeId, image, imageSize, sha256, nowMs)) {
    return false;
  }
  firmwareReadyGeneration_ = transport_.readyGeneration();
  return true;
}

const NodeFirmwareTransferStatus &NodeCoordinator::firmwareUpdateStatus() const {
  return firmwareTransfer_.status();
}

void NodeCoordinator::onBindResponse(const node::RequestOutcome outcome,
                                     const node::Envelope *response,
                                     void *context) {
  auto &coordinator = *static_cast<NodeCoordinator *>(context);
  coordinator.bindingRequested_ = false;
  coordinator.pendingBindCorrelation_ = 0;
  if (outcome != node::RequestOutcome::Response || response == nullptr) return;

  node::BindResultPayload result{};
  if (node::readBindResult(*response, result) != node::CodecResult::Ok ||
      !result.accepted || result.nodeId != coordinator.transport_.remoteNodeId()) {
    return;
  }
  coordinator.bindingReady_ = true;
  if (NodeRecord *record =
          coordinator.registry_.find(coordinator.transport_.remoteNodeId())) {
    record->capabilities.bound = true;
  }
}

void NodeCoordinator::onStatusResponse(const node::RequestOutcome outcome,
                                       const node::Envelope *response,
                                       void *context) {
  auto &coordinator = *static_cast<NodeCoordinator *>(context);
  coordinator.pendingStatusCorrelation_ = 0;
  if (outcome != node::RequestOutcome::Response || response == nullptr) return;

  node::StatusSnapshotPayload status{};
  if (node::readStatusSnapshot(*response, status) != node::CodecResult::Ok) {
    return;
  }
  coordinator.registry_.updateStatus(response->sourceNodeId, status,
                                     coordinator.lastTickMs_);
}

void NodeCoordinator::onControlModeReceipt(
    const node::RequestOutcome outcome, const node::Envelope *response,
    void *context) {
  auto &coordinator = *static_cast<NodeCoordinator *>(context);
  coordinator.pendingControlModeCorrelation_ = 0;
  if (outcome != node::RequestOutcome::Response || response == nullptr) return;
  node::CommandReceiptPayload receipt{};
  if (node::readCommandReceipt(*response, receipt) != node::CodecResult::Ok) {
    return;
  }
  coordinator.registry_.recordCommandReceipt(response->sourceNodeId, receipt,
                                             coordinator.lastTickMs_);
  coordinator.statusRequestedGeneration_ = 0;
}

void NodeCoordinator::onIndependentSceneReceipt(
    const node::RequestOutcome outcome, const node::Envelope *response,
    void *context) {
  auto &coordinator = *static_cast<NodeCoordinator *>(context);
  coordinator.pendingIndependentSceneCorrelation_ = 0;
  if (outcome != node::RequestOutcome::Response || response == nullptr) return;
  node::CommandReceiptPayload receipt{};
  if (node::readCommandReceipt(*response, receipt) != node::CodecResult::Ok) {
    return;
  }
  coordinator.registry_.recordCommandReceipt(response->sourceNodeId, receipt,
                                             coordinator.lastTickMs_);
  coordinator.statusRequestedGeneration_ = 0;
}

void NodeCoordinator::onLedCountReceipt(
    const node::RequestOutcome outcome, const node::Envelope *response,
    void *context) {
  auto &coordinator = *static_cast<NodeCoordinator *>(context);
  coordinator.pendingLedCountCorrelation_ = 0;
  if (outcome != node::RequestOutcome::Response || response == nullptr) return;
  node::CommandReceiptPayload receipt{};
  if (node::readCommandReceipt(*response, receipt) != node::CodecResult::Ok) {
    return;
  }
  coordinator.registry_.recordCommandReceipt(response->sourceNodeId, receipt,
                                             coordinator.lastTickMs_);
  coordinator.statusRequestedGeneration_ = 0;
}

void NodeCoordinator::observeTransportLifecycle(const uint32_t nowMs) {
  if (transport_.ready()) {
    transportWasReady_ = true;
    return;
  }
  if (!transportWasReady_) return;

  firmwareTransfer_.onDisconnected();

  if (activeNodeId_ != 0) registry_.markOffline(activeNodeId_, nowMs);
  if (pendingBindCorrelation_ != 0) bus_.cancelResponse(pendingBindCorrelation_);
  if (pendingStatusCorrelation_ != 0) {
    bus_.cancelResponse(pendingStatusCorrelation_);
  }
  if (pendingControlModeCorrelation_ != 0) {
    bus_.cancelResponse(pendingControlModeCorrelation_);
  }
  if (pendingIndependentSceneCorrelation_ != 0) {
    bus_.cancelResponse(pendingIndependentSceneCorrelation_);
  }
  if (pendingLedCountCorrelation_ != 0) {
    bus_.cancelResponse(pendingLedCountCorrelation_);
  }
  pendingBindCorrelation_ = 0;
  pendingStatusCorrelation_ = 0;
  pendingControlModeCorrelation_ = 0;
  pendingIndependentSceneCorrelation_ = 0;
  pendingLedCountCorrelation_ = 0;
  statusRequestedGeneration_ = 0;
  bindingReady_ = false;
  bindingRequested_ = false;
  statusRequestedGeneration_ = 0;
  activeNodeId_ = 0;
  transportWasReady_ = false;
}

void NodeCoordinator::handleReadyGeneration(const uint32_t nowMs) {
  const uint32_t generation = transport_.readyGeneration();
  if (generation == handledReadyGeneration_) return;
  handledReadyGeneration_ = generation;
  if (firmwareTransfer_.status().state ==
          NodeFirmwareTransferState::Succeeded &&
      generation != firmwareReadyGeneration_) {
    firmwareTransfer_.reset();
    firmwareReadyGeneration_ = 0U;
  }
  bindingRequested_ = false;
  pendingBindCorrelation_ = 0;
  const node::CapabilitiesPayload &capabilities = transport_.capabilities();
  activeNodeId_ = transport_.remoteNodeId();
  bindingReady_ = capabilities.bound;
  if (bindingReady_ || bindingAllowed_) {
    registry_.registerCapabilities(activeNodeId_, capabilities, nowMs);
  }
}

void NodeCoordinator::handleInbound(const uint32_t nowMs) {
  node::Envelope envelope{};
  while (transport_.popInbound(envelope)) {
    if (firmwareTransfer_.handleInbound(envelope)) continue;
    if (bus_.resolveResponse(envelope)) continue;
    if (envelope.messageType == node::MessageType::StatusSnapshot) {
      node::StatusSnapshotPayload status{};
      if (node::readStatusSnapshot(envelope, status) == node::CodecResult::Ok) {
        registry_.updateStatus(envelope.sourceNodeId, status, nowMs);
      }
    }
  }
}

void NodeCoordinator::requestBinding(const uint32_t nowMs) {
  node::BindRequestPayload request{};
  request.coordinatorNodeId = node::kCoordinatorNodeId;
  request.bindingNonce = nowMs ^ transport_.remoteNodeId() ^ 0x534F5A4FU;

  node::Envelope envelope{};
  envelope.messageType = node::MessageType::BindRequest;
  envelope.channelId = static_cast<uint16_t>(node::ServiceId::PairNode);
  envelope.flags = node::kFlagRequiresAck;
  envelope.sourceNodeId = node::kCoordinatorNodeId;
  envelope.targetNodeId = transport_.remoteNodeId();
  envelope.sequence = outboundSequence_++;
  envelope.timestampMs = nowMs;
  envelope.correlationId = correlationSequence_++;
  node::writeBindRequest(envelope, request);

  if (bus_.awaitResponse(envelope.correlationId, nowMs + kBindTimeoutMs,
                         onBindResponse, this) != node::BusResult::Ok) {
    return;
  }
  if (!transport_.send(envelope)) {
    bus_.cancelResponse(envelope.correlationId);
    return;
  }
  pendingBindCorrelation_ = envelope.correlationId;
  bindingRequested_ = true;
}

bool NodeCoordinator::requestStatus(const uint32_t nowMs) {
  if (pendingStatusCorrelation_ != 0) return false;

  node::Envelope envelope{};
  envelope.channelId = static_cast<uint16_t>(node::ServiceId::GetNodeState);
  envelope.flags = node::kFlagRequiresAck;
  envelope.sourceNodeId = node::kCoordinatorNodeId;
  envelope.targetNodeId = transport_.remoteNodeId();
  envelope.sequence = outboundSequence_++;
  envelope.timestampMs = nowMs;
  envelope.correlationId = correlationSequence_++;
  if (node::writeStatusRequest(envelope) != node::CodecResult::Ok) {
    return false;
  }
  if (bus_.awaitResponse(envelope.correlationId,
                         nowMs + kStatusRequestTimeoutMs, onStatusResponse,
                         this) != node::BusResult::Ok) {
    return false;
  }
  if (!transport_.send(envelope)) {
    bus_.cancelResponse(envelope.correlationId);
    return false;
  }
  pendingStatusCorrelation_ = envelope.correlationId;
  statusRequestedGeneration_ = transport_.readyGeneration();
  return true;
}

bool NodeCoordinator::isAvailableLightNode(const node::NodeId nodeId) const {
  if (!transport_.ready() || !bindingReady_ || nodeId == 0 ||
      nodeId != activeNodeId_) {
    return false;
  }
  const NodeRecord *record = registry_.find(nodeId);
  return record != nullptr &&
         (record->capabilities.capabilityBits &
          node::capabilityMask(node::Capability::LightOutput)) != 0U;
}

void NodeCoordinator::sendAudio(const uint32_t nowMs, const EffectMode mode,
                                const AudioFrame &audioFrame) {
  if (!isAudioEffect(mode) ||
      static_cast<uint32_t>(nowMs - lastAudioSentMs_) < kAudioIntervalMs) {
    return;
  }
  lastAudioSentMs_ = nowMs;
  node::Envelope envelope{};
  envelope.messageType = node::MessageType::AudioFeatures;
  envelope.channelId =
      static_cast<uint16_t>(node::TopicId::SpaceAudioFeatures);
  envelope.sourceNodeId = node::kCoordinatorNodeId;
  envelope.targetNodeId = transport_.remoteNodeId();
  envelope.sequence = outboundSequence_++;
  envelope.timestampMs = nowMs;
  envelope.sceneRevision = 0U;
  node::writeAudioFeatures(envelope, makeAudioFeatures(audioFrame));
  transport_.send(envelope);
}

bool NodeCoordinator::isAudioEffect(const EffectMode mode) const {
  return mode == EffectMode::Music || mode == EffectMode::FlameAudio ||
         mode == EffectMode::BassRipple;
}

}  // namespace sozo
