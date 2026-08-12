#ifdef ARDUINO
#include <Arduino.h>
#endif

#include <C3NodeApplication.h>
#include <NodeControlPort.h>
#include <NodeFirmwareReceiver.h>
#include <NodeLedCountPort.h>
#include <NodeSceneRuntime.h>

#include <initializer_list>
#include <PairingWindow.h>
#include "../../../SOZO-Common/test/TestHarness.h"

namespace {

class FakeFirmwareWriter final : public sozo::c3::FirmwareWriterPort {
 public:
  bool begin(const uint32_t imageSize) override {
    ++beginCalls;
    expectedSize = imageSize;
    length = 0U;
    return beginSucceeds;
  }

  bool write(const uint8_t *data, const size_t dataLength) override {
    ++writeCalls;
    if (!writeSucceeds || length + dataLength > sizeof(bytes)) return false;
    for (size_t i = 0; i < dataLength; ++i) bytes[length + i] = data[i];
    length += dataLength;
    return true;
  }

  sozo::c3::FirmwareCommitResult finish(
      const uint8_t expectedSha256[32]) override {
    ++finishCalls;
    for (size_t i = 0; i < 32U; ++i) receivedSha256[i] = expectedSha256[i];
    return finishResult;
  }

  void abort() override { ++abortCalls; }

  bool beginSucceeds{true};
  bool writeSucceeds{true};
  sozo::c3::FirmwareCommitResult finishResult{
      sozo::c3::FirmwareCommitResult::Ok};
  uint32_t expectedSize{0U};
  uint8_t bytes[512]{};
  uint8_t receivedSha256[32]{};
  size_t length{0U};
  int beginCalls{0};
  int writeCalls{0};
  int finishCalls{0};
  int abortCalls{0};
};

sozo::node::FirmwareBeginPayload makeFirmwareBegin(const uint32_t imageSize) {
  sozo::node::FirmwareBeginPayload begin{};
  begin.imageSize = imageSize;
  for (size_t i = 0; i < sizeof(begin.sha256); ++i) {
    begin.sha256[i] = static_cast<uint8_t>(i + 1U);
  }
  return begin;
}

sozo::node::FirmwareChunkPayload makeFirmwareChunk(
    const uint32_t offset, const std::initializer_list<uint8_t> bytes) {
  sozo::node::FirmwareChunkPayload chunk{};
  chunk.offset = offset;
  chunk.dataLength = static_cast<uint16_t>(bytes.size());
  size_t index = 0U;
  for (const uint8_t value : bytes) chunk.data[index++] = value;
  return chunk;
}

void test_firmware_receiver_commits_only_an_ordered_complete_image() {
  FakeFirmwareWriter writer;
  sozo::c3::NodeFirmwareReceiver receiver(writer, 512U, 5000U);

  CHECK_TRUE(receiver.begin(makeFirmwareBegin(5U), 100U));
  CHECK_TRUE(receiver.append(makeFirmwareChunk(0U, {1U, 2U, 3U}), 110U));
  CHECK_TRUE(receiver.append(makeFirmwareChunk(3U, {4U, 5U}), 120U));
  CHECK_TRUE(receiver.finish(130U));

  CHECK_EQ(1, writer.beginCalls);
  CHECK_EQ(2, writer.writeCalls);
  CHECK_EQ(1, writer.finishCalls);
  CHECK_EQ(5U, writer.length);
  CHECK_EQ(5U, receiver.status().nextOffset);
  CHECK_EQ(sozo::node::FirmwareUpdateState::ReadyToRestart,
           receiver.status().state);
  CHECK_TRUE(receiver.restartRequested());
}

void test_firmware_receiver_rejects_a_gap_without_advancing() {
  FakeFirmwareWriter writer;
  sozo::c3::NodeFirmwareReceiver receiver(writer, 512U, 5000U);
  CHECK_TRUE(receiver.begin(makeFirmwareBegin(5U), 100U));

  CHECK_TRUE(!receiver.append(makeFirmwareChunk(2U, {3U, 4U}), 110U));
  CHECK_EQ(0U, receiver.status().nextOffset);
  CHECK_EQ(sozo::node::FirmwareUpdateError::UnexpectedOffset,
           receiver.status().error);
  CHECK_EQ(0, writer.writeCalls);
  CHECK_TRUE(receiver.active());
}

void test_firmware_receiver_acknowledges_a_duplicate_chunk_without_rewrite() {
  FakeFirmwareWriter writer;
  sozo::c3::NodeFirmwareReceiver receiver(writer, 512U, 5000U);
  CHECK_TRUE(receiver.begin(makeFirmwareBegin(4U), 100U));
  const auto chunk = makeFirmwareChunk(0U, {1U, 2U});
  CHECK_TRUE(receiver.append(chunk, 110U));
  CHECK_TRUE(receiver.append(chunk, 120U));
  CHECK_EQ(1, writer.writeCalls);
  CHECK_EQ(2U, receiver.status().nextOffset);
}

void test_firmware_receiver_acknowledges_a_retried_matching_begin() {
  FakeFirmwareWriter writer;
  sozo::c3::NodeFirmwareReceiver receiver(writer, 512U, 5000U);
  const auto begin = makeFirmwareBegin(4U);
  CHECK_TRUE(receiver.begin(begin, 100U));
  CHECK_TRUE(receiver.begin(begin, 110U));
  CHECK_EQ(1, writer.beginCalls);
  CHECK_EQ(sozo::node::FirmwareUpdateError::None, receiver.status().error);
  CHECK_TRUE(receiver.active());
}

void test_firmware_receiver_aborts_on_hash_failure_disconnect_and_timeout() {
  FakeFirmwareWriter hashWriter;
  hashWriter.finishResult = sozo::c3::FirmwareCommitResult::HashMismatch;
  sozo::c3::NodeFirmwareReceiver hashReceiver(hashWriter, 512U, 5000U);
  CHECK_TRUE(hashReceiver.begin(makeFirmwareBegin(2U), 100U));
  CHECK_TRUE(hashReceiver.append(makeFirmwareChunk(0U, {1U, 2U}), 110U));
  CHECK_TRUE(!hashReceiver.finish(120U));
  CHECK_EQ(sozo::node::FirmwareUpdateError::HashMismatch,
           hashReceiver.status().error);
  CHECK_TRUE(!hashReceiver.restartRequested());

  FakeFirmwareWriter disconnectWriter;
  sozo::c3::NodeFirmwareReceiver disconnectReceiver(disconnectWriter, 512U,
                                                     5000U);
  CHECK_TRUE(disconnectReceiver.begin(makeFirmwareBegin(2U), 100U));
  disconnectReceiver.onDisconnected();
  CHECK_EQ(1, disconnectWriter.abortCalls);
  CHECK_EQ(sozo::node::FirmwareUpdateError::LinkLost,
           disconnectReceiver.status().error);

  FakeFirmwareWriter timeoutWriter;
  sozo::c3::NodeFirmwareReceiver timeoutReceiver(timeoutWriter, 512U, 5000U);
  CHECK_TRUE(timeoutReceiver.begin(makeFirmwareBegin(2U), 100U));
  timeoutReceiver.tick(5100U);
  CHECK_EQ(0, timeoutWriter.abortCalls);
  timeoutReceiver.tick(5101U);
  CHECK_EQ(1, timeoutWriter.abortCalls);
  CHECK_EQ(sozo::node::FirmwareUpdateError::Timeout,
           timeoutReceiver.status().error);
}

class FakeLightingSink final : public sozo::c3::NodeLightingSink {
 public:
  explicit FakeLightingSink(const sozo::PersistedLightingState &initial)
      : state_(initial) {}

