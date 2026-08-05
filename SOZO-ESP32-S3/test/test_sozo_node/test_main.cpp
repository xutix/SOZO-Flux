#include <SceneMessageMapper.h>
#include <NodeRegistry.h>
#include <NodeCoordinator.h>
#include <NodeTransport.h>
#include <BleLinkStateMachine.h>
#include <BleOperationSupervisor.h>
#include <BleOutboundMailbox.h>
#include <TestHarness.h>

namespace {

class FakeNodeTransport final : public sozo::NodeTransport {
 public:
  bool begin() override {
    began = true;
    return true;
  }
  void tick(uint32_t) override {}
  bool send(const sozo::node::Envelope &envelope) override {
    if (sentCount >= 8) return false;
    sent[sentCount++] = envelope;
    return true;
  }
  bool popInbound(sozo::node::Envelope &envelope) override {
    if (!hasInbound) return false;
    envelope = inbound;
    hasInbound = false;
    return true;
  }
  bool ready() const override { return isReady; }
  sozo::NodeTransportState state() const override {
    return isReady ? sozo::NodeTransportState::Ready
                   : sozo::NodeTransportState::Searching;
  }
  sozo::node::NodeId remoteNodeId() const override { return remoteId; }
  const sozo::node::CapabilitiesPayload &capabilities() const override {
    return remoteCapabilities;
  }
  uint32_t readyGeneration() const override { return generation; }
  uint32_t droppedPackets() const override { return 0; }
  const char *operationName() const override { return "idle"; }
  bool workerBusy() const override { return false; }
  uint32_t timeoutCount() const override { return 0; }

  void queueInbound(const sozo::node::Envelope &envelope) {
    inbound = envelope;
    hasInbound = true;
  }

