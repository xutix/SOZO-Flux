#include <BleLinkStateMachine.h>

namespace sozo {

bool BleLinkStateMachine::handle(const BleLinkEvent event) {
  BleLinkState next = state_;
  switch (state_) {
    case BleLinkState::Idle:
      if (event == BleLinkEvent::StartScan) next = BleLinkState::Scanning;
      break;
    case BleLinkState::Scanning:
      if (event == BleLinkEvent::DeviceFound) {
        next = BleLinkState::Connecting;
      } else if (event == BleLinkEvent::Failed ||
                 event == BleLinkEvent::OperationTimedOut) {
        next = BleLinkState::Backoff;
      }
      break;
    case BleLinkState::Connecting:
      if (event == BleLinkEvent::Connected) {
        next = BleLinkState::Discovering;
      } else if (event == BleLinkEvent::Failed ||
                 event == BleLinkEvent::OperationTimedOut) {
        next = BleLinkState::Backoff;
      }
      break;
    case BleLinkState::Discovering:
      if (event == BleLinkEvent::ServicesDiscovered) {
        next = BleLinkState::Authenticating;
      } else if (event == BleLinkEvent::Failed ||
                 event == BleLinkEvent::OperationTimedOut) {
        next = BleLinkState::Backoff;
      }
      break;
    case BleLinkState::Authenticating:
      if (event == BleLinkEvent::Authenticated) {
        next = BleLinkState::Ready;
      } else if (event == BleLinkEvent::Failed ||
                 event == BleLinkEvent::OperationTimedOut) {
        next = BleLinkState::Backoff;
      }
      break;
    case BleLinkState::Ready:
      if (event == BleLinkEvent::Disconnected ||
          event == BleLinkEvent::Failed ||
          event == BleLinkEvent::OperationTimedOut) {
        next = BleLinkState::Backoff;
      }
      break;
    case BleLinkState::Backoff:
      if (event == BleLinkEvent::RetryDue) next = BleLinkState::Scanning;
      break;
  }
  if (next == state_) return false;
  state_ = next;
  return true;
}

BleLinkState BleLinkStateMachine::state() const { return state_; }

bool BleLinkStateMachine::requiresWorker() const {
  return state_ == BleLinkState::Connecting ||
         state_ == BleLinkState::Discovering ||
         state_ == BleLinkState::Authenticating ||
         state_ == BleLinkState::Ready;
}

void BleLinkStateMachine::reset() { state_ = BleLinkState::Idle; }

}  // namespace sozo