  const sozo::PersistedLightingState &state() const override { return state_; }

  void begin(const sozo::PersistedLightingState &state) override {
    state_ = state;
    ++beginCalls;
  }

  void applyState(const sozo::PersistedLightingState &state) override {
    state_ = state;
    ++applyCalls;
  }

  void setLitPixelCount(const uint16_t count) override {
    lastLitPixelCount = count;
    ++litPixelCalls;
  }

  void tick(const uint32_t now, const sozo::AudioFrame &audio) override {
    lastTickNow = now;
    lastAudio = audio;
    ++tickCalls;
  }

  sozo::PersistedLightingState state_{};
  int beginCalls{0};
  int applyCalls{0};
  int litPixelCalls{0};
  int tickCalls{0};
  uint16_t lastLitPixelCount{0};
  uint32_t lastTickNow{0};
  sozo::AudioFrame lastAudio{};
};

class FakeNodeTransport final : public sozo::c3::NodeTransportPort {
 public:
  bool begin(sozo::node::NodeId nodeId,
             const sozo::node::CapabilitiesPayload &capabilities, bool bound,
             sozo::node::NodeId coordinatorNodeId, uint64_t,
             uint8_t) override {
    began = true;
    localNodeId = nodeId;
    receivedCapabilities = capabilities;
    initiallyBound = bound;
    initialCoordinatorNodeId = coordinatorNodeId;
    return true;
  }

  void handlePairingEvent(sozo::c3::PairingEvent) override {}

  bool connected() const override { return isConnected; }

  bool popInbound(sozo::node::Envelope &envelope) override {
    if (!hasInbound) return false;
    envelope = inbound;
    hasInbound = false;
    return true;
  }

  bool send(const sozo::node::Envelope &envelope) override {
    if (sentCount >= 4U) return false;
    sent[sentCount++] = envelope;
    return true;
  }

  bool bindCoordinator(sozo::node::NodeId coordinatorNodeId) override {
    boundCoordinatorNodeId = coordinatorNodeId;
    return true;
  }

  void clearBinding() override { bindingCleared = true; }

  uint64_t connectedPeerIdentityAddress() const override {
    return 0xAABBCCDDEEFFU;
  }

  uint8_t connectedPeerIdentityAddressType() const override { return 1U; }

  void queueInbound(const sozo::node::Envelope &envelope) {
    inbound = envelope;
    hasInbound = true;
  }

  bool began{false};
  bool isConnected{true};
  bool initiallyBound{false};
  bool bindingCleared{false};
  bool hasInbound{false};
  sozo::node::NodeId localNodeId{0};
  sozo::node::NodeId initialCoordinatorNodeId{0};
  sozo::node::NodeId boundCoordinatorNodeId{0};
  sozo::node::CapabilitiesPayload receivedCapabilities{};
  sozo::node::Envelope inbound{};
  sozo::node::Envelope sent[4]{};
  size_t sentCount{0};
};

class FakeBindingRepository final : public sozo::c3::NodeBindingRepository {
 public:
  sozo::c3::NodeBinding load() override { return binding; }