  bool began{false};
  bool isReady{true};
  bool hasInbound{false};
  sozo::node::NodeId remoteId{0xC3000042U};
  sozo::node::CapabilitiesPayload remoteCapabilities{};
  uint32_t generation{1};
  sozo::node::Envelope inbound{};
  sozo::node::Envelope sent[8]{};
  size_t sentCount{0};
};

void test_maps_every_runtime_effect_parameter() {
  sozo::PersistedLightingState state{};
  state.mode = sozo::EffectMode::BassRipple;
  state.brightness = 201;
  state.primaryColor = {1, 2, 3};
  state.audioColorStyle = 4;
  state.cometColorStyle = 3;
  state.lighting.rainbowStyle = 2;
  state.lighting.flowSpeed = 91;
  state.lighting.cometTail = 72;
  state.lighting.cometSpeed = 49;
  state.lighting.cometDensity = 8;
  state.lighting.cometBackground = 55;
  state.lighting.cometRandom = true;
  state.lighting.audioSensitivityX100 = 499;
  state.lighting.audioColorGainX100 = 401;
  state.lighting.audioHueDrive = 3;
  state.lighting.breathFloorPercent = 59;
  state.lighting.secondaryRed = 4;
  state.lighting.secondaryGreen = 5;
  state.lighting.secondaryBlue = 6;
  state.lighting.pulseAmplitudePercent = 99;
  state.lighting.pulseHeightPercent = 98;
  state.lighting.animationBrightness = 233;
  state.layout.profile = spatial_light::LayoutProfile::Segmented;
  state.layout.activeCount = 500;
  state.layout.leftCount = 200;
  state.layout.centerCount = 100;
  state.layout.rightCount = 200;
  state.layout.reversed = true;
  const sozo::LightingSnapshot runtime{
      sozo::EffectMode::BassRipple, 500, false, 321};

  const sozo::node::SceneSnapshotPayload scene =
      sozo::makeSceneSnapshot(state, runtime);

  CHECK_EQ(static_cast<uint8_t>(state.mode), scene.effectMode);
  CHECK_EQ(state.brightness, scene.brightness);
  CHECK_EQ(state.primaryColor.red, scene.primaryRed);
  CHECK_EQ(state.primaryColor.green, scene.primaryGreen);
  CHECK_EQ(state.primaryColor.blue, scene.primaryBlue);
  CHECK_EQ(state.lighting.rainbowStyle, scene.rainbowStyle);
  CHECK_EQ(state.lighting.flowSpeed, scene.flowSpeed);
  CHECK_EQ(state.lighting.cometTail, scene.cometTail);
  CHECK_EQ(state.lighting.cometSpeed, scene.cometSpeed);
  CHECK_EQ(state.lighting.cometDensity, scene.cometDensity);
  CHECK_EQ(state.lighting.cometBackground, scene.cometBackground);
  CHECK_EQ(state.lighting.cometRandom, scene.cometRandom);
  CHECK_EQ(state.lighting.audioSensitivityX100,
           scene.audioSensitivityX100);
  CHECK_EQ(state.lighting.audioColorGainX100, scene.audioColorGainX100);
  CHECK_EQ(state.lighting.audioHueDrive, scene.audioHueDrive);
  CHECK_EQ(state.lighting.breathFloorPercent, scene.breathFloorPercent);
  CHECK_EQ(state.lighting.secondaryRed, scene.secondaryRed);
  CHECK_EQ(state.lighting.secondaryGreen, scene.secondaryGreen);
  CHECK_EQ(state.lighting.secondaryBlue, scene.secondaryBlue);
  CHECK_EQ(state.lighting.pulseAmplitudePercent,
           scene.pulseAmplitudePercent);
  CHECK_EQ(state.lighting.pulseHeightPercent, scene.pulseHeightPercent);
  CHECK_EQ(state.lighting.animationBrightness, scene.animationBrightness);
  CHECK_EQ(state.audioColorStyle, scene.audioColorStyle);
  CHECK_EQ(state.cometColorStyle, scene.cometColorStyle);
  CHECK_EQ(runtime.manualLitPixelCount, scene.manualLitPixelCount);
  CHECK_EQ(static_cast<uint8_t>(state.layout.profile), scene.spatialProfile);
  CHECK_EQ(sozo::node::kSpatialFlagReversed, scene.spatialFlags);
}

void test_scene_does_not_grow_with_physical_led_count() {
  sozo::PersistedLightingState shortStrip{};
  sozo::PersistedLightingState longStrip{};
  shortStrip.layout.activeCount = 30;
  longStrip.layout.activeCount = 500;
  const sozo::LightingSnapshot runtime{sozo::EffectMode::Static, 30, false,
                                       -1};

  const auto shortScene = sozo::makeSceneSnapshot(shortStrip, runtime);
  const auto longScene = sozo::makeSceneSnapshot(longStrip, runtime);
  CHECK_EQ(sizeof(shortScene), sizeof(longScene));
  CHECK_EQ(sozo::node::kSceneSnapshotWireBytes, 30);
}

void test_audio_features_are_clamped_and_scaled() {
  const sozo::AudioFrame input{-2.0F, 999999.0F, 123.45F, 999.0F,
                               42.75F, 77U, true};
  const sozo::node::AudioFeaturesPayload output =
      sozo::makeAudioFeatures(input);
  CHECK_EQ(0U, output.volumeX100);
  CHECK_EQ(12345U, output.fastEnergyX100);
  CHECK_EQ(65535U, output.slowEnergyX100);
  CHECK_EQ(4275U, output.beatPulseX100);
  CHECK_EQ(77U, output.framesRead);
  CHECK_TRUE(output.available);
}

void test_coordinator_streams_audio_for_every_audio_effect() {
  const sozo::EffectMode audioModes[] = {
      sozo::EffectMode::Music,
      sozo::EffectMode::FlameAudio,
      sozo::EffectMode::BassRipple,
  };
  const sozo::AudioFrame audio{75.0F, 0.0F, 50.0F, 40.0F,
                               30.0F, 12U, true};

  for (const sozo::EffectMode mode : audioModes) {
    FakeNodeTransport transport;
    transport.remoteCapabilities.bound = true;
    transport.remoteCapabilities.capabilityBits = sozo::node::capabilityMask(
        sozo::node::Capability::LightOutput);
    sozo::NodeCoordinator coordinator(transport);
    CHECK_TRUE(coordinator.begin());

    sozo::PersistedLightingState state =
        sozo::makeDefaultPersistedLightingState();
    state.mode = mode;
    const sozo::LightingSnapshot runtime{mode, state.layout.activeCount, false,
                                         -1};
    coordinator.tick(100U, state, runtime, audio);

    CHECK_EQ(2U, transport.sentCount);
    CHECK_EQ(sozo::node::MessageType::SceneSnapshot,
             transport.sent[0].messageType);
    CHECK_EQ(sozo::node::MessageType::AudioFeatures,
             transport.sent[1].messageType);
    CHECK_EQ(static_cast<uint16_t>(sozo::node::TopicId::SpaceAudioFeatures),
             transport.sent[1].channelId);

    sozo::node::AudioFeaturesPayload decoded{};
    CHECK_EQ(sozo::node::CodecResult::Ok,
             sozo::node::readAudioFeatures(transport.sent[1], decoded));
    CHECK_EQ(7500U, decoded.volumeX100);
    CHECK_EQ(5000U, decoded.fastEnergyX100);
    CHECK_EQ(4000U, decoded.slowEnergyX100);
    CHECK_EQ(3000U, decoded.beatPulseX100);
    CHECK_TRUE(decoded.available);
  }

  FakeNodeTransport transport;
  transport.remoteCapabilities.bound = true;
  sozo::NodeCoordinator coordinator(transport);
  CHECK_TRUE(coordinator.begin());
  sozo::PersistedLightingState state =
      sozo::makeDefaultPersistedLightingState();
  state.mode = sozo::EffectMode::Rainbow;
  const sozo::LightingSnapshot runtime{state.mode, state.layout.activeCount,
                                       false, -1};
  coordinator.tick(100U, state, runtime, audio);
  CHECK_EQ(1U, transport.sentCount);
  CHECK_EQ(sozo::node::MessageType::SceneSnapshot,
           transport.sent[0].messageType);
}

void test_scene_equality_compares_render_intent() {
  sozo::PersistedLightingState state{};
  const sozo::LightingSnapshot runtime{state.mode, state.layout.activeCount,
                                       false, -1};
  auto first = sozo::makeSceneSnapshot(state, runtime);
  auto second = first;
  CHECK_TRUE(sozo::sameSceneSnapshot(first, second));
  second.flowSpeed++;
  CHECK_TRUE(!sozo::sameSceneSnapshot(first, second));
}

void test_registry_tracks_link_receipts_and_status_independently() {
  sozo::NodeRegistry registry;
  sozo::node::CapabilitiesPayload capabilities{};
  capabilities.capabilityBits = sozo::node::capabilityMask(
      sozo::node::Capability::LightOutput);
  capabilities.maxLedCount = 300;
  capabilities.protocolMin = sozo::node::kProtocolVersion;
  capabilities.protocolMax = sozo::node::kProtocolVersion;

  CHECK_EQ(sozo::NodeRegistrationResult::Added,
           registry.registerCapabilities(100U, capabilities, 1000U));
  CHECK_EQ(1U, registry.size());
  CHECK_TRUE(registry.recordAt(0) != nullptr);
  CHECK_TRUE(registry.recordAt(sozo::NodeRegistry::kCapacity) == nullptr);
  const sozo::NodeRecord *record = registry.find(100U);
  CHECK_TRUE(record != nullptr);
  CHECK_EQ(sozo::NodeConnectionState::Ready, record->connectionState);
  CHECK_EQ(300U, record->capabilities.maxLedCount);

  capabilities.maxLedCount = 480;
  CHECK_EQ(sozo::NodeRegistrationResult::Updated,
           registry.registerCapabilities(100U, capabilities, 1100U));
  CHECK_EQ(1U, registry.size());
  CHECK_EQ(480U, registry.find(100U)->capabilities.maxLedCount);

  sozo::node::CommandReceiptPayload receipt{};
  receipt.accepted = true;
  receipt.lastAppliedSceneRevision = 8U;
  CHECK_TRUE(registry.recordCommandReceipt(100U, receipt, 1200U));
  record = registry.find(100U);
  CHECK_EQ(1200U, record->lastReceiptMs);
  CHECK_EQ(8U, record->lastAppliedSceneRevision);

  sozo::node::StatusSnapshotPayload status{};
  status.lastAppliedSceneRevision = 9U;
  status.freeHeapBytes = 50000U;
  status.ledCount = 62U;
  CHECK_TRUE(registry.updateStatus(100U, status, 1300U));
  record = registry.find(100U);
  CHECK_EQ(1300U, record->lastStatusMs);
  CHECK_EQ(9U, record->lastAppliedSceneRevision);
  CHECK_EQ(62U, record->status.ledCount);

  CHECK_TRUE(registry.markOffline(100U, 2200U));
  CHECK_EQ(sozo::NodeConnectionState::Offline,
           registry.find(100U)->connectionState);
}

void test_registry_has_fixed_eight_node_capacity() {
  sozo::NodeRegistry registry;
  sozo::node::CapabilitiesPayload capabilities{};
  for (uint32_t index = 0; index < sozo::NodeRegistry::kCapacity; ++index) {
    CHECK_EQ(sozo::NodeRegistrationResult::Added,
             registry.registerCapabilities(100U + index, capabilities,
                                           index));
  }
  CHECK_EQ(sozo::NodeRegistry::kCapacity, registry.size());
  CHECK_EQ(sozo::NodeRegistrationResult::Full,
           registry.registerCapabilities(999U, capabilities, 99U));
  CHECK_TRUE(!registry.recordCommandReceipt(999U, {}, 100U));
  CHECK_TRUE(!registry.updateStatus(999U, {}, 100U));
  CHECK_TRUE(!registry.markOffline(999U, 100U));
}

void test_ble_link_state_machine_reaches_ready_in_order() {
  sozo::BleLinkStateMachine link;
  CHECK_EQ(sozo::BleLinkState::Idle, link.state());
  CHECK_TRUE(link.handle(sozo::BleLinkEvent::StartScan));
  CHECK_EQ(sozo::BleLinkState::Scanning, link.state());
  CHECK_TRUE(link.handle(sozo::BleLinkEvent::DeviceFound));
  CHECK_EQ(sozo::BleLinkState::Connecting, link.state());
  CHECK_TRUE(link.handle(sozo::BleLinkEvent::Connected));
  CHECK_EQ(sozo::BleLinkState::Discovering, link.state());
  CHECK_TRUE(link.handle(sozo::BleLinkEvent::ServicesDiscovered));
  CHECK_EQ(sozo::BleLinkState::Authenticating, link.state());
  CHECK_TRUE(link.handle(sozo::BleLinkEvent::Authenticated));
  CHECK_EQ(sozo::BleLinkState::Ready, link.state());
}

void test_ble_link_failure_uses_backoff_before_rescan() {
  sozo::BleLinkStateMachine link;
  link.handle(sozo::BleLinkEvent::StartScan);
  link.handle(sozo::BleLinkEvent::DeviceFound);
  CHECK_TRUE(link.handle(sozo::BleLinkEvent::Failed));
  CHECK_EQ(sozo::BleLinkState::Backoff, link.state());
  CHECK_TRUE(!link.handle(sozo::BleLinkEvent::StartScan));
  CHECK_TRUE(link.handle(sozo::BleLinkEvent::RetryDue));
  CHECK_EQ(sozo::BleLinkState::Scanning, link.state());

  link.handle(sozo::BleLinkEvent::DeviceFound);
  link.handle(sozo::BleLinkEvent::Connected);
  link.handle(sozo::BleLinkEvent::ServicesDiscovered);
  link.handle(sozo::BleLinkEvent::Authenticated);
  CHECK_TRUE(link.handle(sozo::BleLinkEvent::Disconnected));
  CHECK_EQ(sozo::BleLinkState::Backoff, link.state());
}

void test_ble_link_timeout_enters_backoff_without_ready() {
  sozo::BleLinkStateMachine link;
  link.handle(sozo::BleLinkEvent::StartScan);
  link.handle(sozo::BleLinkEvent::DeviceFound);
  CHECK_TRUE(link.handle(sozo::BleLinkEvent::OperationTimedOut));
  CHECK_EQ(sozo::BleLinkState::Backoff, link.state());

  link.reset();
  link.handle(sozo::BleLinkEvent::StartScan);
  link.handle(sozo::BleLinkEvent::DeviceFound);
  link.handle(sozo::BleLinkEvent::Connected);
  link.handle(sozo::BleLinkEvent::ServicesDiscovered);
  link.handle(sozo::BleLinkEvent::Authenticated);
  CHECK_EQ(sozo::BleLinkState::Ready, link.state());
  CHECK_TRUE(link.handle(sozo::BleLinkEvent::OperationTimedOut));
  CHECK_EQ(sozo::BleLinkState::Backoff, link.state());
}

void test_ble_link_reports_when_a_worker_owned_state_needs_recovery() {
  sozo::BleLinkStateMachine link;
  CHECK_TRUE(!link.requiresWorker());
  link.handle(sozo::BleLinkEvent::StartScan);
  CHECK_TRUE(!link.requiresWorker());
  link.handle(sozo::BleLinkEvent::DeviceFound);
  CHECK_TRUE(link.requiresWorker());
  link.handle(sozo::BleLinkEvent::Connected);
  CHECK_TRUE(link.requiresWorker());
  link.handle(sozo::BleLinkEvent::ServicesDiscovered);
  CHECK_TRUE(link.requiresWorker());
  link.handle(sozo::BleLinkEvent::Authenticated);
  CHECK_TRUE(link.requiresWorker());
  link.handle(sozo::BleLinkEvent::Failed);
  CHECK_TRUE(!link.requiresWorker());
}

void test_ble_supervisor_aborts_each_stage_once_at_deadline() {
  sozo::BleOperationSupervisor supervisor;
  supervisor.start(sozo::BleOperationStage::Discovering, 1000U);
  CHECK_EQ(sozo::BleSupervisorAction::None, supervisor.tick(5999U));
  CHECK_EQ(sozo::BleSupervisorAction::Abort, supervisor.tick(6000U));
  CHECK_EQ(sozo::BleSupervisorAction::None, supervisor.tick(6001U));
  CHECK_EQ(1U, supervisor.timeoutCount());
}

void test_ble_supervisor_completion_cancels_deadline() {
  sozo::BleOperationSupervisor supervisor;
  supervisor.start(sozo::BleOperationStage::Authenticating, 10U);
  supervisor.complete();
  CHECK_EQ(sozo::BleSupervisorAction::None, supervisor.tick(100000U));
  CHECK_TRUE(!supervisor.active());
  CHECK_EQ(sozo::BleOperationStage::Idle, supervisor.stage());
}

void test_ble_supervisor_deadline_survives_millis_wraparound() {
  sozo::BleOperationSupervisor supervisor;
  supervisor.start(sozo::BleOperationStage::Connecting, 0xFFFFFC18U);
  CHECK_EQ(sozo::BleSupervisorAction::None, supervisor.tick(4999U));
  CHECK_EQ(sozo::BleSupervisorAction::Abort, supervisor.tick(5000U));
}

void test_ble_attempt_gate_rejects_cancelled_and_stale_worker_events() {
  sozo::BleAttemptGate gate;
  const uint32_t first = gate.beginAttempt();
  CHECK_TRUE(gate.accepts(first));
  gate.cancelCurrent();
  CHECK_TRUE(!gate.accepts(first));

  const uint32_t second = gate.beginAttempt();
  CHECK_TRUE(second != first);
  CHECK_TRUE(!gate.accepts(first));
  CHECK_TRUE(gate.accepts(second));
}

void test_ble_mailbox_keeps_latest_audio_without_overwriting_control() {
  sozo::BleOutboundMailbox mailbox;
  sozo::node::Envelope control{};
  control.messageType = sozo::node::MessageType::SceneSnapshot;
  control.sequence = 1U;
  CHECK_TRUE(mailbox.push(control));

  sozo::node::Envelope audio = control;
  audio.messageType = sozo::node::MessageType::AudioFeatures;
  audio.sequence = 2U;
  CHECK_TRUE(mailbox.push(audio));
  audio.sequence = 3U;
  CHECK_TRUE(mailbox.push(audio));

  sozo::node::Envelope output{};
  CHECK_TRUE(mailbox.pop(output));
  CHECK_EQ(1U, output.sequence);
  CHECK_TRUE(mailbox.pop(output));
  CHECK_EQ(3U, output.sequence);
  CHECK_TRUE(!mailbox.pop(output));
}

void test_ble_mailbox_rejects_ninth_control_without_changing_fifo() {
  sozo::BleOutboundMailbox mailbox;
  sozo::node::Envelope control{};
  control.messageType = sozo::node::MessageType::SceneSnapshot;
  for (uint32_t sequence = 1; sequence <= 8; ++sequence) {
    control.sequence = sequence;
    CHECK_TRUE(mailbox.push(control));
  }
  control.sequence = 9U;
  CHECK_TRUE(!mailbox.push(control));
  CHECK_EQ(8U, mailbox.controlCount());

  sozo::node::Envelope output{};
  for (uint32_t sequence = 1; sequence <= 8; ++sequence) {
    CHECK_TRUE(mailbox.pop(output));
    CHECK_EQ(sequence, output.sequence);
  }
  CHECK_TRUE(!mailbox.pop(output));
}

void test_coordinator_depends_on_transport_contract_not_ble_concrete_type() {
  FakeNodeTransport transport;
  transport.remoteCapabilities.bound = false;
  sozo::NodeCoordinator coordinator(transport);
  CHECK_TRUE(coordinator.begin());
  CHECK_TRUE(transport.began);

  const sozo::PersistedLightingState state =
      sozo::makeDefaultPersistedLightingState();
  const sozo::LightingSnapshot runtime{state.mode, state.layout.activeCount,
                                       false, -1};
  coordinator.tick(1000U, state, runtime, {});
  CHECK_EQ(1U, transport.sentCount);
  CHECK_EQ(sozo::node::MessageType::BindRequest,
           transport.sent[0].messageType);

  sozo::node::BindResultPayload result{};
  result.accepted = true;
  result.nodeId = transport.remoteId;
  sozo::node::BindRequestPayload request{};
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::readBindRequest(transport.sent[0], request));
  result.bindingNonce = request.bindingNonce;
  sozo::node::Envelope response{};
  response.messageType = sozo::node::MessageType::BindResult;
  response.channelId = transport.sent[0].channelId;
  response.flags = sozo::node::kFlagIsResponse;
  response.sourceNodeId = transport.remoteId;
  response.targetNodeId = sozo::node::kCoordinatorNodeId;
  response.correlationId = transport.sent[0].correlationId;
  sozo::node::writeBindResult(response, result);
  transport.queueInbound(response);

