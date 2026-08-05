#include <BlePeripheralAdapter.h>

#include <Arduino.h>

namespace sozo::c3 {

BlePeripheralAdapter::BlePeripheralAdapter(PairingWindow &pairingWindow)
    : pairingWindow_(pairingWindow) {}

bool BlePeripheralAdapter::begin(
    const node::NodeId nodeId,
    const node::CapabilitiesPayload &capabilities, const bool bound,
    const node::NodeId coordinatorNodeId,
    const uint64_t boundPeerIdentityAddress,
    const uint8_t boundPeerIdentityAddressType) {
  if (initialized_ || nodeId == 0) return false;
  if (bound && (coordinatorNodeId == 0 || boundPeerIdentityAddress == 0)) {
    return false;
  }
  nodeId_ = nodeId;
  bound_ = bound;
  coordinatorNodeId_ = bound ? coordinatorNodeId : 0;
  boundPeerIdentityAddress_ = bound ? boundPeerIdentityAddress : 0;
  boundPeerIdentityAddressType_ =
      bound ? boundPeerIdentityAddressType : 0;
  capabilities_ = capabilities;
  capabilities_.bound = bound_;

  char deviceName[24]{};
  snprintf(deviceName, sizeof(deviceName), "SOZO-C3-%04lX",
           static_cast<unsigned long>(nodeId_ & 0xFFFFU));
  if (!NimBLEDevice::init(deviceName)) return false;
  NimBLEDevice::setMTU(ble::kPreferredMtu);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  NimBLEDevice::setSecurityAuth(true, false, true);

  server_ = NimBLEDevice::createServer();
  if (server_ == nullptr) return false;
  server_->setCallbacks(this);

  NimBLEService *service = server_->createService(ble::kServiceUuid);
  if (service == nullptr) return false;
  controlCharacteristic_ = service->createCharacteristic(
      ble::kControlCharacteristicUuid,
      NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR |
          NIMBLE_PROPERTY::WRITE_ENC,
      node::kMaxPacketBytes);
  eventCharacteristic_ = service->createCharacteristic(
      ble::kEventCharacteristicUuid,
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC |
          NIMBLE_PROPERTY::NOTIFY,
      node::kMaxPacketBytes);
  infoCharacteristic_ = service->createCharacteristic(
      ble::kInfoCharacteristicUuid,
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::READ_ENC,
      node::kMaxPacketBytes);
  if (controlCharacteristic_ == nullptr || eventCharacteristic_ == nullptr ||
      infoCharacteristic_ == nullptr) {
    return false;
  }
  controlCharacteristic_->setCallbacks(this);

  if (!updateInfoCharacteristic()) return false;

  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  advertising->setName(deviceName);
  advertising->addServiceUUID(ble::kServiceUuid);
  advertising->enableScanResponse(true);
  if (bound_) {
    const NimBLEAddress peer(boundPeerIdentityAddress_,
                             boundPeerIdentityAddressType_);
    if (!NimBLEDevice::whiteListAdd(peer)) return false;
    advertising->setScanFilter(false, true);
  } else {
    advertising->setScanFilter(false, false);
  }
  initialized_ = true;
  if (bound_ || pairingWindow_.isOpen()) startAdvertising();
  return true;
}

void BlePeripheralAdapter::handlePairingEvent(const PairingEvent event) {
  if (!initialized_) return;
  if (event == PairingEvent::Opened) {
    if (!bound_) NimBLEDevice::getAdvertising()->setScanFilter(false, false);
    startAdvertising();
  } else if (event == PairingEvent::Closed) {
    stopAdvertisingIfUnbound();
  }
}

bool BlePeripheralAdapter::popInbound(node::Envelope &envelope) {
  if (inboundCount_ == 0) return false;
  envelope = inbound_[inboundHead_];
  inboundHead_ = (inboundHead_ + 1U) % kInboundQueueCapacity;
  --inboundCount_;
  return true;
}

bool BlePeripheralAdapter::send(const node::Envelope &envelope) {
  if (!connected_ || !authenticated_ || eventCharacteristic_ == nullptr) {
    return false;
  }
  uint8_t encoded[node::kMaxPacketBytes]{};
  size_t encodedLength = 0;
  if (node::encodeEnvelope(envelope, encoded, sizeof(encoded), encodedLength) !=
      node::CodecResult::Ok) {
    return false;
  }
  return eventCharacteristic_->notify(encoded, encodedLength);
}

bool BlePeripheralAdapter::bindCoordinator(
    const node::NodeId coordinatorNodeId) {
  if (coordinatorNodeId == 0 ||
      coordinatorNodeId == node::kBroadcastNodeId ||
      connectedPeerIdentityAddress_ == 0) {
    return false;
  }
  const NimBLEAddress peer(connectedPeerIdentityAddress_,
                           connectedPeerIdentityAddressType_);
  if (!NimBLEDevice::whiteListAdd(peer)) return false;
  coordinatorNodeId_ = coordinatorNodeId;
  boundPeerIdentityAddress_ = connectedPeerIdentityAddress_;
  boundPeerIdentityAddressType_ = connectedPeerIdentityAddressType_;
  bound_ = true;
  capabilities_.bound = true;
  updateInfoCharacteristic();
  NimBLEDevice::getAdvertising()->setScanFilter(false, true);
  pairingWindow_.close();
  return true;
}

void BlePeripheralAdapter::clearBinding() {
  if (boundPeerIdentityAddress_ != 0) {
    NimBLEDevice::whiteListRemove(
        NimBLEAddress(boundPeerIdentityAddress_, boundPeerIdentityAddressType_));
  }
  NimBLEDevice::deleteAllBonds();
  coordinatorNodeId_ = 0;
  boundPeerIdentityAddress_ = 0;
  boundPeerIdentityAddressType_ = 0;
  bound_ = false;
  capabilities_.bound = false;
  updateInfoCharacteristic();
  NimBLEDevice::getAdvertising()->setScanFilter(false, false);
  pairingWindow_.close();
}

bool BlePeripheralAdapter::connected() const { return connected_; }
bool BlePeripheralAdapter::authenticated() const { return authenticated_; }
bool BlePeripheralAdapter::bound() const { return bound_; }
node::NodeId BlePeripheralAdapter::coordinatorNodeId() const {
  return coordinatorNodeId_;
}
uint64_t BlePeripheralAdapter::connectedPeerIdentityAddress() const {
  return connectedPeerIdentityAddress_;
}
uint8_t BlePeripheralAdapter::connectedPeerIdentityAddressType() const {
  return connectedPeerIdentityAddressType_;
}
uint32_t BlePeripheralAdapter::droppedPackets() const {
  return droppedPackets_;
}

void BlePeripheralAdapter::onConnect(NimBLEServer *,
                                     NimBLEConnInfo &connection) {
  connected_ = true;
  authenticated_ = connection.isEncrypted();
}

void BlePeripheralAdapter::onDisconnect(NimBLEServer *, NimBLEConnInfo &,
                                        int) {
  connected_ = false;
  authenticated_ = false;
  connectedPeerIdentityAddress_ = 0;
  connectedPeerIdentityAddressType_ = 0;
  if (bound_ || pairingWindow_.isOpen()) startAdvertising();
}

void BlePeripheralAdapter::onAuthenticationComplete(
    NimBLEConnInfo &connection) {
  authenticated_ = connection.isEncrypted();
  if (authenticated_) {
    const NimBLEAddress identity = connection.getIdAddress();
    connectedPeerIdentityAddress_ = static_cast<uint64_t>(identity);
    connectedPeerIdentityAddressType_ = identity.getType();
  }
  if (!authenticated_ && server_ != nullptr) {
    server_->disconnect(connection.getConnHandle());
  }
}

void BlePeripheralAdapter::onWrite(NimBLECharacteristic *characteristic,
                                   NimBLEConnInfo &connection) {
  if (characteristic != controlCharacteristic_ || !connection.isEncrypted()) {
    return;
  }
  const NimBLEAttValue value = characteristic->getValue();
  node::Envelope envelope{};
  if (node::decodeEnvelope(value.data(), value.size(), envelope) !=
          node::CodecResult::Ok ||
      !accepts(envelope)) {
    ++droppedPackets_;
    return;
  }
  enqueue(envelope);
}

void BlePeripheralAdapter::startAdvertising() {
  if (!NimBLEDevice::getAdvertising()->isAdvertising()) {
    NimBLEDevice::startAdvertising();
  }
}

void BlePeripheralAdapter::stopAdvertisingIfUnbound() {
  if (!bound_ && !connected_) NimBLEDevice::stopAdvertising();
}

bool BlePeripheralAdapter::updateInfoCharacteristic() {
  if (infoCharacteristic_ == nullptr) return false;
  node::Envelope info{};
  info.messageType = node::MessageType::Capabilities;
  info.channelId = static_cast<uint16_t>(node::TopicId::NodeCapabilities);
  info.sourceNodeId = nodeId_;
  info.targetNodeId = node::kCoordinatorNodeId;
  if (node::writeCapabilities(info, capabilities_) != node::CodecResult::Ok) {
    return false;
  }
  uint8_t encoded[node::kMaxPacketBytes]{};
  size_t encodedLength = 0;
  if (node::encodeEnvelope(info, encoded, sizeof(encoded), encodedLength) !=
      node::CodecResult::Ok) {
    return false;
  }
  infoCharacteristic_->setValue(encoded, encodedLength);
  return true;
}

bool BlePeripheralAdapter::enqueue(const node::Envelope &envelope) {
  if (inboundCount_ == kInboundQueueCapacity) {
    inboundHead_ = (inboundHead_ + 1U) % kInboundQueueCapacity;
    --inboundCount_;
    ++droppedPackets_;
  }
  inbound_[inboundTail_] = envelope;
  inboundTail_ = (inboundTail_ + 1U) % kInboundQueueCapacity;
  ++inboundCount_;
  return true;
}

bool BlePeripheralAdapter::accepts(const node::Envelope &envelope) const {
  if (envelope.targetNodeId != nodeId_ &&
      envelope.targetNodeId != node::kBroadcastNodeId) {
    return false;
  }
  if (bound_) return envelope.sourceNodeId == coordinatorNodeId_;
  return pairingWindow_.isOpen() &&
         envelope.messageType == node::MessageType::BindRequest;
}

}  // namespace sozo::c3