  bool save(sozo::node::NodeId coordinatorNodeId,
            uint64_t identityAddress, uint8_t identityAddressType) override {
    binding.bound = true;
    binding.coordinatorNodeId = coordinatorNodeId;
    binding.bleIdentityAddress = identityAddress;
    binding.bleIdentityAddressType = identityAddressType;
    ++saveCalls;
    return saveSucceeds;
  }

  bool clear() override {
    binding = {};
    ++clearCalls;
    return clearSucceeds;
  }

  sozo::c3::NodeBinding binding{};
  bool saveSucceeds{true};
  bool clearSucceeds{true};
  int saveCalls{0};
  int clearCalls{0};
};

class FakeNodeControlRepository final
    : public sozo::c3::NodeControlRepository {
 public:
  sozo::c3::NodeControlState load() override { return state; }

  bool save(const sozo::c3::NodeControlState &next) override {
    state = next;
    ++saveCalls;
    return saveSucceeds;
  }

  bool clear() override {
    state = {};
    ++clearCalls;
    return clearSucceeds;
  }

  sozo::c3::NodeControlState state{};
  bool saveSucceeds{true};
  bool clearSucceeds{true};
  int saveCalls{0};
  int clearCalls{0};
};

class FakeNodeLedCountRepository final
    : public sozo::c3::NodeLedCountRepository {
 public:
  sozo::c3::NodeLedCountState load() override { return state; }

  bool save(const sozo::c3::NodeLedCountState &next) override {
    if (!saveSucceeds) return false;
    state = next;
    ++saveCalls;
    return true;
  }

  sozo::c3::NodeLedCountState state{};
  bool saveSucceeds{true};
  int saveCalls{0};
};

class FakeButtonInput final : public sozo::c3::NodeButtonInput {
 public:
  bool pressed() const override { return isPressed; }
  bool isPressed{false};
};

class FakeDiagnostics final : public sozo::c3::NodeDiagnosticsPort {
 public:
  uint32_t freeHeapBytes() const override { return 45678U; }
};

sozo::node::SceneSnapshotPayload makeScene() {
  sozo::node::SceneSnapshotPayload scene{};
  scene.effectMode = static_cast<uint8_t>(sozo::EffectMode::GlassFlow);
  scene.brightness = 180;
  scene.primaryRed = 10;
  scene.primaryGreen = 20;
  scene.primaryBlue = 30;
  scene.rainbowStyle = 2;
  scene.flowSpeed = 88;
  scene.cometTail = 42;
  scene.cometSpeed = 33;
  scene.cometDensity = 4;
  scene.cometBackground = 12;
  scene.cometRandom = true;
  scene.audioSensitivityX100 = 175;
  scene.audioColorGainX100 = 250;
  scene.audioHueDrive = 2;
  scene.breathFloorPercent = 21;
  scene.secondaryRed = 40;
  scene.secondaryGreen = 50;
  scene.secondaryBlue = 60;
  scene.pulseAmplitudePercent = 75;
  scene.pulseHeightPercent = 9;
  scene.animationBrightness = 210;
  scene.audioColorStyle = 3;
  scene.cometColorStyle = 2;
  scene.manualLitPixelCount = -1;
  scene.spatialProfile = 1;
  scene.spatialFlags = sozo::node::kSpatialFlagReversed;
  return scene;
}

void test_new_scene_applies_render_intent_but_preserves_local_hardware() {
  sozo::PersistedLightingState local{};
  local.layout.activeCount = 480;
  local.layout.centerIndex = 239;
  local.layout.profile = spatial_light::LayoutProfile::Continuous;
  local.layout.reversed = false;
  local.startupColor = {70, 80, 90};
  local.startupAnimationSpeed = 1.25F;
  FakeLightingSink sink(local);
  sozo::c3::NodeSceneRuntime runtime(sink);

  CHECK_EQ(sozo::c3::SceneApplyResult::Applied,
           runtime.applyScene(makeScene(), 5U, 10000U, 2000U));
  CHECK_EQ(1, sink.applyCalls);
  CHECK_EQ(sozo::EffectMode::GlassFlow, sink.state_.mode);
  CHECK_EQ(180U, sink.state_.brightness);
  CHECK_EQ(10U, sink.state_.primaryColor.red);
  CHECK_EQ(88U, sink.state_.lighting.flowSpeed);
  CHECK_EQ(40U, sink.state_.lighting.secondaryRed);
  CHECK_EQ(480U, sink.state_.layout.activeCount);
  CHECK_EQ(239U, sink.state_.layout.centerIndex);
  CHECK_EQ(spatial_light::LayoutProfile::Continuous,
           sink.state_.layout.profile);
  CHECK_TRUE(!sink.state_.layout.reversed);
  CHECK_EQ(70U, sink.state_.startupColor.red);
  CHECK_EQ(5U, runtime.lastAppliedSceneRevision());
}

void test_duplicate_and_stale_scene_versions_do_not_reapply() {
  FakeLightingSink sink(sozo::makeDefaultPersistedLightingState());
  sozo::c3::NodeSceneRuntime runtime(sink);
  CHECK_EQ(sozo::c3::SceneApplyResult::Applied,
           runtime.applyScene(makeScene(), 10U, 5000U, 1000U));
  CHECK_EQ(sozo::c3::SceneApplyResult::Duplicate,
           runtime.applyScene(makeScene(), 10U, 5001U, 1001U));
  CHECK_EQ(sozo::c3::SceneApplyResult::Stale,
           runtime.applyScene(makeScene(), 9U, 5002U, 1002U));
  CHECK_EQ(1, sink.applyCalls);
}

void test_independent_scene_isolated_from_follow_scene_until_follow_is_restored() {
  FakeLightingSink sink(sozo::makeDefaultPersistedLightingState());
  sozo::c3::NodeSceneRuntime runtime(sink);
  auto follow = makeScene();
  follow.primaryRed = 10U;
  CHECK_EQ(sozo::c3::SceneApplyResult::Applied,
           runtime.applyScene(follow, 4U, 100U, 50U,
                              sozo::c3::SceneTarget::FollowSpace));
  CHECK_EQ(10U, sink.state_.primaryColor.red);

  CHECK_TRUE(runtime.setControlMode(sozo::node::NodeControlMode::Independent));
  auto independent = follow;
  independent.primaryRed = 88U;
  CHECK_EQ(sozo::c3::SceneApplyResult::Applied,
           runtime.applyScene(independent, 1U, 110U, 60U,
                              sozo::c3::SceneTarget::Independent));
  CHECK_EQ(88U, sink.state_.primaryColor.red);

  auto newerFollow = follow;
  newerFollow.primaryRed = 23U;
  CHECK_EQ(sozo::c3::SceneApplyResult::Applied,
           runtime.applyScene(newerFollow, 5U, 120U, 70U,
                              sozo::c3::SceneTarget::FollowSpace));
  CHECK_EQ(88U, sink.state_.primaryColor.red);

  CHECK_TRUE(runtime.setControlMode(sozo::node::NodeControlMode::FollowMain));
  CHECK_EQ(23U, sink.state_.primaryColor.red);
  CHECK_EQ(sozo::node::NodeControlMode::FollowMain, runtime.controlMode());
}

void test_independent_scene_state_survives_a_runtime_restart() {
  FakeLightingSink firstSink(sozo::makeDefaultPersistedLightingState());
  sozo::c3::NodeSceneRuntime firstRuntime(firstSink);
  auto independent = makeScene();
  independent.primaryRed = 73U;
  CHECK_TRUE(
      firstRuntime.setControlMode(sozo::node::NodeControlMode::Independent));
  CHECK_EQ(sozo::c3::SceneApplyResult::Applied,
           firstRuntime.applyScene(independent, 6U, 200U, 100U,
                                   sozo::c3::SceneTarget::Independent));

  const sozo::c3::NodeControlState persisted = firstRuntime.controlState();
  CHECK_EQ(sozo::LightControlMode::Independent, persisted.controlMode);
  CHECK_TRUE(persisted.hasIndependentScene);
  CHECK_EQ(73U, persisted.independentState.primaryColor.red);

  FakeLightingSink restartedSink(sozo::makeDefaultPersistedLightingState());
  restartedSink.state_.layout.activeCount = 60U;
  sozo::c3::NodeSceneRuntime restartedRuntime(restartedSink);
  CHECK_TRUE(restartedRuntime.restoreControlState(persisted));
  CHECK_EQ(sozo::node::NodeControlMode::Independent,
           restartedRuntime.controlMode());
  CHECK_EQ(73U, restartedSink.state_.primaryColor.red);
  CHECK_EQ(60U, restartedSink.state_.layout.activeCount);
}

void test_independent_scene_accepts_a_new_coordinator_revision_after_restart() {
  FakeLightingSink sink(sozo::makeDefaultPersistedLightingState());
  sozo::c3::NodeSceneRuntime runtime(sink);
  CHECK_TRUE(
      runtime.setControlMode(sozo::node::NodeControlMode::Independent));

  auto persistedScene = makeScene();
  persistedScene.primaryRed = 41U;
  CHECK_EQ(sozo::c3::SceneApplyResult::Applied,
           runtime.applyScene(persistedScene, 18U, 100U, 50U,
                              sozo::c3::SceneTarget::Independent));

  // A restarted coordinator restarts its local revision counter.  Independent
  // scenes are explicit, ordered device commands, so the new coordinator must
  // be able to replace a persisted local scene with revision 1.
  auto freshScene = persistedScene;
  freshScene.primaryRed = 99U;
  CHECK_EQ(sozo::c3::SceneApplyResult::Applied,
           runtime.applyScene(freshScene, 1U, 200U, 100U,
                              sozo::c3::SceneTarget::Independent));
  CHECK_EQ(99U, sink.state_.primaryColor.red);
  CHECK_EQ(1U, runtime.controlState().independentRevision);
}

void test_latest_audio_features_drive_tick_on_coordinator_clock() {
  FakeLightingSink sink(sozo::makeDefaultPersistedLightingState());
  sozo::c3::NodeSceneRuntime runtime(sink);
  CHECK_EQ(sozo::c3::SceneApplyResult::Applied,
           runtime.applyScene(makeScene(), 1U, 10000U, 2000U));

  sozo::node::AudioFeaturesPayload audio{};
  audio.volumeX100 = 12345;
  audio.fastEnergyX100 = 2222;
  audio.slowEnergyX100 = 3333;
  audio.beatPulseX100 = 4444;
  audio.framesRead = 55;
  audio.available = true;
  CHECK_TRUE(runtime.applyAudioFeatures(audio, 7U));
  CHECK_TRUE(!runtime.applyAudioFeatures(audio, 6U));

  runtime.tick(2500U);
  CHECK_EQ(10500U, sink.lastTickNow);
  CHECK_EQ(123.45F, sink.lastAudio.volume);
  CHECK_EQ(22.22F, sink.lastAudio.fastEnergy);
  CHECK_EQ(33.33F, sink.lastAudio.slowEnergy);
  CHECK_EQ(44.44F, sink.lastAudio.beatPulse);
  CHECK_EQ(55U, sink.lastAudio.framesRead);
  CHECK_TRUE(sink.lastAudio.available);
}

void test_audio_sequence_restarts_after_transport_disconnect() {
  FakeLightingSink sink(sozo::makeDefaultPersistedLightingState());
  sozo::c3::NodeSceneRuntime runtime(sink);

  sozo::node::AudioFeaturesPayload beforeRestart{};
  beforeRestart.volumeX100 = 100U;
  beforeRestart.available = true;
  CHECK_TRUE(runtime.applyAudioFeatures(beforeRestart, 900U));

  runtime.onDisconnected();

  sozo::node::AudioFeaturesPayload afterRestart{};
  afterRestart.volumeX100 = 7500U;
  afterRestart.fastEnergyX100 = 5000U;
  afterRestart.slowEnergyX100 = 4000U;
  afterRestart.beatPulseX100 = 3000U;
  afterRestart.available = true;
  CHECK_TRUE(runtime.applyAudioFeatures(afterRestart, 1U));

  runtime.tick(100U);
  CHECK_EQ(75.0F, sink.lastAudio.volume);
  CHECK_EQ(50.0F, sink.lastAudio.fastEnergy);
  CHECK_EQ(40.0F, sink.lastAudio.slowEnergy);
  CHECK_EQ(30.0F, sink.lastAudio.beatPulse);
}

void test_node_application_resets_stream_ordering_on_disconnect() {
  FakeLightingSink sink(sozo::makeDefaultPersistedLightingState());
  FakeButtonInput button;
  FakeDiagnostics diagnostics;
  FakeBindingRepository bindings;
  FakeNodeControlRepository controls;
  FakeNodeLedCountRepository ledCounts;
  FakeNodeTransport transport;
  sozo::c3::PairingWindow pairing(1500U, 60000U);
  const sozo::c3::C3NodeProfile profile{60U, 512U, 1U};
  sozo::c3::C3NodeApplication app(sink, button, diagnostics, pairing,
                                   transport, bindings, controls, ledCounts,
                                   profile);
  CHECK_TRUE(app.begin(0xC3000042U, 0U));

  sozo::node::AudioFeaturesPayload audio{};
  audio.volumeX100 = 100U;
  audio.available = true;
  sozo::node::Envelope envelope{};
  envelope.sourceNodeId = sozo::node::kCoordinatorNodeId;
  envelope.sequence = 900U;
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::writeAudioFeatures(envelope, audio));
  transport.queueInbound(envelope);
  app.tick(10U);
  CHECK_EQ(1.0F, sink.lastAudio.volume);

