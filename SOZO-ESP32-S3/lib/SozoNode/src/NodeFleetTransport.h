#pragma once

#include <stddef.h>
#include <stdint.h>

#include <NodeTransport.h>

namespace sozo {

class NodeFleetTransport {
 public:
  virtual ~NodeFleetTransport() = default;

  virtual bool begin() = 0;
  virtual void tick(uint32_t nowMs) = 0;
  virtual size_t capacity() const = 0;
  virtual NodeTransport *linkAt(size_t index) = 0;
  virtual const NodeTransport *linkAt(size_t index) const = 0;
  virtual bool releaseLink(size_t index) = 0;

  virtual bool openPairingWindow(uint32_t nowMs, uint32_t durationMs) = 0;
  virtual bool pairingWindowOpen(uint32_t nowMs) const = 0;
  virtual uint32_t pairingRemainingMs(uint32_t nowMs) const = 0;
  virtual bool scanning() const = 0;
};

}  // namespace sozo
