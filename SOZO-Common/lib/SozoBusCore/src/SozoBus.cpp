#include <SozoBus.h>

namespace sozo::node {

BusResult SozoBus::subscribe(const uint16_t channelId,
                             const MessageType messageType,
                             const MessageHandler handler, void *context) {
  if (handler == nullptr || !isKnownMessageType(messageType)) {
    return BusResult::InvalidArgument;
  }

  Subscription *available = nullptr;
  for (Subscription &subscription : subscriptions_) {
    if (!subscription.active) {
      if (available == nullptr) available = &subscription;
      continue;
    }
    if (subscription.channelId == channelId &&
        subscription.messageType == messageType &&
        subscription.handler == handler && subscription.context == context) {
      return BusResult::DuplicateSubscription;
    }
  }
  if (available == nullptr) return BusResult::SubscriptionLimit;

  *available = {true, channelId, messageType, handler, context};
  return BusResult::Ok;
}

size_t SozoBus::publish(const Envelope &envelope) const {
  size_t deliveries = 0;
  for (const Subscription &subscription : subscriptions_) {
    if (!subscription.active || subscription.channelId != envelope.channelId ||
        subscription.messageType != envelope.messageType) {
      continue;
    }
    subscription.handler(envelope, subscription.context);
    ++deliveries;
  }
  return deliveries;
}

BusResult SozoBus::awaitResponse(const uint32_t correlationId,
                                 const uint32_t deadlineMs,
                                 const ResponseHandler handler,
                                 void *context) {
  if (correlationId == 0 || handler == nullptr) {
    return BusResult::InvalidArgument;
  }

  PendingRequest *available = nullptr;
  for (PendingRequest &pending : pendingRequests_) {
    if (!pending.active) {
      if (available == nullptr) available = &pending;
      continue;
    }
    if (pending.correlationId == correlationId) {
      return BusResult::CorrelationInUse;
    }
  }
  if (available == nullptr) return BusResult::PendingRequestLimit;

  *available = {true, correlationId, deadlineMs, handler, context};
  return BusResult::Ok;
}

bool SozoBus::cancelResponse(const uint32_t correlationId) {
  if (correlationId == 0) return false;
  for (PendingRequest &pending : pendingRequests_) {
    if (!pending.active || pending.correlationId != correlationId) continue;
    pending = PendingRequest{};
    return true;
  }
  return false;
}

bool SozoBus::resolveResponse(const Envelope &envelope) {
  if ((envelope.flags & kFlagIsResponse) == 0 ||
      envelope.correlationId == 0) {
    return false;
  }

  for (PendingRequest &pending : pendingRequests_) {
    if (!pending.active ||
        pending.correlationId != envelope.correlationId) {
      continue;
    }
    const ResponseHandler handler = pending.handler;
    void *const context = pending.context;
    pending = PendingRequest{};
    handler(RequestOutcome::Response, &envelope, context);
    return true;
  }
  return false;
}

size_t SozoBus::expireRequests(const uint32_t nowMs) {
  size_t expired = 0;
  for (PendingRequest &pending : pendingRequests_) {
    if (!pending.active ||
        static_cast<int32_t>(nowMs - pending.deadlineMs) < 0) {
      continue;
    }
    const ResponseHandler handler = pending.handler;
    void *const context = pending.context;
    pending = PendingRequest{};
    handler(RequestOutcome::Timeout, nullptr, context);
    ++expired;
  }
  return expired;
}

}  // namespace sozo::node
