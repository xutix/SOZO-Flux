#include <C3NodeApplication.h>
#include <SozoVersion.h>

namespace sozo::c3 {
namespace {

uint16_t sceneErrorCode(const SceneApplyResult result) {
  switch (result) {
    case SceneApplyResult::Invalid:
      return 1;
    case SceneApplyResult::Stale:
      return 2;
    case SceneApplyResult::Applied:
    case SceneApplyResult::Duplicate:
    default:
      return 0;
  }
}

bool acceptsScene(const SceneApplyResult result) {
  return result == SceneApplyResult::Applied ||
         result == SceneApplyResult::Duplicate;
}

}  // namespace

C3NodeApplication::C3NodeApplication(
    NodeLightingSink &lighting, NodeButtonInput &button,
    NodeDiagnosticsPort &diagnostics, PairingWindow &pairingWindow,
    NodeTransportPort &transport, NodeBindingRepository &bindings,
    NodeControlRepository &controls, NodeLedCountRepository &ledCounts,
    const C3NodeProfile profile, NodeFirmwareReceiver *firmwareReceiver)
    : lighting_(lighting),
      button_(button),
      diagnostics_(diagnostics),
      pairingWindow_(pairingWindow),
      transport_(transport),
      bindings_(bindings),
      controls_(controls),
      ledCounts_(ledCounts),
      firmwareReceiver_(firmwareReceiver),
      profile_(profile),
      sceneRuntime_(lighting) {}

bool C3NodeApplication::begin(const node::NodeId nodeId,
                              const uint32_t nowMs) {
  if (initialized_ || nodeId == 0 || nodeId == node::kCoordinatorNodeId ||
      nodeId == node::kBroadcastNodeId || profile_.defaultLedCount == 0 ||
      profile_.defaultLedCount > profile_.maxLedCount) {
    return false;
  }

  nodeId_ = nodeId;
  pairingWindow_.begin(button_.pressed(), nowMs);
  ledCountState_ = ledCounts_.load();
  if (ledCountState_.schemaVersion != NodeLedCountState::kSchemaVersion ||
      ledCountState_.ledCount == 0U ||
      ledCountState_.ledCount > profile_.maxLedCount) {
    ledCountState_.ledCount = profile_.defaultLedCount;
  }
  lighting_.begin(makeLocalState(ledCountState_.ledCount));
  sceneRuntime_.restoreControlState(controls_.load());
  binding_ = bindings_.load();
  if (!transport_.begin(nodeId_, makeCapabilities(), binding_.bound,
                        binding_.coordinatorNodeId,
                        binding_.bleIdentityAddress,
                        binding_.bleIdentityAddressType)) {
    return false;
  }
  transportConnected_ = transport_.connected();
  initialized_ = true;
  return true;
}

C3NodeEvent C3NodeApplication::tick(const uint32_t nowMs) {
  if (!initialized_) return C3NodeEvent::None;

  const bool connected = transport_.connected();
  if (transportConnected_ && !connected) {
    sceneRuntime_.onDisconnected();
    if (firmwareReceiver_ != nullptr) firmwareReceiver_->onDisconnected();
  }
  transportConnected_ = connected;

  const PairingEvent pairingEvent = pairingWindow_.tick(button_.pressed(), nowMs);
  transport_.handlePairingEvent(pairingEvent);
  if (pairingEvent == PairingEvent::ClearBindingRequested) {
    bindings_.clear();
    controls_.clear();
    binding_ = NodeBinding{};
    transport_.clearBinding();
    return C3NodeEvent::RestartRequested;
  }

  node::Envelope inbound{};
  while (transport_.popInbound(inbound)) handleInbound(inbound, nowMs);
  if (firmwareReceiver_ != nullptr) {
    firmwareReceiver_->tick(nowMs);
    if (firmwareReceiver_->restartRequested()) {
      return C3NodeEvent::FirmwareRestartRequested;
    }
  }
  sceneRuntime_.tick(nowMs);

  if (pairingEvent == PairingEvent::Opened) return C3NodeEvent::PairingOpened;
  if (pairingEvent == PairingEvent::Closed) return C3NodeEvent::PairingClosed;
  return C3NodeEvent::None;
}

node::NodeId C3NodeApplication::nodeId() const { return nodeId_; }

const NodeBinding &C3NodeApplication::binding() const { return binding_; }

uint32_t C3NodeApplication::lastAppliedSceneRevision() const {
  return sceneRuntime_.lastAppliedSceneRevision();
}

PersistedLightingState C3NodeApplication::makeLocalState(
    const uint16_t ledCount) const {
  PersistedLightingState state = makeDefaultPersistedLightingState();
  state.layout.profile = spatial_light::LayoutProfile::Continuous;
  state.layout.activeCount = ledCount;
  state.layout.centerIndex =
      static_cast<uint16_t>((ledCount - 1U) / 2U);
  state.layout.leftCount = 0;
  state.layout.centerCount = ledCount;
  state.layout.rightCount = 0;
  state.layout.reversed = false;
  return state;
}

node::CapabilitiesPayload C3NodeApplication::makeCapabilities() const {
  node::CapabilitiesPayload capabilities{};
  capabilities.capabilityBits = node::capabilityMask(node::Capability::LightOutput);
  if (firmwareReceiver_ != nullptr) {
    capabilities.capabilityBits |=
        node::capabilityMask(node::Capability::FirmwareUpdate);
  }
  capabilities.maxLedCount = profile_.maxLedCount;
  capabilities.maxPacketBytes = node::kMaxPacketBytes;
  capabilities.protocolMin = node::kProtocolVersion;
  capabilities.protocolMax = node::kProtocolVersion;
  capabilities.firmwareMajor = version::kNodeC3Major;
  capabilities.firmwareMinor = version::kNodeC3Minor;
  capabilities.firmwarePatch = version::kNodeC3Patch;
  capabilities.controlMode = sceneRuntime_.controlMode();
  capabilities.hardwareProfile = profile_.hardwareProfile;
  capabilities.bound = binding_.bound;
  return capabilities;
}

void C3NodeApplication::handleInbound(const node::Envelope &envelope,
                                      const uint32_t nowMs) {
  if (firmwareReceiver_ != nullptr) {
    if (envelope.messageType == node::MessageType::FirmwareBegin) {
      node::FirmwareBeginPayload payload{};
      if (node::readFirmwareBegin(envelope, payload) == node::CodecResult::Ok) {
        firmwareReceiver_->begin(payload, nowMs);
      }
      sendFirmwareStatus(envelope, nowMs);
      return;
    }
    if (envelope.messageType == node::MessageType::FirmwareChunk) {
      node::FirmwareChunkPayload payload{};
      if (node::readFirmwareChunk(envelope, payload) == node::CodecResult::Ok) {
        firmwareReceiver_->append(payload, nowMs);
      }
      sendFirmwareStatus(envelope, nowMs);
      return;
    }
    if (envelope.messageType == node::MessageType::FirmwareEnd) {
      if (node::readFirmwareEnd(envelope) == node::CodecResult::Ok) {
        firmwareReceiver_->finish(nowMs);
      }
      sendFirmwareStatus(envelope, nowMs);
      return;
    }
    if (firmwareReceiver_->active()) return;
  }

  switch (envelope.messageType) {
    case node::MessageType::BindRequest:
      handleBindRequest(envelope, nowMs);
      break;
    case node::MessageType::SceneSnapshot: {
      node::SceneSnapshotPayload scene{};
      SceneApplyResult result = SceneApplyResult::Invalid;
      const SceneTarget target =
          envelope.channelId ==
                  static_cast<uint16_t>(node::TopicId::NodeIndependentScene)
              ? SceneTarget::Independent
              : SceneTarget::FollowMain;
      if (envelope.channelId == static_cast<uint16_t>(node::TopicId::SpaceScene) ||
          target == SceneTarget::Independent) {
        if (node::readSceneSnapshot(envelope, scene) == node::CodecResult::Ok) {
          const NodeControlState before = sceneRuntime_.controlState();
          result = sceneRuntime_.applyScene(scene, envelope.sceneRevision,
                                            envelope.timestampMs, nowMs,
                                            target);
          if (target == SceneTarget::Independent && acceptsScene(result) &&
              !controls_.save(sceneRuntime_.controlState())) {
            sceneRuntime_.restoreControlState(before);
            result = SceneApplyResult::Invalid;
          }
        }
      }
      sendSceneReceipt(envelope, result, nowMs);
      break;
    }
    case node::MessageType::ControlModeRequest: {
      node::ControlModePayload payload{};
      const NodeControlState before = sceneRuntime_.controlState();
      bool accepted =
          node::readControlModeRequest(envelope, payload) == node::CodecResult::Ok &&
          sceneRuntime_.setControlMode(payload.controlMode);
      uint16_t errorCode = accepted ? 0U : 4U;
      if (accepted && !controls_.save(sceneRuntime_.controlState())) {
        sceneRuntime_.restoreControlState(before);
        accepted = false;
        errorCode = 5U;
      }
      sendCommandReceipt(envelope, accepted, errorCode, nowMs);
      break;
    }
    case node::MessageType::LedCountRequest: {
      node::LedCountPayload payload{};
      bool accepted =
          node::readLedCountRequest(envelope, payload) == node::CodecResult::Ok &&
          payload.ledCount <= profile_.maxLedCount;
      uint16_t errorCode = accepted ? 0U : 6U;
      if (accepted) {
        const NodeLedCountState previous = ledCountState_;
        const NodeLedCountState next{NodeLedCountState::kSchemaVersion,
                                     payload.ledCount};
        if (!sceneRuntime_.setLocalLedCount(next.ledCount)) {
          accepted = false;
          errorCode = 6U;
        } else if (!ledCounts_.save(next)) {
          sceneRuntime_.setLocalLedCount(previous.ledCount);
          accepted = false;
          errorCode = 7U;
        } else {
          ledCountState_ = next;
        }
      }
      sendCommandReceipt(envelope, accepted, errorCode, nowMs);
      break;
    }
    case node::MessageType::AudioFeatures: {
      node::AudioFeaturesPayload audio{};
      if (node::readAudioFeatures(envelope, audio) == node::CodecResult::Ok) {
        sceneRuntime_.applyAudioFeatures(audio, envelope.sequence);
      }
      break;
    }
    case node::MessageType::StatusRequest:
      if (node::readStatusRequest(envelope) == node::CodecResult::Ok) {
        sendStatusSnapshot(envelope, nowMs);
      }
      break;
    default:
      break;
  }
}

void C3NodeApplication::handleBindRequest(const node::Envelope &envelope,
                                          const uint32_t nowMs) {
  node::BindRequestPayload request{};
  if (!pairingWindow_.isOpen() || binding_.bound ||
      node::readBindRequest(envelope, request) != node::CodecResult::Ok ||
      request.coordinatorNodeId != envelope.sourceNodeId) {
    sendBindResult(envelope, false, 1, nowMs);
    return;
  }

  const uint64_t peerIdentityAddress = transport_.connectedPeerIdentityAddress();
  const uint8_t peerIdentityAddressType =
      transport_.connectedPeerIdentityAddressType();
  if (!bindings_.save(request.coordinatorNodeId, peerIdentityAddress,
                      peerIdentityAddressType)) {
    sendBindResult(envelope, false, 2, nowMs);
    return;
  }
  if (!transport_.bindCoordinator(request.coordinatorNodeId)) {
    bindings_.clear();
    sendBindResult(envelope, false, 3, nowMs);
    return;
  }
  binding_.bound = true;
  binding_.coordinatorNodeId = request.coordinatorNodeId;
  binding_.bleIdentityAddress = peerIdentityAddress;
  binding_.bleIdentityAddressType = peerIdentityAddressType;
  sendBindResult(envelope, true, 0, nowMs);
}

void C3NodeApplication::sendBindResult(const node::Envelope &request,
                                       const bool accepted,
                                       const uint16_t errorCode,
                                       const uint32_t nowMs) {
  node::BindResultPayload result{};
  result.accepted = accepted;
  result.nodeId = nodeId_;
  result.errorCode = errorCode;
  node::BindRequestPayload bindRequest{};
  if (node::readBindRequest(request, bindRequest) == node::CodecResult::Ok) {
    result.bindingNonce = bindRequest.bindingNonce;
  }

  node::Envelope response{};
  response.messageType = node::MessageType::BindResult;
  response.channelId = request.channelId;
  response.flags = node::kFlagIsResponse |
                   (accepted ? 0U : node::kFlagIsError);
  response.sourceNodeId = nodeId_;
  response.targetNodeId = request.sourceNodeId;
  response.sequence = outboundSequence_++;
  response.timestampMs = nowMs;
  response.correlationId = request.correlationId;
  node::writeBindResult(response, result);
  transport_.send(response);
}

void C3NodeApplication::sendSceneReceipt(const node::Envelope &request,
                                         const SceneApplyResult result,
                                         const uint32_t nowMs) {
  if ((request.flags & node::kFlagRequiresAck) == 0U) return;

  node::CommandReceiptPayload receipt{};
  receipt.accepted = acceptsScene(result);
  receipt.lastAppliedSceneRevision =
      acceptsScene(result) ? request.sceneRevision
                           : sceneRuntime_.lastAppliedSceneRevision();
  receipt.errorCode = sceneErrorCode(result);

  node::Envelope response{};
  response.channelId = request.channelId;
  response.sourceNodeId = nodeId_;
  response.targetNodeId = request.sourceNodeId;
  response.sequence = outboundSequence_++;
  response.timestampMs = nowMs;
  response.sceneRevision = request.sceneRevision;
  response.correlationId = request.correlationId;
  node::writeCommandReceipt(response, receipt);
  transport_.send(response);
}

void C3NodeApplication::sendCommandReceipt(
    const node::Envelope &request, const bool accepted, const uint16_t errorCode,
    const uint32_t nowMs) {
  if ((request.flags & node::kFlagRequiresAck) == 0U) return;
  node::CommandReceiptPayload receipt{};
  receipt.accepted = accepted;
  receipt.lastAppliedSceneRevision = sceneRuntime_.lastAppliedSceneRevision();
  receipt.errorCode = errorCode;
  node::Envelope response{};
  response.channelId = request.channelId;
  response.sourceNodeId = nodeId_;
  response.targetNodeId = request.sourceNodeId;
  response.sequence = outboundSequence_++;
  response.timestampMs = nowMs;
  response.correlationId = request.correlationId;
  node::writeCommandReceipt(response, receipt);
  transport_.send(response);
}

void C3NodeApplication::sendStatusSnapshot(const node::Envelope &request,
                                           const uint32_t nowMs) {
  if ((request.flags & node::kFlagRequiresAck) == 0U) return;

  node::StatusSnapshotPayload status{};
  status.lastAppliedSceneRevision = sceneRuntime_.lastAppliedSceneRevision();
  status.freeHeapBytes = diagnostics_.freeHeapBytes();
  status.controlMode = sceneRuntime_.controlMode();
  status.pairingWindowOpen = pairingWindow_.isOpen();
  status.ledCount = ledCountState_.ledCount;

  node::Envelope response{};
  response.channelId = request.channelId;
  response.sourceNodeId = nodeId_;
  response.targetNodeId = request.sourceNodeId;
  response.sequence = outboundSequence_++;
  response.timestampMs = nowMs;
  response.correlationId = request.correlationId;
  node::writeStatusSnapshot(response, status);
  transport_.send(response);
}

void C3NodeApplication::sendFirmwareStatus(const node::Envelope &request,
                                           const uint32_t nowMs) {
  if (firmwareReceiver_ == nullptr) return;
  node::Envelope response{};
  response.channelId = static_cast<uint16_t>(node::TopicId::NodeState);
  response.sourceNodeId = nodeId_;
  response.targetNodeId = request.sourceNodeId;
  response.sequence = outboundSequence_++;
  response.timestampMs = nowMs;
  response.correlationId = request.correlationId;
  if (node::writeFirmwareStatus(response, firmwareReceiver_->status()) ==
      node::CodecResult::Ok) {
    transport_.send(response);
  }
}

}  // namespace sozo::c3