  transport.isConnected = false;
  app.tick(11U);
  transport.isConnected = true;
  app.tick(12U);

  audio.volumeX100 = 7500U;
  envelope = {};
  envelope.sourceNodeId = sozo::node::kCoordinatorNodeId;
  envelope.sequence = 1U;
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::writeAudioFeatures(envelope, audio));
  transport.queueInbound(envelope);
  app.tick(13U);
  CHECK_EQ(75.0F, sink.lastAudio.volume);
}

void test_node_application_exposes_and_runs_firmware_update_protocol() {
  FakeLightingSink sink(sozo::makeDefaultPersistedLightingState());
  FakeButtonInput button;
  FakeDiagnostics diagnostics;
  FakeBindingRepository bindings;
  bindings.binding.bound = true;
  bindings.binding.coordinatorNodeId = sozo::node::kCoordinatorNodeId;
  bindings.binding.bleIdentityAddress = 0xAABBCCDDEEFFU;
  FakeNodeControlRepository controls;
  FakeNodeLedCountRepository ledCounts;
  FakeNodeTransport transport;
  FakeFirmwareWriter writer;
  sozo::c3::NodeFirmwareReceiver firmwareReceiver(writer, 512U, 5000U);
  sozo::c3::PairingWindow pairing(1500U, 60000U);
  const sozo::c3::C3NodeProfile profile{60U, 512U, 1U};
  sozo::c3::C3NodeApplication app(
      sink, button, diagnostics, pairing, transport, bindings, controls,
      ledCounts, profile, &firmwareReceiver);
  CHECK_TRUE(app.begin(0xC3000042U, 0U));
  CHECK_TRUE((transport.receivedCapabilities.capabilityBits &
              sozo::node::capabilityMask(
                  sozo::node::Capability::FirmwareUpdate)) != 0U);

  sozo::node::Envelope request{};
  request.sourceNodeId = sozo::node::kCoordinatorNodeId;
  request.targetNodeId = app.nodeId();
  request.correlationId = 71U;
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::writeFirmwareBegin(request, makeFirmwareBegin(3U)));
  transport.queueInbound(request);
  CHECK_EQ(sozo::c3::C3NodeEvent::None, app.tick(100U));

  request = {};
  request.sourceNodeId = sozo::node::kCoordinatorNodeId;
  request.targetNodeId = app.nodeId();
  request.correlationId = 72U;
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::writeFirmwareChunk(
               request, makeFirmwareChunk(0U, {1U, 2U, 3U})));
  transport.queueInbound(request);
  CHECK_EQ(sozo::c3::C3NodeEvent::None, app.tick(110U));

  request = {};
  request.sourceNodeId = sozo::node::kCoordinatorNodeId;
  request.targetNodeId = app.nodeId();
  request.correlationId = 73U;
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::writeFirmwareEnd(request));
  transport.queueInbound(request);
  CHECK_EQ(sozo::c3::C3NodeEvent::FirmwareRestartRequested, app.tick(120U));

  CHECK_EQ(3U, transport.sentCount);
  sozo::node::FirmwareStatusPayload status{};
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::readFirmwareStatus(transport.sent[2], status));
  CHECK_EQ(sozo::node::FirmwareUpdateState::ReadyToRestart, status.state);
  CHECK_EQ(3U, status.nextOffset);
}

