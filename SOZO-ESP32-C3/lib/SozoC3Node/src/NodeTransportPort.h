#pragma once

#include <NodeBindingPort.h>
#include <PairingWindow.h>
#include <SozoNodeMessages.h>

namespace sozo::c3 {

class NodeTransportPort {
 public:
  virtual ~NodeTransportPort() = default;

  virtual bool begin(node::NodeId nodeId,
                     const node::CapabilitiesPayload &capabilities,
                     bool bound, node::NodeId coordinatorNodeId,
                     uint64_t boundPeerIdentityAddress,
                     uint8_t boundPeerIdentityAddressType) = 0;
  virtual void handlePairingEvent(PairingEvent event) = 0;
  virtual bool connected() const = 0;
  virtual bool popInbound(node::Envelope &envelope) = 0;
  virtual bool send(const node::Envelope &envelope) = 0;
  virtual bool bindCoordinator(node::NodeId coordinatorNodeId) = 0;
  virtual void clearBinding() = 0;
  virtual uint64_t connectedPeerIdentityAddress() const = 0;
  virtual uint8_t connectedPeerIdentityAddressType() const = 0;
};

}  // namespace sozo::c3