  coordinator.tick(1001U, state, runtime, {});
  CHECK_TRUE(coordinator.nodeReady());
  CHECK_EQ(2U, transport.sentCount);
  CHECK_EQ(sozo::node::MessageType::SceneSnapshot,
           transport.sent[1].messageType);
  CHECK_TRUE((transport.sent[1].flags & sozo::node::kFlagRequiresAck) != 0U);
  CHECK_TRUE(transport.sent[1].correlationId != 0U);

  sozo::node::CommandReceiptPayload receipt{};
  receipt.accepted = true;
  receipt.lastAppliedSceneRevision = transport.sent[1].sceneRevision;
  sozo::node::Envelope sceneResponse{};
  sceneResponse.channelId = transport.sent[1].channelId;
  sceneResponse.sourceNodeId = transport.remoteId;
  sceneResponse.targetNodeId = sozo::node::kCoordinatorNodeId;
  sceneResponse.correlationId = transport.sent[1].correlationId;
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::writeCommandReceipt(sceneResponse, receipt));
  transport.queueInbound(sceneResponse);
  coordinator.tick(1002U, state, runtime, {});
  const sozo::NodeRecord *record = coordinator.registry().find(transport.remoteId);
  CHECK_TRUE(record != nullptr);
  CHECK_EQ(transport.sent[1].sceneRevision, record->lastAppliedSceneRevision);
  CHECK_EQ(1002U, record->lastReceiptMs);
  CHECK_EQ(3U, transport.sentCount);
  CHECK_EQ(sozo::node::MessageType::StatusRequest,
           transport.sent[2].messageType);
  CHECK_TRUE(transport.sent[2].correlationId != 0U);

  sozo::node::StatusSnapshotPayload status{};
  status.lastAppliedSceneRevision = transport.sent[1].sceneRevision;
  status.freeHeapBytes = 45678U;
  status.ledCount = 62U;
  sozo::node::Envelope statusResponse{};
  statusResponse.channelId = transport.sent[2].channelId;
  statusResponse.sourceNodeId = transport.remoteId;
  statusResponse.targetNodeId = sozo::node::kCoordinatorNodeId;
  statusResponse.correlationId = transport.sent[2].correlationId;
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::writeStatusSnapshot(statusResponse, status));
  transport.queueInbound(statusResponse);
  coordinator.tick(1003U, state, runtime, {});
  record = coordinator.registry().find(transport.remoteId);
  CHECK_EQ(45678U, record->status.freeHeapBytes);
  CHECK_EQ(1003U, record->lastStatusMs);

  transport.isReady = false;
  coordinator.tick(1004U, state, runtime, {});
  CHECK_EQ(sozo::NodeConnectionState::Offline,
           coordinator.registry().find(transport.remoteId)->connectionState);
}

