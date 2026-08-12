#pragma once

#include <BleLinkStateMachine.h>
#include <BleOperationSupervisor.h>
#include <BleOutboundMailbox.h>
#include <NimBLEDevice.h>
#include <NodeTransport.h>
#include <SozoBleContract.h>
#include <SozoNodeMessages.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

namespace sozo {

class BleCentralAdapter final : public NodeTransport,
                                private NimBLEClientCallbacks {
 public:
  static constexpr size_t kInboundQueueCapacity = 8;

  bool begin() override;
  void tick(uint32_t nowMs) override;
  bool send(const node::Envelope &envelope) override;
  bool popInbound(node::Envelope &envelope) override;

  bool ready() const override;
  NodeTransportState state() const override;
  node::NodeId remoteNodeId() const override;
  const node::CapabilitiesPayload &capabilities() const override;
  uint32_t readyGeneration() const override;
  uint32_t droppedPackets() const override;
  const char *operationName() const override;
  bool workerBusy() const override;
  uint32_t timeoutCount() const override;

  BleOperationStage operationStage() const;
  bool connect(uint64_t address, uint8_t addressType, uint32_t nowMs);
  bool assigned() const;
  uint64_t peerAddress() const;
  bool release();

 private:
  enum class WorkerCommandType : uint8_t { Connect = 0 };
  enum class WorkerEventType : uint8_t {
    Connected = 0,
    ServicesDiscovered,
    Ready,
    Failed,
    Disconnected,
  };

  struct Candidate {
    uint64_t address{0};
    uint8_t addressType{0};
  };

  struct WorkerCommand {
    WorkerCommandType type{WorkerCommandType::Connect};
    uint32_t attemptId{0};
    Candidate candidate{};
  };

  struct WorkerEvent {
    WorkerEventType type{WorkerEventType::Failed};
    uint32_t attemptId{0};
    node::NodeId nodeId{0};
    node::CapabilitiesPayload capabilities{};
  };

  void onConnect(NimBLEClient *client) override;
  void onDisconnect(NimBLEClient *client, int reason) override;
  void onAuthenticationComplete(NimBLEConnInfo &connection) override;

  static void onNotification(NimBLERemoteCharacteristic *characteristic,
                             uint8_t *data, size_t length, bool isNotify);
  static BleCentralAdapter *ownerOf(
      NimBLERemoteCharacteristic *characteristic);
  static void workerEntry(void *context);

  void workerLoop();
  void runConnectAttempt(const WorkerCommand &command);
  bool discoverSecureAndRead(uint32_t attemptId,
                             node::NodeId &remoteNodeId,
                             node::CapabilitiesPayload &capabilities);
  bool writeEnvelope(const node::Envelope &envelope, uint32_t attemptId);
  bool popOutbound(node::Envelope &envelope);
  void clearOutbound();
  void postWorkerEvent(const WorkerEvent &event, TickType_t waitTicks = 0);
  void drainWorkerEvents(uint32_t nowMs);
  void observeWorkerStage(uint32_t nowMs);
  void abortTimedOutOperation(uint32_t nowMs);
  void launchConnect(const Candidate &candidate, uint32_t nowMs);
  void enterBackoff(uint32_t nowMs, BleLinkEvent event);
  void clearConnectionState();
  void clearWorkerConnectionPointers();

  void setOperationStage(BleOperationStage stage);
  void setWorkerBusy(bool busy, uint32_t attemptId = 0);
  void cancelWorkerAttempt(uint32_t attemptId);
  bool workerAttemptActive(uint32_t attemptId) const;
  uint32_t connectionAttemptId() const;
  NimBLERemoteCharacteristic *eventCharacteristic() const;
  void incrementDroppedPackets();

  static constexpr size_t kMaxAdapterInstances = 4;
  static constexpr UBaseType_t kWorkerEventQueueCapacity = 12;
  static BleCentralAdapter *instances_[kMaxAdapterInstances];

  BleLinkStateMachine link_{};
  BleOperationSupervisor operationSupervisor_{};
  BleAttemptGate attemptGate_{};
  BleOutboundMailbox outbound_{};
  NimBLEClient *client_{nullptr};
  NimBLERemoteCharacteristic *controlCharacteristic_{nullptr};
  NimBLERemoteCharacteristic *eventCharacteristic_{nullptr};
  NimBLERemoteCharacteristic *infoCharacteristic_{nullptr};
  QueueHandle_t workerCommandQueue_{nullptr};
  QueueHandle_t workerEventQueue_{nullptr};
  QueueHandle_t inboundQueue_{nullptr};
  // NimBLE reserves task notifications for synchronous client operations.
  SemaphoreHandle_t workerWakeSemaphore_{nullptr};
  TaskHandle_t workerTask_{nullptr};
  node::CapabilitiesPayload capabilities_{};
  node::NodeId remoteNodeId_{0};
  Candidate assignedCandidate_{};
  uint32_t retryAtMs_{0};
  uint32_t readyGeneration_{0};
  uint32_t droppedPackets_{0};
  uint32_t activeWorkerAttemptId_{0};
  uint32_t connectedAttemptId_{0};
  BleOperationStage operationStage_{BleOperationStage::Idle};
  BleOperationStage observedOperationStage_{BleOperationStage::Idle};
  bool assigned_{false};
  bool initialized_{false};
  bool authenticated_{false};
  bool workerBusy_{false};
  bool workerAttemptCancelled_{true};
  mutable portMUX_TYPE stateMux_ = portMUX_INITIALIZER_UNLOCKED;
  mutable portMUX_TYPE outboundMux_ = portMUX_INITIALIZER_UNLOCKED;
};

}  // namespace sozo
