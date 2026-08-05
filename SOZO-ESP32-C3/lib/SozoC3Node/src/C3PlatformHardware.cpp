#include <C3PlatformHardware.h>

#include <Arduino.h>

namespace sozo::c3 {

C3BootButton::C3BootButton(const uint8_t pin) : pin_(pin) {}

void C3BootButton::begin() { pinMode(pin_, INPUT_PULLUP); }

bool C3BootButton::pressed() const { return digitalRead(pin_) == LOW; }

uint32_t C3RuntimeDiagnostics::freeHeapBytes() const {
  return ESP.getFreeHeap();
}

}  // namespace sozo::c3