void test_coordinator_routes_independent_scene_and_mode_to_selected_light_node() {
  FakeNodeTransport transport;
  transport.remoteCapabilities.bound = true;
  transport.remoteCapabilities.capabilityBits = sozo::node::capabilityMask(
      sozo::node::Capability::LightOutput);
  sozo::NodeCoordinator coordinator(transport);
  CHECK_TRUE(coordinator.begin());
  const sozo::PersistedLightingState mainState =
      sozo::makeDefaultPersistedLightingState();
  const sozo::LightingSnapshot runtime{mainState.mode,
                                       mainState.layout.activeCount, false, -1};
  coordinator.tick(100U, mainState, runtime, {});

  CHECK_TRUE(coordinator.requestNodeControlMode(
      transport.remoteId, sozo::node::NodeControlMode::Independent, 101U));
  CHECK_EQ(sozo::node::MessageType::ControlModeRequest, transport.sent[1].messageType);
  CHECK_EQ(static_cast<uint16_t>(sozo::node::ServiceId::SetControlMode),
           transport.sent[1].channelId);

  sozo::node::StatusSnapshotPayload status{};
  status.controlMode = sozo::node::NodeControlMode::Independent;
  sozo::node::Envelope statusResponse{};
  statusResponse.sourceNodeId = transport.remoteId;
  statusResponse.targetNodeId = sozo::node::kCoordinatorNodeId;
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::writeStatusSnapshot(statusResponse, status));
  transport.queueInbound(statusResponse);
  coordinator.tick(102U, mainState, runtime, {});

  auto independent = mainState;
  independent.mode = sozo::EffectMode::Rainbow;
  independent.brightness = 177U;
  CHECK_TRUE(coordinator.requestIndependentScene(transport.remoteId,
                                                 independent, 103U));
  CHECK_EQ(sozo::node::MessageType::SceneSnapshot, transport.sent[2].messageType);
  CHECK_EQ(static_cast<uint16_t>(sozo::node::TopicId::NodeIndependentScene),
           transport.sent[2].channelId);

  CHECK_TRUE(!coordinator.requestNodeControlMode(
      0x11111111U, sozo::node::NodeControlMode::FollowMain, 103U));
}