void test_manual_lit_count_is_local_output_command_not_layout_change() {
  sozo::PersistedLightingState local{};
  local.layout.activeCount = 300;
  FakeLightingSink sink(local);
  sozo::c3::NodeSceneRuntime runtime(sink);
  auto scene = makeScene();
  scene.manualLitPixelCount = 120;
  CHECK_EQ(sozo::c3::SceneApplyResult::Applied,
           runtime.applyScene(scene, 1U, 100U, 50U));
  CHECK_EQ(1, sink.litPixelCalls);
  CHECK_EQ(120U, sink.lastLitPixelCount);
  CHECK_EQ(300U, sink.state_.layout.activeCount);
}

void test_pairing_window_requires_runtime_long_press() {
  sozo::c3::PairingWindow window(1500U, 60000U);
  window.begin(false, 0U);
  CHECK_EQ(sozo::c3::PairingEvent::None, window.tick(true, 100U));
  CHECK_EQ(sozo::c3::PairingEvent::None, window.tick(true, 1599U));
  CHECK_EQ(sozo::c3::PairingEvent::Opened, window.tick(true, 1600U));
  CHECK_TRUE(window.isOpen());
  CHECK_EQ(sozo::c3::PairingEvent::None, window.tick(false, 61599U));
  CHECK_EQ(sozo::c3::PairingEvent::Closed, window.tick(false, 61600U));
  CHECK_TRUE(!window.isOpen());
}

