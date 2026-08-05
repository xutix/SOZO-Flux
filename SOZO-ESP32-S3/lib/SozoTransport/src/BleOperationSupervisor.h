#pragma once

#include <stdint.h>

namespace sozo {

enum class BleOperationStage : uint8_t {
  Idle = 0,
  Connecting,
  Discovering,
  Authenticating,
  Writing,
};

enum class BleSupervisorAction : uint8_t {
  None = 0,
  Abort,
};

class BleOperationSupervisor {
 public:
  void start(BleOperationStage stage, uint32_t nowMs);
  void complete();
  BleSupervisorAction tick(uint32_t nowMs);

  bool active() const;
  BleOperationStage stage() const;
  uint32_t timeoutCount() const;

 private:
  static uint32_t timeoutFor(BleOperationStage stage);

  BleOperationStage stage_{BleOperationStage::Idle};
  uint32_t deadlineMs_{0};
  uint32_t timeoutCount_{0};
  bool active_{false};
  bool abortIssued_{false};
};

class BleAttemptGate {
 public:
  uint32_t beginAttempt();
  void cancelCurrent();
  bool accepts(uint32_t attemptId) const;
  uint32_t current() const;

 private:
  uint32_t current_{0};
  bool cancelled_{true};
};

}  // namespace sozo