void test_independent_scene_requires_node_confirmation_of_independent_mode() {
  FakeNodeTransport transport;
  transport.remoteCapabilities.bound = true;
  transport.remoteCapabilities.capabilityBits = sozo::node::capabilityMask(
      sozo::node::Capability::LightOutput);
  sozo::NodeCoordinator coordinator(transport);
  CHECK_TRUE(coordinator.begin());
  const sozo::PersistedLightingState state =
      sozo::makeDefaultPersistedLightingState();
  const sozo::LightingSnapshot runtime{state.mode, state.layout.activeCount,
                                       false, -1};
  coordinator.tick(100U, state, runtime, {});

  CHECK_TRUE(!coordinator.requestIndependentScene(transport.remoteId, state,
                                                   101U));

  sozo::node::StatusSnapshotPayload status{};
  status.controlMode = sozo::node::NodeControlMode::Independent;
  sozo::node::Envelope statusResponse{};
  statusResponse.sourceNodeId = transport.remoteId;
  statusResponse.targetNodeId = sozo::node::kCoordinatorNodeId;
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::writeStatusSnapshot(statusResponse, status));
  transport.queueInbound(statusResponse);
  coordinator.tick(102U, state, runtime, {});

  CHECK_TRUE(
      coordinator.requestIndependentScene(transport.remoteId, state, 103U));
}

