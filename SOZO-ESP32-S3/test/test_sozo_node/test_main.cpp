#include <SceneMessageMapper.h>
#include <LightingController.h>
#include <NodeRegistry.h>
#include <NodeCoordinator.h>
#include <NodeFleetCoordinator.h>
#include <NodeFleetTransport.h>
#include <NodeFirmwareTransfer.h>
#include <NodeTransport.h>
#include <BleLinkStateMachine.h>
#include <BleOperationSupervisor.h>
#include <BleOutboundMailbox.h>
#include "../../../SOZO-Common/test/TestHarness.h"

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

class FakeNodeFleetTransport final : public sozo::NodeFleetTransport {
 public:
  FakeNodeFleetTransport() {
    links[0].remoteId = 0xC3000001U;
    links[1].remoteId = 0xC3000002U;
    for (FakeNodeTransport &link : links) {
      link.remoteCapabilities.bound = true;
      link.remoteCapabilities.capabilityBits = sozo::node::capabilityMask(
          sozo::node::Capability::LightOutput);
    }
  }

  bool begin() override {
    began = true;
    return true;
  }
  void tick(uint32_t) override {}
  size_t capacity() const override { return 2U; }
  sozo::NodeTransport *linkAt(size_t index) override {
    return index < capacity() ? &links[index] : nullptr;
  }
  const sozo::NodeTransport *linkAt(size_t index) const override {
    return index < capacity() ? &links[index] : nullptr;
  }
  bool releaseLink(size_t index) override {
    if (index >= capacity()) return false;
    links[index].isReady = false;
    return true;
  }
  bool openPairingWindow(uint32_t, uint32_t) override {
    pairingOpen = true;
    return true;
  }
  bool pairingWindowOpen(uint32_t) const override { return pairingOpen; }
  uint32_t pairingRemainingMs(uint32_t) const override {
    return pairingOpen ? 60000U : 0U;
  }
  bool scanning() const override { return pairingOpen; }

  FakeNodeTransport links[2]{};
  bool began{false};
  bool pairingOpen{false};
};

sozo::node::Envelope makeFirmwareStatusResponse(
    const sozo::node::Envelope &request,
    const sozo::node::FirmwareUpdateState state, const uint32_t nextOffset,
    const uint32_t imageSize,
    const sozo::node::FirmwareUpdateError error =
        sozo::node::FirmwareUpdateError::None) {
  sozo::node::FirmwareStatusPayload payload{};
  payload.state = state;
  payload.nextOffset = nextOffset;
  payload.imageSize = imageSize;
  payload.error = error;
  sozo::node::Envelope response{};
  response.sourceNodeId = request.targetNodeId;
  response.targetNodeId = request.sourceNodeId;
  response.correlationId = request.correlationId;
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::writeFirmwareStatus(response, payload));
  return response;
}

void test_firmware_transfer_waits_for_each_c3_status_before_advancing() {
  FakeNodeTransport transport;
  sozo::NodeFirmwareTransfer transfer(transport, 1000U, 2U);
  const uint8_t image[]{1U, 2U, 3U, 4U};
  uint8_t sha256[32]{};
  sha256[0] = 0xA5U;

  CHECK_TRUE(transfer.start(transport.remoteId, image, sizeof(image), sha256,
                            100U));
  transfer.tick(100U);
  CHECK_EQ(1U, transport.sentCount);
  CHECK_EQ(sozo::node::MessageType::FirmwareBegin,
           transport.sent[0].messageType);
  CHECK_TRUE(transfer.handleInbound(makeFirmwareStatusResponse(
      transport.sent[0], sozo::node::FirmwareUpdateState::Receiving, 0U,
      sizeof(image))));

  transfer.tick(110U);
  CHECK_EQ(2U, transport.sentCount);
  CHECK_EQ(sozo::node::MessageType::FirmwareChunk,
           transport.sent[1].messageType);
  sozo::node::FirmwareChunkPayload chunk{};
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::readFirmwareChunk(transport.sent[1], chunk));
  CHECK_EQ(0U, chunk.offset);
  CHECK_EQ(sizeof(image), chunk.dataLength);
  CHECK_TRUE(transfer.handleInbound(makeFirmwareStatusResponse(
      transport.sent[1], sozo::node::FirmwareUpdateState::Receiving,
      sizeof(image), sizeof(image))));

  transfer.tick(120U);
  CHECK_EQ(3U, transport.sentCount);
  CHECK_EQ(sozo::node::MessageType::FirmwareEnd,
           transport.sent[2].messageType);
  CHECK_TRUE(transfer.handleInbound(makeFirmwareStatusResponse(
      transport.sent[2], sozo::node::FirmwareUpdateState::ReadyToRestart,
      sizeof(image), sizeof(image))));
  CHECK_EQ(sozo::NodeFirmwareTransferState::Succeeded,
           transfer.status().state);
  CHECK_EQ(sizeof(image), transfer.status().confirmedBytes);
}