void test_boot_held_button_must_be_released_before_pairing_press() {
  sozo::c3::PairingWindow window(1500U, 60000U);
  window.begin(true, 0U);
  CHECK_EQ(sozo::c3::PairingEvent::None, window.tick(true, 5000U));
  CHECK_TRUE(!window.isOpen());
  CHECK_EQ(sozo::c3::PairingEvent::None, window.tick(false, 5100U));
  CHECK_EQ(sozo::c3::PairingEvent::None, window.tick(true, 5200U));
  CHECK_EQ(sozo::c3::PairingEvent::Opened, window.tick(true, 6700U));
  CHECK_TRUE(window.isOpen());
}

void test_short_press_does_not_open_pairing() {
  sozo::c3::PairingWindow window(1500U, 60000U);
  window.begin(false, 0U);
  window.tick(true, 100U);
  window.tick(false, 1200U);
  CHECK_TRUE(!window.isOpen());
}

void test_continuous_eight_second_hold_requests_binding_reset() {
  sozo::c3::PairingWindow window(1500U, 60000U, 8000U);
  window.begin(false, 0U);
  CHECK_EQ(sozo::c3::PairingEvent::None, window.tick(true, 100U));
  CHECK_EQ(sozo::c3::PairingEvent::Opened, window.tick(true, 1600U));
  CHECK_EQ(sozo::c3::PairingEvent::None, window.tick(true, 8099U));
  CHECK_EQ(sozo::c3::PairingEvent::ClearBindingRequested,
           window.tick(true, 8100U));
  CHECK_TRUE(!window.isOpen());
  CHECK_EQ(sozo::c3::PairingEvent::None, window.tick(true, 9000U));
}

void test_node_application_keeps_transport_and_rendering_separate() {
  FakeLightingSink sink(sozo::makeDefaultPersistedLightingState());
  FakeButtonInput button;
  FakeDiagnostics diagnostics;
  FakeBindingRepository bindings;
  FakeNodeControlRepository controls;
  FakeNodeLedCountRepository ledCounts;
  FakeNodeTransport transport;
  sozo::c3::PairingWindow pairing(1500U, 60000U);
  const sozo::c3::C3NodeProfile profile{60U, 512U, 1U};
  sozo::c3::C3NodeApplication app(sink, button, diagnostics, pairing,
                                   transport, bindings, controls, ledCounts,
                                   profile);

  CHECK_TRUE(app.begin(0xC3000042U, 0U));
  CHECK_TRUE(transport.began);
  CHECK_EQ(1, sink.beginCalls);
  CHECK_EQ(60U, sink.state_.layout.activeCount);
  CHECK_EQ(512U, transport.receivedCapabilities.maxLedCount);

  sozo::node::Envelope sceneCommand{};
  sceneCommand.channelId =
      static_cast<uint16_t>(sozo::node::TopicId::SpaceScene);
  sceneCommand.flags = sozo::node::kFlagRequiresAck;
  sceneCommand.sourceNodeId = sozo::node::kCoordinatorNodeId;
  sceneCommand.sceneRevision = 1U;
  sceneCommand.timestampMs = 100U;
  sceneCommand.correlationId = 17U;
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::writeSceneSnapshot(sceneCommand, makeScene()));
  transport.queueInbound(sceneCommand);
  app.tick(10U);
  CHECK_EQ(1U, transport.sentCount);
  CHECK_EQ(sozo::node::MessageType::Ack, transport.sent[0].messageType);
  CHECK_EQ(17U, transport.sent[0].correlationId);
  sozo::node::CommandReceiptPayload receipt{};
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::readCommandReceipt(transport.sent[0], receipt));
  CHECK_TRUE(receipt.accepted);
  CHECK_EQ(1U, receipt.lastAppliedSceneRevision);

  const int renderedAfterCommand = sink.tickCalls;
  app.tick(11U);
  CHECK_EQ(renderedAfterCommand + 1, sink.tickCalls);
  CHECK_EQ(1U, transport.sentCount);

  sozo::node::Envelope statusRequest{};
  statusRequest.flags = sozo::node::kFlagRequiresAck;
  statusRequest.sourceNodeId = sozo::node::kCoordinatorNodeId;
  statusRequest.correlationId = 18U;
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::writeStatusRequest(statusRequest));
  transport.queueInbound(statusRequest);
  app.tick(12U);
  CHECK_EQ(2U, transport.sentCount);
  CHECK_EQ(sozo::node::MessageType::StatusSnapshot,
           transport.sent[1].messageType);
  sozo::node::StatusSnapshotPayload status{};
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::readStatusSnapshot(transport.sent[1], status));
  CHECK_EQ(45678U, status.freeHeapBytes);
  CHECK_EQ(60U, status.ledCount);
}

