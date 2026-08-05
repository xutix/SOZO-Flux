#pragma once

#include <stdint.h>

namespace sozo::c3 {

enum class PairingEvent : uint8_t {
  None = 0,
  Opened,
  Closed,
  ClearBindingRequested,
};

class PairingWindow {
 public:
  PairingWindow(uint32_t holdDurationMs, uint32_t windowDurationMs,
                uint32_t clearBindingHoldDurationMs = 8000U);

  void begin(bool buttonPressedAtBoot, uint32_t nowMs);
  PairingEvent tick(bool buttonPressed, uint32_t nowMs);
  void close();
  bool isOpen() const;

 private:
  uint32_t holdDurationMs_;
  uint32_t windowDurationMs_;
  uint32_t clearBindingHoldDurationMs_;
  uint32_t pressStartedMs_{0};
  uint32_t windowOpenedMs_{0};
  bool armed_{false};
  bool pressActive_{false};
  bool pressTriggered_{false};
  bool clearTriggered_{false};
  bool open_{false};
};

}  // namespace sozo::c3
