#pragma once

#include <stdint.h>

namespace sozo::c3 {

class NodeButtonInput {
 public:
  virtual ~NodeButtonInput() = default;
  virtual bool pressed() const = 0;
};

class NodeDiagnosticsPort {
 public:
  virtual ~NodeDiagnosticsPort() = default;
  virtual uint32_t freeHeapBytes() const = 0;
};

}  // namespace sozo::c3