void test_coordinator_forwards_led_count_and_refreshes_confirmed_value() {
  FakeNodeTransport transport;
  transport.remoteCapabilities.bound = true;
  transport.remoteCapabilities.capabilityBits = sozo::node::capabilityMask(
      sozo::node::Capability::LightOutput);
  transport.remoteCapabilities.maxLedCount = 512U;
  sozo::NodeCoordinator coordinator(transport);
  CHECK_TRUE(coordinator.begin());
  const sozo::PersistedLightingState state =
      sozo::makeDefaultPersistedLightingState();
  const sozo::LightingSnapshot runtime{state.mode, state.layout.activeCount,
                                       false, -1};

  coordinator.tick(100U, state, runtime, {});
  CHECK_EQ(sozo::node::MessageType::SceneSnapshot, transport.sent[0].messageType);

  sozo::node::CommandReceiptPayload sceneReceipt{};
  sceneReceipt.accepted = true;
  sceneReceipt.lastAppliedSceneRevision = transport.sent[0].sceneRevision;
  sozo::node::Envelope sceneResponse{};
  sceneResponse.sourceNodeId = transport.remoteId;
  sceneResponse.targetNodeId = sozo::node::kCoordinatorNodeId;
  sceneResponse.correlationId = transport.sent[0].correlationId;
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::writeCommandReceipt(sceneResponse, sceneReceipt));
  transport.queueInbound(sceneResponse);
  coordinator.tick(101U, state, runtime, {});

  sozo::node::StatusSnapshotPayload initialStatus{};
  initialStatus.ledCount = 60U;
  sozo::node::Envelope initialStatusResponse{};
  initialStatusResponse.sourceNodeId = transport.remoteId;
  initialStatusResponse.targetNodeId = sozo::node::kCoordinatorNodeId;
  initialStatusResponse.correlationId = transport.sent[1].correlationId;
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::writeStatusSnapshot(initialStatusResponse,
                                            initialStatus));
  transport.queueInbound(initialStatusResponse);
  coordinator.tick(102U, state, runtime, {});

  CHECK_TRUE(coordinator.requestNodeLedCount(transport.remoteId, 58U, 103U));
  CHECK_EQ(sozo::node::MessageType::LedCountRequest,
           transport.sent[2].messageType);
  CHECK_EQ(static_cast<uint16_t>(sozo::node::ServiceId::SetLedCount),
           transport.sent[2].channelId);
  sozo::node::LedCountPayload request{};
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::readLedCountRequest(transport.sent[2], request));
  CHECK_EQ(58U, request.ledCount);

  sozo::node::CommandReceiptPayload countReceipt{};
  countReceipt.accepted = true;
  sozo::node::Envelope countResponse{};
  countResponse.sourceNodeId = transport.remoteId;
  countResponse.targetNodeId = sozo::node::kCoordinatorNodeId;
  countResponse.correlationId = transport.sent[2].correlationId;
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::writeCommandReceipt(countResponse, countReceipt));
  transport.queueInbound(countResponse);
  coordinator.tick(104U, state, runtime, {});
  CHECK_EQ(sozo::node::MessageType::StatusRequest,
           transport.sent[3].messageType);

  sozo::node::StatusSnapshotPayload updatedStatus{};
  updatedStatus.ledCount = 58U;
  sozo::node::Envelope updatedStatusResponse{};
  updatedStatusResponse.sourceNodeId = transport.remoteId;
  updatedStatusResponse.targetNodeId = sozo::node::kCoordinatorNodeId;
  updatedStatusResponse.correlationId = transport.sent[3].correlationId;
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::writeStatusSnapshot(updatedStatusResponse,
                                            updatedStatus));
  transport.queueInbound(updatedStatusResponse);
  coordinator.tick(105U, state, runtime, {});
  const sozo::NodeRecord *record = coordinator.registry().find(transport.remoteId);
  CHECK_TRUE(record != nullptr);
  CHECK_EQ(58U, record->status.ledCount);
}

}  // namespace

