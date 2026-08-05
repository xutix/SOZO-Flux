#pragma once

#include <NimBLEDevice.h>
#include <NodeTransportPort.h>
#include <PairingWindow.h>
#include <SozoBleContract.h>
#include <SozoNodeMessages.h>

namespace sozo::c3 {

class BlePeripheralAdapter final : public NodeTransportPort,
                                   private NimBLEServerCallbacks,
                                   private NimBLECharacteristicCallbacks {
 public:
  static constexpr size_t kInboundQueueCapacity = 4;

  explicit BlePeripheralAdapter(PairingWindow &pairingWindow);

  bool begin(node::NodeId nodeId,
             const node::CapabilitiesPayload &capabilities, bool bound,
             node::NodeId coordinatorNodeId,
             uint64_t boundPeerIdentityAddress,
             uint8_t boundPeerIdentityAddressType) override;
  void handlePairingEvent(PairingEvent event) override;
  bool popInbound(node::Envelope &envelope) override;
  bool send(const node::Envelope &envelope) override;
  bool bindCoordinator(node::NodeId coordinatorNodeId) override;
  void clearBinding() override;

  bool connected() const override;
  bool authenticated() const;
  bool bound() const;
  node::NodeId coordinatorNodeId() const;
  uint64_t connectedPeerIdentityAddress() const override;
  uint8_t connectedPeerIdentityAddressType() const override;
  uint32_t droppedPackets() const;

 private:
  void onConnect(NimBLEServer *server, NimBLEConnInfo &connection) override;
  void onDisconnect(NimBLEServer *server, NimBLEConnInfo &connection,
                    int reason) override;
  void onAuthenticationComplete(NimBLEConnInfo &connection) override;
  void onWrite(NimBLECharacteristic *characteristic,
               NimBLEConnInfo &connection) override;

  void startAdvertising();
  void stopAdvertisingIfUnbound();
  bool updateInfoCharacteristic();
  bool enqueue(const node::Envelope &envelope);
  bool accepts(const node::Envelope &envelope) const;

  PairingWindow &pairingWindow_;
  NimBLEServer *server_{nullptr};
  NimBLECharacteristic *controlCharacteristic_{nullptr};
  NimBLECharacteristic *eventCharacteristic_{nullptr};
  NimBLECharacteristic *infoCharacteristic_{nullptr};
  node::Envelope inbound_[kInboundQueueCapacity]{};
  size_t inboundHead_{0};
  size_t inboundTail_{0};
  size_t inboundCount_{0};
  node::NodeId nodeId_{0};
  node::NodeId coordinatorNodeId_{0};
  node::CapabilitiesPayload capabilities_{};
  uint64_t boundPeerIdentityAddress_{0};
  uint64_t connectedPeerIdentityAddress_{0};
  uint8_t boundPeerIdentityAddressType_{0};
  uint8_t connectedPeerIdentityAddressType_{0};
  uint32_t droppedPackets_{0};
  bool connected_{false};
  bool authenticated_{false};
  bool bound_{false};
  bool initialized_{false};
};

}  // namespace sozo::c3
