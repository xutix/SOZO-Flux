#include <BleOperationSupervisor.h>

namespace sozo {

void BleOperationSupervisor::start(const BleOperationStage stage,
                                   const uint32_t nowMs) {
  if (stage == BleOperationStage::Idle) {
    complete();
    return;
  }
  stage_ = stage;
  deadlineMs_ = nowMs + timeoutFor(stage);
  active_ = true;
  abortIssued_ = false;
}

void BleOperationSupervisor::complete() {
  stage_ = BleOperationStage::Idle;
  deadlineMs_ = 0;
  active_ = false;
  abortIssued_ = false;
}

BleSupervisorAction BleOperationSupervisor::tick(const uint32_t nowMs) {
  if (!active_ || abortIssued_ ||
      static_cast<int32_t>(nowMs - deadlineMs_) < 0) {
    return BleSupervisorAction::None;
  }
  abortIssued_ = true;
  ++timeoutCount_;
  return BleSupervisorAction::Abort;
}

bool BleOperationSupervisor::active() const { return active_; }

BleOperationStage BleOperationSupervisor::stage() const { return stage_; }

uint32_t BleOperationSupervisor::timeoutCount() const {
  return timeoutCount_;
}

uint32_t BleOperationSupervisor::timeoutFor(
    const BleOperationStage stage) {
  switch (stage) {
    case BleOperationStage::Connecting:
      return 6000U;
    case BleOperationStage::Discovering:
      return 5000U;
    case BleOperationStage::Authenticating:
      return 6000U;
    case BleOperationStage::Writing:
      return 3000U;
    case BleOperationStage::Idle:
    default:
      return 0U;
  }
}

uint32_t BleAttemptGate::beginAttempt() {
  ++current_;
  if (current_ == 0U) {
    ++current_;
  }
  cancelled_ = false;
  return current_;
}

void BleAttemptGate::cancelCurrent() { cancelled_ = true; }

bool BleAttemptGate::accepts(const uint32_t attemptId) const {
  return !cancelled_ && attemptId != 0U && attemptId == current_;
}

uint32_t BleAttemptGate::current() const { return current_; }

}  // namespace sozo
