#pragma once

#include <stdint.h>

namespace sozo {

enum class BleLinkState : uint8_t {
  Idle = 0,
  Scanning,
  Connecting,
  Discovering,
  Authenticating,
  Ready,
  Backoff,
};

enum class BleLinkEvent : uint8_t {
  StartScan = 0,
  DeviceFound,
  Connected,
  ServicesDiscovered,
  Authenticated,
  Failed,
  OperationTimedOut,
  Disconnected,
  RetryDue,
};

class BleLinkStateMachine {
 public:
  bool handle(BleLinkEvent event);
  BleLinkState state() const;
  bool requiresWorker() const;
  void reset();

 private:
  BleLinkState state_{BleLinkState::Idle};
};

}  // namespace sozo
