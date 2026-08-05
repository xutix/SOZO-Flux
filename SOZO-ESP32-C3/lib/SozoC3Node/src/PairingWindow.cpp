#include <PairingWindow.h>

namespace sozo::c3 {

PairingWindow::PairingWindow(const uint32_t holdDurationMs,
                             const uint32_t windowDurationMs,
                             const uint32_t clearBindingHoldDurationMs)
    : holdDurationMs_(holdDurationMs),
      windowDurationMs_(windowDurationMs),
      clearBindingHoldDurationMs_(clearBindingHoldDurationMs) {}

void PairingWindow::begin(const bool buttonPressedAtBoot,
                          const uint32_t nowMs) {
  armed_ = !buttonPressedAtBoot;
  pressActive_ = false;
  pressTriggered_ = false;
  clearTriggered_ = false;
  open_ = false;
  pressStartedMs_ = nowMs;
  windowOpenedMs_ = nowMs;
}

PairingEvent PairingWindow::tick(const bool buttonPressed,
                                 const uint32_t nowMs) {
  if (open_ &&
      static_cast<uint32_t>(nowMs - windowOpenedMs_) >= windowDurationMs_) {
    close();
    return PairingEvent::Closed;
  }

  if (!armed_) {
    if (!buttonPressed) armed_ = true;
    return PairingEvent::None;
  }

  if (!buttonPressed) {
    pressActive_ = false;
    pressTriggered_ = false;
    clearTriggered_ = false;
    return PairingEvent::None;
  }

  if (!pressActive_) {
    pressActive_ = true;
    pressStartedMs_ = nowMs;
    return PairingEvent::None;
  }

  if (!open_ && !pressTriggered_ &&
      static_cast<uint32_t>(nowMs - pressStartedMs_) >= holdDurationMs_) {
    open_ = true;
    pressTriggered_ = true;
    windowOpenedMs_ = nowMs;
    return PairingEvent::Opened;
  }
  if (!clearTriggered_ &&
      static_cast<uint32_t>(nowMs - pressStartedMs_) >=
          clearBindingHoldDurationMs_) {
    clearTriggered_ = true;
    close();
    return PairingEvent::ClearBindingRequested;
  }
  return PairingEvent::None;
}

void PairingWindow::close() { open_ = false; }

bool PairingWindow::isOpen() const { return open_; }

}  // namespace sozo::c3