void test_firmware_transfer_retries_a_lost_status_then_fails_boundedly() {
  FakeNodeTransport transport;
  sozo::NodeFirmwareTransfer transfer(transport, 100U, 2U);
  const uint8_t image[]{1U};
  uint8_t sha256[32]{};
  CHECK_TRUE(transfer.start(transport.remoteId, image, sizeof(image), sha256,
                            100U));
  transfer.tick(100U);
  transfer.tick(201U);
  transfer.tick(302U);
  CHECK_EQ(3U, transport.sentCount);
  transfer.tick(403U);
  CHECK_EQ(sozo::NodeFirmwareTransferState::Failed,
           transfer.status().state);
  CHECK_EQ(sozo::node::FirmwareUpdateError::Timeout,
           transfer.status().error);
}

void test_fleet_allows_only_one_c3_firmware_update_at_a_time() {
  FakeNodeFleetTransport transport;
  for (FakeNodeTransport &link : transport.links) {
    link.remoteCapabilities.capabilityBits |= sozo::node::capabilityMask(
        sozo::node::Capability::FirmwareUpdate);
  }
  sozo::NodeFleetCoordinator fleet(transport);
  CHECK_TRUE(fleet.begin());
  const sozo::AudioFrame audio{};
  fleet.tick(100U, audio);

  const uint8_t image[]{1U, 2U};
  uint8_t sha256[32]{};
  CHECK_TRUE(fleet.requestNodeFirmwareUpdate(
      transport.links[0].remoteId, image, sizeof(image), sha256, 101U));
  CHECK_TRUE(!fleet.requestNodeFirmwareUpdate(
      transport.links[1].remoteId, image, sizeof(image), sha256, 102U));
  CHECK_EQ(transport.links[0].remoteId,
           fleet.firmwareUpdateStatus().nodeId);
}

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

  const sozo::node::SceneSnapshotPayload scene = sozo::makeSceneSnapshot(
      sozo::makeLightingScene(state, runtime.manualLitPixelCount));

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
  CHECK_EQ(static_cast<uint8_t>(spatial_light::LayoutProfile::Continuous),
           scene.spatialProfile);
  CHECK_EQ(0U, scene.spatialFlags);
}

void test_scene_does_not_grow_with_physical_led_count() {
  sozo::PersistedLightingState shortStrip{};
  sozo::PersistedLightingState longStrip{};
  shortStrip.layout.activeCount = 30;
  longStrip.layout.activeCount = 500;
  const sozo::LightingSnapshot runtime{sozo::EffectMode::Static, 30, false,
                                       -1};

  const auto shortScene = sozo::makeSceneSnapshot(
      sozo::makeLightingScene(shortStrip, runtime.manualLitPixelCount));
  const auto longScene = sozo::makeSceneSnapshot(
      sozo::makeLightingScene(longStrip, runtime.manualLitPixelCount));
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
    coordinator.tick(100U, audio);
    CHECK_EQ(1U, transport.sentCount);
    CHECK_EQ(sozo::node::MessageType::StatusRequest,
             transport.sent[0].messageType);
    sozo::node::StatusSnapshotPayload status{};
    status.controlMode = sozo::node::NodeControlMode::Independent;
    sozo::node::Envelope statusResponse{};
    statusResponse.sourceNodeId = transport.remoteId;
    statusResponse.targetNodeId = sozo::node::kCoordinatorNodeId;
    statusResponse.correlationId = transport.sent[0].correlationId;
    CHECK_EQ(sozo::node::CodecResult::Ok,
             sozo::node::writeStatusSnapshot(statusResponse, status));
    transport.queueInbound(statusResponse);
    coordinator.tick(101U, audio);
    CHECK_TRUE(coordinator.requestDesiredScene(
        transport.remoteId, sozo::makeLightingScene(state), 1U, 102U));
    coordinator.tick(135U, audio);

    CHECK_EQ(3U, transport.sentCount);
    CHECK_EQ(sozo::node::MessageType::SceneSnapshot,
             transport.sent[1].messageType);
    CHECK_EQ(sozo::node::MessageType::AudioFeatures,
             transport.sent[2].messageType);
    CHECK_EQ(static_cast<uint16_t>(sozo::node::TopicId::SpaceAudioFeatures),
             transport.sent[2].channelId);

    sozo::node::AudioFeaturesPayload decoded{};
    CHECK_EQ(sozo::node::CodecResult::Ok,
             sozo::node::readAudioFeatures(transport.sent[2], decoded));
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
  coordinator.tick(100U, audio);
  CHECK_EQ(1U, transport.sentCount);
  CHECK_EQ(sozo::node::MessageType::StatusRequest,
           transport.sent[0].messageType);
}