int main(int, char **) {
  test_maps_every_runtime_effect_parameter();
  test_scene_does_not_grow_with_physical_led_count();
  test_audio_features_are_clamped_and_scaled();
  test_coordinator_streams_audio_for_every_audio_effect();
  test_scene_equality_compares_render_intent();
  test_registry_tracks_link_receipts_and_status_independently();
  test_registry_has_fixed_eight_node_capacity();
  test_ble_link_state_machine_reaches_ready_in_order();
  test_ble_link_failure_uses_backoff_before_rescan();
  test_ble_link_timeout_enters_backoff_without_ready();
  test_ble_link_reports_when_a_worker_owned_state_needs_recovery();
  test_ble_supervisor_aborts_each_stage_once_at_deadline();
  test_ble_supervisor_completion_cancels_deadline();
  test_ble_supervisor_deadline_survives_millis_wraparound();
  test_ble_attempt_gate_rejects_cancelled_and_stale_worker_events();
  test_ble_mailbox_keeps_latest_audio_without_overwriting_control();
  test_ble_mailbox_rejects_ninth_control_without_changing_fifo();
  test_coordinator_depends_on_transport_contract_not_ble_concrete_type();
  test_coordinator_routes_independent_scene_and_mode_to_selected_light_node();
  test_independent_scene_requires_node_confirmation_of_independent_mode();
  test_coordinator_forwards_led_count_and_refreshes_confirmed_value();
  return sozo::test::finish("scene mapper tests");
}
