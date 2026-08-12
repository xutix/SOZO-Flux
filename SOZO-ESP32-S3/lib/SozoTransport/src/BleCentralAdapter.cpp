#include <BleCentralAdapter.h>

#include <Arduino.h>

namespace sozo {
namespace {

constexpr uint32_t kRetryDelayMs = 2000U;
constexpr uint32_t kWorkerStackBytes = 8192U;
constexpr UBaseType_t kWorkerPriority = 1;
constexpr TickType_t kWorkerPollTicks = pdMS_TO_TICKS(20);

}  // namespace

BleCentralAdapter *BleCentralAdapter::instances_[kMaxAdapterInstances]{};

bool BleCentralAdapter::begin() {
  if (initialized_) return true;

  size_t instanceIndex = kMaxAdapterInstances;
  for (size_t index = 0; index < kMaxAdapterInstances; ++index) {
    if (instances_[index] == nullptr) {
      instanceIndex = index;
      break;
    }
  }
  if (instanceIndex == kMaxAdapterInstances) return false;

  workerCommandQueue_ = xQueueCreate(1, sizeof(WorkerCommand));
  workerEventQueue_ =
      xQueueCreate(kWorkerEventQueueCapacity, sizeof(WorkerEvent));
  inboundQueue_ = xQueueCreate(kInboundQueueCapacity, sizeof(node::Envelope));
  workerWakeSemaphore_ = xSemaphoreCreateBinary();
  if (workerCommandQueue_ == nullptr || workerEventQueue_ == nullptr ||
      inboundQueue_ == nullptr || workerWakeSemaphore_ == nullptr) {
    if (workerCommandQueue_ != nullptr) vQueueDelete(workerCommandQueue_);
    if (workerEventQueue_ != nullptr) vQueueDelete(workerEventQueue_);
    if (inboundQueue_ != nullptr) vQueueDelete(inboundQueue_);
    if (workerWakeSemaphore_ != nullptr) {
      vSemaphoreDelete(workerWakeSemaphore_);
    }
    workerCommandQueue_ = nullptr;
    workerEventQueue_ = nullptr;
    inboundQueue_ = nullptr;
    workerWakeSemaphore_ = nullptr;
    return false;
  }

  if (xTaskCreate(workerEntry, "sozo-ble-worker", kWorkerStackBytes, this,
                  kWorkerPriority, &workerTask_) != pdPASS) {
    vQueueDelete(workerCommandQueue_);
    vQueueDelete(workerEventQueue_);
    vQueueDelete(inboundQueue_);
    vSemaphoreDelete(workerWakeSemaphore_);
    workerCommandQueue_ = nullptr;
    workerEventQueue_ = nullptr;
    inboundQueue_ = nullptr;
    workerWakeSemaphore_ = nullptr;
    workerTask_ = nullptr;
    return false;
  }

  instances_[instanceIndex] = this;
  initialized_ = true;
  return true;
}

void BleCentralAdapter::tick(const uint32_t nowMs) {
  if (!initialized_) return;

  drainWorkerEvents(nowMs);
  observeWorkerStage(nowMs);
  if (operationSupervisor_.tick(nowMs) == BleSupervisorAction::Abort) {
    abortTimedOutOperation(nowMs);
  }
  if (!workerBusy() && link_.requiresWorker()) {
    enterBackoff(nowMs, BleLinkEvent::Failed);
  }

  if (link_.state() == BleLinkState::Backoff && !workerBusy() &&
      static_cast<int32_t>(nowMs - retryAtMs_) >= 0) {
    link_.handle(BleLinkEvent::RetryDue);
    launchConnect(assignedCandidate_, nowMs);
  }
}

bool BleCentralAdapter::send(const node::Envelope &envelope) {
  if (!ready() || workerTask_ == nullptr || workerWakeSemaphore_ == nullptr) {
    return false;
  }
  bool accepted = false;
  portENTER_CRITICAL(&outboundMux_);
  accepted = outbound_.push(envelope);
  portEXIT_CRITICAL(&outboundMux_);
  if (accepted) xSemaphoreGive(workerWakeSemaphore_);
  return accepted;
}

bool BleCentralAdapter::popInbound(node::Envelope &envelope) {
  if (inboundQueue_ == nullptr) return false;
  while (xQueueReceive(inboundQueue_, &envelope, 0) == pdTRUE) {
    if (remoteNodeId_ == 0 || envelope.sourceNodeId == remoteNodeId_) {
      return true;
    }
    incrementDroppedPackets();
  }
  return false;
}

bool BleCentralAdapter::ready() const {
  return link_.state() == BleLinkState::Ready && authenticated_;
}

NodeTransportState BleCentralAdapter::state() const {
  switch (link_.state()) {
    case BleLinkState::Idle:
      return NodeTransportState::Idle;
    case BleLinkState::Scanning:
      return NodeTransportState::Searching;
    case BleLinkState::Connecting:
      return NodeTransportState::Connecting;
    case BleLinkState::Discovering:
      return NodeTransportState::Discovering;
    case BleLinkState::Authenticating:
      return NodeTransportState::Authenticating;
    case BleLinkState::Ready:
      return NodeTransportState::Ready;
    case BleLinkState::Backoff:
    default:
      return NodeTransportState::Backoff;
  }
}

node::NodeId BleCentralAdapter::remoteNodeId() const { return remoteNodeId_; }

const node::CapabilitiesPayload &BleCentralAdapter::capabilities() const {
  return capabilities_;
}

uint32_t BleCentralAdapter::readyGeneration() const {
  return readyGeneration_;
}

uint32_t BleCentralAdapter::droppedPackets() const {
  uint32_t dropped = 0;
  portENTER_CRITICAL(&stateMux_);
  dropped = droppedPackets_;
  portEXIT_CRITICAL(&stateMux_);
  return dropped;
}

const char *BleCentralAdapter::operationName() const {
  switch (operationStage()) {
    case BleOperationStage::Connecting:
      return "connecting";
    case BleOperationStage::Discovering:
      return "discovering";
    case BleOperationStage::Authenticating:
      return "authenticating";
    case BleOperationStage::Writing:
      return "writing";
    case BleOperationStage::Idle:
    default:
      return "idle";
  }
}

BleOperationStage BleCentralAdapter::operationStage() const {
  BleOperationStage stage = BleOperationStage::Idle;
  portENTER_CRITICAL(&stateMux_);
  stage = operationStage_;
  portEXIT_CRITICAL(&stateMux_);
  return stage;
}

bool BleCentralAdapter::workerBusy() const {
  bool busy = false;
  portENTER_CRITICAL(&stateMux_);
  busy = workerBusy_;
  portEXIT_CRITICAL(&stateMux_);
  return busy;
}

uint32_t BleCentralAdapter::timeoutCount() const {
  return operationSupervisor_.timeoutCount();
}

bool BleCentralAdapter::connect(const uint64_t address,
                                const uint8_t addressType,
                                const uint32_t nowMs) {
  if (!initialized_ || address == 0U || assigned() || workerBusy()) {
    return false;
  }
  portENTER_CRITICAL(&stateMux_);
  assignedCandidate_.address = address;
  assignedCandidate_.addressType = addressType;
  assigned_ = true;
  portEXIT_CRITICAL(&stateMux_);
  link_.reset();
  link_.handle(BleLinkEvent::StartScan);
  launchConnect(assignedCandidate_, nowMs);
  return true;
}

bool BleCentralAdapter::assigned() const {
  bool value = false;
  portENTER_CRITICAL(&stateMux_);
  value = assigned_;
  portEXIT_CRITICAL(&stateMux_);
  return value;
}

uint64_t BleCentralAdapter::peerAddress() const {
  uint64_t value = 0U;
  portENTER_CRITICAL(&stateMux_);
  value = assignedCandidate_.address;
  portEXIT_CRITICAL(&stateMux_);
  return value;
}

bool BleCentralAdapter::release() {
  if (!assigned()) return false;
  const uint32_t attemptId = attemptGate_.current();
  attemptGate_.cancelCurrent();
  cancelWorkerAttempt(attemptId);
  NimBLEClient *client = nullptr;
  portENTER_CRITICAL(&stateMux_);
  client = client_;
  assigned_ = false;
  assignedCandidate_ = Candidate{};
  portEXIT_CRITICAL(&stateMux_);
  if (client != nullptr && client->isConnected()) client->disconnect();
  clearConnectionState();
  clearOutbound();
  operationSupervisor_.complete();
  setOperationStage(BleOperationStage::Idle);
  link_.reset();
  return true;
}

void BleCentralAdapter::onConnect(NimBLEClient *) {}

void BleCentralAdapter::onDisconnect(NimBLEClient *, int) {
  WorkerEvent event{};
  event.type = WorkerEventType::Disconnected;
  event.attemptId = connectionAttemptId();
  if (event.attemptId != 0U) postWorkerEvent(event);
}

void BleCentralAdapter::onAuthenticationComplete(NimBLEConnInfo &) {}

void BleCentralAdapter::onNotification(
    NimBLERemoteCharacteristic *characteristic, uint8_t *data,
    const size_t length, bool) {
  BleCentralAdapter *owner = ownerOf(characteristic);
  if (owner == nullptr || owner->inboundQueue_ == nullptr) return;
  node::Envelope envelope{};
  if (node::decodeEnvelope(data, length, envelope) != node::CodecResult::Ok ||
      xQueueSend(owner->inboundQueue_, &envelope, 0) != pdTRUE) {
    owner->incrementDroppedPackets();
  }
}

BleCentralAdapter *BleCentralAdapter::ownerOf(
    NimBLERemoteCharacteristic *characteristic) {
  for (BleCentralAdapter *instance : instances_) {
    if (instance != nullptr &&
        instance->eventCharacteristic() == characteristic) {
      return instance;
    }
  }
  return nullptr;
}

void BleCentralAdapter::workerEntry(void *context) {
  static_cast<BleCentralAdapter *>(context)->workerLoop();
}

void BleCentralAdapter::workerLoop() {
  for (;;) {
    WorkerCommand command{};
    if (xQueueReceive(workerCommandQueue_, &command, portMAX_DELAY) != pdTRUE) {
      continue;
    }
    if (command.type == WorkerCommandType::Connect) {
      runConnectAttempt(command);
    }
  }
}

void BleCentralAdapter::runConnectAttempt(const WorkerCommand &command) {
  if (!workerAttemptActive(command.attemptId)) {
    setWorkerBusy(false);
    return;
  }

  NimBLEClient *client = nullptr;
  portENTER_CRITICAL(&stateMux_);
  client = client_;
  portEXIT_CRITICAL(&stateMux_);
  if (client == nullptr) {
    client = NimBLEDevice::createClient();
    if (client != nullptr) {
      client->setClientCallbacks(this, false);
      client->setConnectionParams(24, 48, 0, 180);
      client->setConnectTimeout(5000);
      client->setConnectRetries(0);
      portENTER_CRITICAL(&stateMux_);
      client_ = client;
      portEXIT_CRITICAL(&stateMux_);
    }
  }

  setOperationStage(BleOperationStage::Connecting);
  const NimBLEAddress address(command.candidate.address,
                              command.candidate.addressType);
  if (client == nullptr ||
      !client->connect(address, true, false, true) ||
      !workerAttemptActive(command.attemptId)) {
    WorkerEvent event{};
    event.type = WorkerEventType::Failed;
    event.attemptId = command.attemptId;
    postWorkerEvent(event, pdMS_TO_TICKS(20));
    if (client != nullptr && client->isConnected()) client->disconnect();
    clearWorkerConnectionPointers();
    setOperationStage(BleOperationStage::Idle);
    setWorkerBusy(false);
    return;
  }

  portENTER_CRITICAL(&stateMux_);
  connectedAttemptId_ = command.attemptId;
  portEXIT_CRITICAL(&stateMux_);

  WorkerEvent event{};
  event.type = WorkerEventType::Connected;
  event.attemptId = command.attemptId;
  postWorkerEvent(event, pdMS_TO_TICKS(20));

  node::NodeId remoteNodeId = 0;
  node::CapabilitiesPayload capabilities{};
  if (!discoverSecureAndRead(command.attemptId, remoteNodeId, capabilities) ||
      !workerAttemptActive(command.attemptId)) {
    event.type = WorkerEventType::Failed;
    postWorkerEvent(event, pdMS_TO_TICKS(20));
    if (client->isConnected()) client->disconnect();
    clearWorkerConnectionPointers();
    setOperationStage(BleOperationStage::Idle);
    setWorkerBusy(false);
    return;
  }

  event.type = WorkerEventType::Ready;
  event.nodeId = remoteNodeId;
  event.capabilities = capabilities;
  postWorkerEvent(event, pdMS_TO_TICKS(20));
  setOperationStage(BleOperationStage::Idle);

  while (client->isConnected() && workerAttemptActive(command.attemptId)) {
    node::Envelope envelope{};
    if (!popOutbound(envelope)) {
      xSemaphoreTake(workerWakeSemaphore_, kWorkerPollTicks);
      continue;
    }
    if (!writeEnvelope(envelope, command.attemptId)) {
      event.type = WorkerEventType::Failed;
      postWorkerEvent(event, pdMS_TO_TICKS(20));
      if (client->isConnected()) client->disconnect();
      break;
    }
  }

  if (!client->isConnected() && workerAttemptActive(command.attemptId)) {
    event.type = WorkerEventType::Disconnected;
    postWorkerEvent(event, pdMS_TO_TICKS(20));
  }
  clearWorkerConnectionPointers();
  clearOutbound();
  setOperationStage(BleOperationStage::Idle);
  setWorkerBusy(false);
}

bool BleCentralAdapter::discoverSecureAndRead(
    const uint32_t attemptId, node::NodeId &remoteNodeId,
    node::CapabilitiesPayload &capabilities) {
  setOperationStage(BleOperationStage::Discovering);
  NimBLERemoteService *service = client_->getService(ble::kServiceUuid);
  if (service == nullptr || !workerAttemptActive(attemptId)) return false;

  NimBLERemoteCharacteristic *control =
      service->getCharacteristic(ble::kControlCharacteristicUuid);
  NimBLERemoteCharacteristic *event =
      service->getCharacteristic(ble::kEventCharacteristicUuid);
  NimBLERemoteCharacteristic *info =
      service->getCharacteristic(ble::kInfoCharacteristicUuid);
  if (control == nullptr || event == nullptr || info == nullptr ||
      !event->canNotify() || !workerAttemptActive(attemptId)) {
    return false;
  }

  portENTER_CRITICAL(&stateMux_);
  controlCharacteristic_ = control;
  eventCharacteristic_ = event;
  infoCharacteristic_ = info;
  portEXIT_CRITICAL(&stateMux_);

  WorkerEvent lifecycle{};
  lifecycle.type = WorkerEventType::ServicesDiscovered;
  lifecycle.attemptId = attemptId;
  postWorkerEvent(lifecycle, pdMS_TO_TICKS(20));

  setOperationStage(BleOperationStage::Authenticating);
  if (!workerAttemptActive(attemptId) ||
      (!client_->getConnInfo().isEncrypted() &&
       !client_->secureConnection(false)) ||
      !client_->getConnInfo().isEncrypted() ||
      !workerAttemptActive(attemptId)) {
    return false;
  }

  if (!event->subscribe(true, onNotification, true) ||
      !workerAttemptActive(attemptId)) {
    return false;
  }

  const NimBLEAttValue value = info->readValue();
  node::Envelope envelope{};
  if (!workerAttemptActive(attemptId) ||
      node::decodeEnvelope(value.data(), value.size(), envelope) !=
          node::CodecResult::Ok ||
      node::readCapabilities(envelope, capabilities) != node::CodecResult::Ok ||
      envelope.sourceNodeId == 0U) {
    return false;
  }
  remoteNodeId = envelope.sourceNodeId;
  return true;
}

bool BleCentralAdapter::writeEnvelope(const node::Envelope &envelope,
                                      const uint32_t attemptId) {
  uint8_t encoded[node::kMaxPacketBytes]{};
  size_t encodedLength = 0;
  if (node::encodeEnvelope(envelope, encoded, sizeof(encoded), encodedLength) !=
          node::CodecResult::Ok ||
      !workerAttemptActive(attemptId)) {
    return false;
  }

  const bool requireResponse =
      envelope.messageType != node::MessageType::AudioFeatures;
  if (requireResponse) setOperationStage(BleOperationStage::Writing);
  NimBLERemoteCharacteristic *control = nullptr;
  portENTER_CRITICAL(&stateMux_);
  control = controlCharacteristic_;
  portEXIT_CRITICAL(&stateMux_);
  const bool written = control != nullptr &&
                       control->writeValue(encoded, encodedLength,
                                           requireResponse);
  if (requireResponse) setOperationStage(BleOperationStage::Idle);
  return written && workerAttemptActive(attemptId);
}

bool BleCentralAdapter::popOutbound(node::Envelope &envelope) {
  bool available = false;
  portENTER_CRITICAL(&outboundMux_);
  available = outbound_.pop(envelope);
  portEXIT_CRITICAL(&outboundMux_);
  return available;
}

void BleCentralAdapter::clearOutbound() {
  portENTER_CRITICAL(&outboundMux_);
  outbound_.clear();
  portEXIT_CRITICAL(&outboundMux_);
}

void BleCentralAdapter::postWorkerEvent(const WorkerEvent &event,
                                        const TickType_t waitTicks) {
  if (workerEventQueue_ == nullptr) return;
  if (xQueueSend(workerEventQueue_, &event, waitTicks) != pdTRUE) {
    incrementDroppedPackets();
  }
}

void BleCentralAdapter::drainWorkerEvents(const uint32_t nowMs) {
  WorkerEvent event{};
  size_t drained = 0;
  while (drained < kWorkerEventQueueCapacity &&
         xQueueReceive(workerEventQueue_, &event, 0) == pdTRUE) {
    ++drained;
    if (!attemptGate_.accepts(event.attemptId)) continue;

    switch (event.type) {
      case WorkerEventType::Connected:
        if (!link_.handle(BleLinkEvent::Connected)) {
          enterBackoff(nowMs, BleLinkEvent::Failed);
        }
        break;
      case WorkerEventType::ServicesDiscovered:
        if (!link_.handle(BleLinkEvent::ServicesDiscovered)) {
          enterBackoff(nowMs, BleLinkEvent::Failed);
        }
        break;
      case WorkerEventType::Ready:
        if (!link_.handle(BleLinkEvent::Authenticated)) {
          enterBackoff(nowMs, BleLinkEvent::Failed);
          break;
        }
        remoteNodeId_ = event.nodeId;
        capabilities_ = event.capabilities;
        authenticated_ = true;
        ++readyGeneration_;
        if (readyGeneration_ == 0U) ++readyGeneration_;
        break;
      case WorkerEventType::Failed:
        enterBackoff(nowMs, BleLinkEvent::Failed);
        break;
      case WorkerEventType::Disconnected:
        enterBackoff(nowMs, BleLinkEvent::Disconnected);
        break;
    }
  }
}

void BleCentralAdapter::observeWorkerStage(const uint32_t nowMs) {
  const BleOperationStage stage = operationStage();
  if (stage == observedOperationStage_) return;
  observedOperationStage_ = stage;
  if (stage == BleOperationStage::Idle) {
    operationSupervisor_.complete();
  } else {
    operationSupervisor_.start(stage, nowMs);
  }
}

void BleCentralAdapter::abortTimedOutOperation(const uint32_t nowMs) {
  const BleOperationStage timedOutStage = operationSupervisor_.stage();
  const uint32_t attemptId = attemptGate_.current();
  attemptGate_.cancelCurrent();
  cancelWorkerAttempt(attemptId);
  clearConnectionState();
  clearOutbound();

  NimBLEClient *client = nullptr;
  portENTER_CRITICAL(&stateMux_);
  client = client_;
  portEXIT_CRITICAL(&stateMux_);
  if (client != nullptr) {
    if (timedOutStage == BleOperationStage::Connecting) {
      client->cancelConnect();
    } else {
      client->disconnect();
    }
  }

  enterBackoff(nowMs, BleLinkEvent::OperationTimedOut);
}

void BleCentralAdapter::launchConnect(const Candidate &candidate,
                                      const uint32_t nowMs) {
  if (!link_.handle(BleLinkEvent::DeviceFound)) return;
  const uint32_t attemptId = attemptGate_.beginAttempt();
  WorkerCommand command{};
  command.attemptId = attemptId;
  command.candidate = candidate;
  setWorkerBusy(true, attemptId);
  setOperationStage(BleOperationStage::Connecting);
  observedOperationStage_ = BleOperationStage::Connecting;
  operationSupervisor_.start(BleOperationStage::Connecting, nowMs);
  if (xQueueSend(workerCommandQueue_, &command, 0) != pdTRUE) {
    attemptGate_.cancelCurrent();
    cancelWorkerAttempt(attemptId);
    setOperationStage(BleOperationStage::Idle);
    setWorkerBusy(false);
    enterBackoff(nowMs, BleLinkEvent::Failed);
  }
}

void BleCentralAdapter::enterBackoff(const uint32_t nowMs,
                                     const BleLinkEvent event) {
  attemptGate_.cancelCurrent();
  cancelWorkerAttempt(attemptGate_.current());
  clearConnectionState();
  clearOutbound();
  if (link_.state() != BleLinkState::Backoff) link_.handle(event);
  retryAtMs_ = nowMs + kRetryDelayMs;
}

void BleCentralAdapter::clearConnectionState() {
  clearWorkerConnectionPointers();
  authenticated_ = false;
  remoteNodeId_ = 0;
  capabilities_ = node::CapabilitiesPayload{};
}

void BleCentralAdapter::clearWorkerConnectionPointers() {
  portENTER_CRITICAL(&stateMux_);
  controlCharacteristic_ = nullptr;
  eventCharacteristic_ = nullptr;
  infoCharacteristic_ = nullptr;
  connectedAttemptId_ = 0;
  portEXIT_CRITICAL(&stateMux_);
}

void BleCentralAdapter::setOperationStage(const BleOperationStage stage) {
  portENTER_CRITICAL(&stateMux_);
  operationStage_ = stage;
  portEXIT_CRITICAL(&stateMux_);
}

void BleCentralAdapter::setWorkerBusy(const bool busy,
                                      const uint32_t attemptId) {
  portENTER_CRITICAL(&stateMux_);
  workerBusy_ = busy;
  if (busy) {
    activeWorkerAttemptId_ = attemptId;
    workerAttemptCancelled_ = false;
  } else {
    activeWorkerAttemptId_ = 0;
    workerAttemptCancelled_ = true;
  }
  portEXIT_CRITICAL(&stateMux_);
}

void BleCentralAdapter::cancelWorkerAttempt(const uint32_t attemptId) {
  portENTER_CRITICAL(&stateMux_);
  if (activeWorkerAttemptId_ == attemptId) workerAttemptCancelled_ = true;
  portEXIT_CRITICAL(&stateMux_);
  if (workerWakeSemaphore_ != nullptr) xSemaphoreGive(workerWakeSemaphore_);
}

bool BleCentralAdapter::workerAttemptActive(const uint32_t attemptId) const {
  bool active = false;
  portENTER_CRITICAL(&stateMux_);
  active = workerBusy_ && !workerAttemptCancelled_ &&
           activeWorkerAttemptId_ == attemptId;
  portEXIT_CRITICAL(&stateMux_);
  return active;
}

uint32_t BleCentralAdapter::connectionAttemptId() const {
  uint32_t attemptId = 0;
  portENTER_CRITICAL(&stateMux_);
  attemptId = connectedAttemptId_;
  portEXIT_CRITICAL(&stateMux_);
  return attemptId;
}

NimBLERemoteCharacteristic *BleCentralAdapter::eventCharacteristic() const {
  NimBLERemoteCharacteristic *characteristic = nullptr;
  portENTER_CRITICAL(&stateMux_);
  characteristic = eventCharacteristic_;
  portEXIT_CRITICAL(&stateMux_);
  return characteristic;
}

void BleCentralAdapter::incrementDroppedPackets() {
  portENTER_CRITICAL(&stateMux_);
  ++droppedPackets_;
  portEXIT_CRITICAL(&stateMux_);
}

}  // namespace sozo