void test_failed_control_mode_persistence_restores_follow_output() {
  FakeLightingSink sink(sozo::makeDefaultPersistedLightingState());
  FakeButtonInput button;
  FakeDiagnostics diagnostics;
  FakeBindingRepository bindings;
  FakeNodeControlRepository controls;
  controls.state.controlMode = sozo::LightControlMode::FollowScene;
  controls.state.hasIndependentScene = true;
  controls.state.independentState = sozo::makeDefaultPersistedLightingState();
  controls.state.independentState.primaryColor = {88U, 77U, 66U};
  controls.state.independentRevision = 3U;
  controls.saveSucceeds = false;
  FakeNodeLedCountRepository ledCounts;
  FakeNodeTransport transport;
  sozo::c3::PairingWindow pairing(1500U, 60000U);
  const sozo::c3::C3NodeProfile profile{60U, 512U, 1U};
  sozo::c3::C3NodeApplication app(sink, button, diagnostics, pairing,
                                   transport, bindings, controls, ledCounts,
                                   profile);
  CHECK_TRUE(app.begin(0xC3000042U, 0U));
  CHECK_EQ(255U, sink.state_.primaryColor.red);

  sozo::node::ControlModePayload payload{};
  payload.controlMode = sozo::node::NodeControlMode::Independent;
  sozo::node::Envelope request{};
  request.flags = sozo::node::kFlagRequiresAck;
  request.sourceNodeId = sozo::node::kCoordinatorNodeId;
  request.correlationId = 81U;
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::writeControlModeRequest(request, payload));
  transport.queueInbound(request);
  app.tick(10U);

  sozo::node::CommandReceiptPayload receipt{};
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::readCommandReceipt(transport.sent[0], receipt));
  CHECK_TRUE(!receipt.accepted);
  CHECK_EQ(5U, receipt.errorCode);
  CHECK_EQ(255U, sink.state_.primaryColor.red);
}

void test_node_led_count_is_saved_locally_and_survives_scene_commands() {
  FakeLightingSink sink(sozo::makeDefaultPersistedLightingState());
  FakeButtonInput button;
  FakeDiagnostics diagnostics;
  FakeBindingRepository bindings;
  FakeNodeControlRepository controls;
  FakeNodeLedCountRepository ledCounts;
  FakeNodeTransport transport;
  sozo::c3::PairingWindow pairing(1500U, 60000U);
  const sozo::c3::C3NodeProfile profile{60U, 512U, 1U};
  sozo::c3::C3NodeApplication app(sink, button, diagnostics, pairing,
                                   transport, bindings, controls, ledCounts,
                                   profile);

  CHECK_TRUE(app.begin(0xC3000042U, 0U));
  CHECK_EQ(60U, sink.state_.layout.activeCount);

  sozo::node::LedCountPayload count{};
  count.ledCount = 58U;
  sozo::node::Envelope request{};
  request.flags = sozo::node::kFlagRequiresAck;
  request.sourceNodeId = sozo::node::kCoordinatorNodeId;
  request.correlationId = 91U;
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::writeLedCountRequest(request, count));
  transport.queueInbound(request);
  app.tick(10U);

  sozo::node::CommandReceiptPayload receipt{};
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::readCommandReceipt(transport.sent[0], receipt));
  CHECK_TRUE(receipt.accepted);
  CHECK_EQ(58U, ledCounts.state.ledCount);
  CHECK_EQ(58U, sink.state_.layout.activeCount);

  sozo::node::Envelope scene{};
  scene.channelId =
      static_cast<uint16_t>(sozo::node::TopicId::SpaceScene);
  scene.flags = sozo::node::kFlagRequiresAck;
  scene.sourceNodeId = sozo::node::kCoordinatorNodeId;
  scene.sceneRevision = 1U;
  scene.timestampMs = 100U;
  scene.correlationId = 92U;
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::writeSceneSnapshot(scene, makeScene()));
  transport.queueInbound(scene);
  app.tick(11U);
  CHECK_EQ(58U, sink.state_.layout.activeCount);

  sozo::node::Envelope statusRequest{};
  statusRequest.flags = sozo::node::kFlagRequiresAck;
  statusRequest.sourceNodeId = sozo::node::kCoordinatorNodeId;
  statusRequest.correlationId = 93U;
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::writeStatusRequest(statusRequest));
  transport.queueInbound(statusRequest);
  app.tick(12U);
  sozo::node::StatusSnapshotPayload status{};
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::readStatusSnapshot(transport.sent[2], status));
  CHECK_EQ(58U, status.ledCount);
}