void test_scene_equality_compares_render_intent() {
  sozo::PersistedLightingState state{};
  const sozo::LightingSnapshot runtime{state.mode, state.layout.activeCount,
                                       false, -1};
  auto first = sozo::makeSceneSnapshot(
      sozo::makeLightingScene(state, runtime.manualLitPixelCount));
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

  coordinator.tick(1000U, {});
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

  coordinator.tick(1001U, {});
  CHECK_TRUE(coordinator.nodeReady());
  CHECK_EQ(2U, transport.sentCount);
  CHECK_EQ(sozo::node::MessageType::StatusRequest,
           transport.sent[1].messageType);
  CHECK_TRUE((transport.sent[1].flags & sozo::node::kFlagRequiresAck) != 0U);
  CHECK_TRUE(transport.sent[1].correlationId != 0U);

  sozo::node::StatusSnapshotPayload status{};
  status.freeHeapBytes = 45678U;
  status.ledCount = 62U;
  sozo::node::Envelope statusResponse{};
  statusResponse.channelId = transport.sent[1].channelId;
  statusResponse.sourceNodeId = transport.remoteId;
  statusResponse.targetNodeId = sozo::node::kCoordinatorNodeId;
  statusResponse.correlationId = transport.sent[1].correlationId;
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::writeStatusSnapshot(statusResponse, status));
  transport.queueInbound(statusResponse);
  coordinator.tick(1002U, {});
  const sozo::NodeRecord *record = coordinator.registry().find(transport.remoteId);
  CHECK_EQ(45678U, record->status.freeHeapBytes);
  CHECK_EQ(1002U, record->lastStatusMs);

  transport.isReady = false;
  coordinator.tick(1003U, {});
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
  coordinator.tick(100U, {});

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
  coordinator.tick(102U, {});

  auto independent = mainState;
  independent.mode = sozo::EffectMode::Rainbow;
  independent.brightness = 177U;
  CHECK_TRUE(coordinator.requestDesiredScene(
      transport.remoteId, sozo::makeLightingScene(independent), 7U, 103U));
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
  coordinator.tick(100U, {});

  CHECK_TRUE(!coordinator.requestDesiredScene(
      transport.remoteId, sozo::makeLightingScene(state), 6U, 101U));

  sozo::node::StatusSnapshotPayload status{};
  status.controlMode = sozo::node::NodeControlMode::Independent;
  sozo::node::Envelope statusResponse{};
  statusResponse.sourceNodeId = transport.remoteId;
  statusResponse.targetNodeId = sozo::node::kCoordinatorNodeId;
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::writeStatusSnapshot(statusResponse, status));
  transport.queueInbound(statusResponse);
  coordinator.tick(102U, {});

  CHECK_TRUE(coordinator.requestDesiredScene(
      transport.remoteId, sozo::makeLightingScene(state), 7U, 103U));
}

void test_coordinator_forwards_led_count_and_refreshes_confirmed_value() {
  FakeNodeTransport transport;
  transport.remoteCapabilities.bound = true;
  transport.remoteCapabilities.capabilityBits = sozo::node::capabilityMask(
      sozo::node::Capability::LightOutput);
  transport.remoteCapabilities.maxLedCount = 512U;
  sozo::NodeCoordinator coordinator(transport);
  CHECK_TRUE(coordinator.begin());
  coordinator.tick(100U, {});
  CHECK_EQ(sozo::node::MessageType::StatusRequest, transport.sent[0].messageType);

  sozo::node::StatusSnapshotPayload initialStatus{};
  initialStatus.ledCount = 60U;
  sozo::node::Envelope initialStatusResponse{};
  initialStatusResponse.sourceNodeId = transport.remoteId;
  initialStatusResponse.targetNodeId = sozo::node::kCoordinatorNodeId;
  initialStatusResponse.correlationId = transport.sent[0].correlationId;
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::writeStatusSnapshot(initialStatusResponse,
                                            initialStatus));
  transport.queueInbound(initialStatusResponse);
  coordinator.tick(102U, {});

  CHECK_TRUE(coordinator.requestNodeLedCount(transport.remoteId, 58U, 103U));
  CHECK_EQ(sozo::node::MessageType::LedCountRequest,
           transport.sent[1].messageType);
  CHECK_EQ(static_cast<uint16_t>(sozo::node::ServiceId::SetLedCount),
           transport.sent[1].channelId);
  sozo::node::LedCountPayload request{};
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::readLedCountRequest(transport.sent[1], request));
  CHECK_EQ(58U, request.ledCount);

  sozo::node::CommandReceiptPayload countReceipt{};
  countReceipt.accepted = true;
  sozo::node::Envelope countResponse{};
  countResponse.sourceNodeId = transport.remoteId;
  countResponse.targetNodeId = sozo::node::kCoordinatorNodeId;
  countResponse.correlationId = transport.sent[1].correlationId;
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::writeCommandReceipt(countResponse, countReceipt));
  transport.queueInbound(countResponse);
  coordinator.tick(104U, {});
  CHECK_EQ(sozo::node::MessageType::StatusRequest,
           transport.sent[2].messageType);

  sozo::node::StatusSnapshotPayload updatedStatus{};
  updatedStatus.ledCount = 58U;
  sozo::node::Envelope updatedStatusResponse{};
  updatedStatusResponse.sourceNodeId = transport.remoteId;
  updatedStatusResponse.targetNodeId = sozo::node::kCoordinatorNodeId;
  updatedStatusResponse.correlationId = transport.sent[2].correlationId;
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::writeStatusSnapshot(updatedStatusResponse,
                                            updatedStatus));
  transport.queueInbound(updatedStatusResponse);
  coordinator.tick(105U, {});
  const sozo::NodeRecord *record = coordinator.registry().find(transport.remoteId);
  CHECK_TRUE(record != nullptr);
  CHECK_EQ(58U, record->status.ledCount);
}

