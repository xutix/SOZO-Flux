#pragma once

#include <SozoNodeMessages.h>

namespace sozo {

enum class NodeTransportState : uint8_t {
  Idle = 0,
  Searching,
  Connecting,
  Discovering,
  Authenticating,
  Ready,
  Backoff,
};

class NodeTransport {
 public:
  virtual ~NodeTransport() = default;

  virtual bool begin() = 0;
  virtual void tick(uint32_t nowMs) = 0;
  virtual bool send(const node::Envelope &envelope) = 0;
  virtual bool popInbound(node::Envelope &envelope) = 0;

  virtual bool ready() const = 0;
  virtual NodeTransportState state() const = 0;
  virtual node::NodeId remoteNodeId() const = 0;
  virtual const node::CapabilitiesPayload &capabilities() const = 0;
  virtual uint32_t readyGeneration() const = 0;
  virtual uint32_t droppedPackets() const = 0;
  virtual const char *operationName() const = 0;
  virtual bool workerBusy() const = 0;
  virtual uint32_t timeoutCount() const = 0;
};

}  // namespace sozo