void test_node_geometry_is_saved_applied_and_reported() {
  FakeLightingSink sink(sozo::makeDefaultPersistedLightingState());
  FakeButtonInput button;
  FakeDiagnostics diagnostics;
  FakeBindingRepository bindings;
  FakeNodeControlRepository controls;
  FakeNodeLedCountRepository ledCounts;
  FakeNodeTransport transport;
  sozo::c3::PairingWindow pairing(1500U, 60000U);
  const sozo::c3::C3NodeProfile profile{60U, 512U, 1U};
  sozo::c3::C3NodeApplication app(sink, button, diagnostics, pairing,
                                   transport, bindings, controls, ledCounts,
                                   profile);
  CHECK_TRUE(app.begin(0xC3000042U, 0U));

  sozo::node::LedGeometryPayload geometry{};
  geometry.layoutProfile = 1U;
  geometry.activeCount = 72U;
  geometry.centerIndex = 35U;
  geometry.leftCount = 18U;
  geometry.centerCount = 36U;
  geometry.rightCount = 18U;
  geometry.spatialFlags = sozo::node::kSpatialFlagReversed;
  sozo::node::Envelope request{};
  request.flags = sozo::node::kFlagRequiresAck;
  request.sourceNodeId = sozo::node::kCoordinatorNodeId;
  request.correlationId = 94U;
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::writeLedGeometryRequest(request, geometry));
  transport.queueInbound(request);
  app.tick(10U);

  sozo::node::CommandReceiptPayload receipt{};
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::readCommandReceipt(transport.sent[0], receipt));
  CHECK_TRUE(receipt.accepted);
  CHECK_EQ(72U, ledCounts.state.ledCount);
  CHECK_EQ(1U, ledCounts.state.layoutProfile);
  CHECK_TRUE(ledCounts.state.reversed);
  CHECK_EQ(spatial_light::LayoutProfile::Segmented, sink.state_.layout.profile);
  CHECK_EQ(72U, sink.state_.layout.activeCount);
  CHECK_EQ(18U, sink.state_.layout.leftCount);
  CHECK_EQ(36U, sink.state_.layout.centerCount);
  CHECK_EQ(18U, sink.state_.layout.rightCount);
  CHECK_TRUE(sink.state_.layout.reversed);

  sozo::node::Envelope statusRequest{};
  statusRequest.flags = sozo::node::kFlagRequiresAck;
  statusRequest.sourceNodeId = sozo::node::kCoordinatorNodeId;
  statusRequest.correlationId = 95U;
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::writeStatusRequest(statusRequest));
  transport.queueInbound(statusRequest);
  app.tick(11U);
  sozo::node::StatusSnapshotPayload status{};
  CHECK_EQ(sozo::node::CodecResult::Ok,
           sozo::node::readStatusSnapshot(transport.sent[1], status));
  CHECK_EQ(72U, status.ledCount);
  CHECK_EQ(1U, status.layoutProfile);
  CHECK_EQ(18U, status.leftCount);
  CHECK_EQ(36U, status.centerCount);
  CHECK_EQ(18U, status.rightCount);
  CHECK_EQ(sozo::node::kSpatialFlagReversed, status.spatialFlags);
}

}  // namespace

int runNodeRuntimeTests() {
  test_firmware_receiver_commits_only_an_ordered_complete_image();
  test_firmware_receiver_rejects_a_gap_without_advancing();
  test_firmware_receiver_acknowledges_a_duplicate_chunk_without_rewrite();
  test_firmware_receiver_acknowledges_a_retried_matching_begin();
  test_firmware_receiver_aborts_on_hash_failure_disconnect_and_timeout();
  test_new_scene_applies_render_intent_but_preserves_local_hardware();
  test_duplicate_and_stale_scene_versions_do_not_reapply();
  test_independent_scene_isolated_from_follow_scene_until_follow_is_restored();
  test_independent_scene_state_survives_a_runtime_restart();
  test_independent_scene_accepts_a_new_coordinator_revision_after_restart();
  test_latest_audio_features_drive_tick_on_coordinator_clock();
  test_audio_sequence_restarts_after_transport_disconnect();
  test_node_application_resets_stream_ordering_on_disconnect();
  test_node_application_exposes_and_runs_firmware_update_protocol();
  test_manual_lit_count_is_local_output_command_not_layout_change();
  test_pairing_window_requires_runtime_long_press();
  test_boot_held_button_must_be_released_before_pairing_press();
  test_short_press_does_not_open_pairing();
  test_continuous_eight_second_hold_requests_binding_reset();
  test_node_application_keeps_transport_and_rendering_separate();
  test_failed_control_mode_persistence_restores_follow_output();
  test_node_led_count_is_saved_locally_and_survives_scene_commands();
  test_node_geometry_is_saved_applied_and_reported();
  return sozo::test::finish("C3 node runtime tests");
}

#ifdef ARDUINO
void setup() {
  Serial.begin(115200);
  delay(200U);
  runNodeRuntimeTests();
  Serial.printf("C3 node runtime failures: %d\n", sozo::test::failures);
}

void loop() {}
#else
int main(int, char **) {
  return runNodeRuntimeTests();
}
#endif