void test_fleet_initializes_ready_nodes_without_broadcasting_a_scene() {
  FakeNodeFleetTransport transport;
  sozo::NodeFleetCoordinator fleet(transport);
  CHECK_TRUE(fleet.begin());

  fleet.tick(100U, {});

  CHECK_TRUE(transport.began);
  CHECK_EQ(2U, fleet.onlineCount());
  CHECK_EQ(2U, fleet.registry().size());
  CHECK_EQ(1U, transport.links[0].sentCount);
  CHECK_EQ(1U, transport.links[1].sentCount);
  CHECK_EQ(transport.links[0].remoteId,
           transport.links[0].sent[0].targetNodeId);
  CHECK_EQ(transport.links[1].remoteId,
           transport.links[1].sent[0].targetNodeId);
  CHECK_EQ(sozo::node::MessageType::StatusRequest,
           transport.links[0].sent[0].messageType);
  CHECK_EQ(sozo::node::MessageType::StatusRequest,
           transport.links[1].sent[0].messageType);
}

void test_fleet_only_binds_an_unknown_node_during_explicit_pairing() {
  FakeNodeFleetTransport transport;
  transport.links[0].remoteCapabilities.bound = false;
  transport.links[1].isReady = false;
  sozo::NodeFleetCoordinator fleet(transport);
  CHECK_TRUE(fleet.begin());

  fleet.tick(100U, {});
  CHECK_EQ(0U, transport.links[0].sentCount);
  CHECK_EQ(0U, fleet.registry().size());

  CHECK_TRUE(fleet.openPairingWindow(101U));
  transport.links[0].isReady = true;
  ++transport.links[0].generation;
  fleet.tick(102U, {});
  CHECK_EQ(1U, transport.links[0].sentCount);
  CHECK_EQ(sozo::node::MessageType::BindRequest,
           transport.links[0].sent[0].messageType);
  CHECK_EQ(1U, fleet.registry().size());
}

void test_fleet_routes_a_device_command_only_to_the_selected_node() {
  FakeNodeFleetTransport transport;
  sozo::NodeFleetCoordinator fleet(transport);
  CHECK_TRUE(fleet.begin());

  fleet.tick(100U, {});

  CHECK_TRUE(fleet.requestNodeControlMode(
      transport.links[1].remoteId,
      sozo::node::NodeControlMode::Independent, 101U));
  CHECK_EQ(1U, transport.links[0].sentCount);
  CHECK_EQ(2U, transport.links[1].sentCount);
  CHECK_EQ(transport.links[1].remoteId,
           transport.links[1].sent[1].targetNodeId);
  CHECK_EQ(sozo::node::MessageType::ControlModeRequest,
           transport.links[1].sent[1].messageType);
}

}  // namespace

int runNodeTests() {
  test_firmware_transfer_waits_for_each_c3_status_before_advancing();
  test_firmware_transfer_retries_a_lost_status_then_fails_boundedly();
  test_fleet_allows_only_one_c3_firmware_update_at_a_time();
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
  test_fleet_initializes_ready_nodes_without_broadcasting_a_scene();
  test_fleet_only_binds_an_unknown_node_during_explicit_pairing();
  test_fleet_routes_a_device_command_only_to_the_selected_node();
  return sozo::test::finish("scene mapper tests");
}

#if defined(ARDUINO)
void setup() { runNodeTests(); }
void loop() {}
#else
int main(int, char **) { return runNodeTests(); }
#endif
