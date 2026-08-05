#pragma once

#include <NodePlatformPorts.h>

namespace sozo::c3 {

class C3BootButton final : public NodeButtonInput {
 public:
  explicit C3BootButton(uint8_t pin);

  void begin();
  bool pressed() const override;

 private:
  uint8_t pin_{0};
};

class C3RuntimeDiagnostics final : public NodeDiagnosticsPort {
 public:
  uint32_t freeHeapBytes() const override;
};

}  // namespace sozo::c3
