#pragma once

#include <SozoNodeProtocol.h>

namespace sozo::node {

enum class BusResult : uint8_t {
  Ok = 0,
  InvalidArgument,
  DuplicateSubscription,
  SubscriptionLimit,
  CorrelationInUse,
  PendingRequestLimit,
};

enum class RequestOutcome : uint8_t {
  Response = 0,
  Timeout,
};

using MessageHandler = void (*)(const Envelope &envelope, void *context);
using ResponseHandler = void (*)(RequestOutcome outcome,
                                 const Envelope *response, void *context);

class SozoBus {
 public:
  static constexpr size_t kMaxSubscriptions = 16;
  static constexpr size_t kMaxPendingRequests = 8;

  BusResult subscribe(uint16_t channelId, MessageType messageType,
                      MessageHandler handler, void *context);
  size_t publish(const Envelope &envelope) const;

  BusResult awaitResponse(uint32_t correlationId, uint32_t deadlineMs,
                          ResponseHandler handler, void *context);
  bool cancelResponse(uint32_t correlationId);
  bool resolveResponse(const Envelope &envelope);
  size_t expireRequests(uint32_t nowMs);

 private:
  struct Subscription {
    bool active{false};
    uint16_t channelId{0};
    MessageType messageType{MessageType::Heartbeat};
    MessageHandler handler{nullptr};
    void *context{nullptr};
  };

  struct PendingRequest {
    bool active{false};
    uint32_t correlationId{0};
    uint32_t deadlineMs{0};
    ResponseHandler handler{nullptr};
    void *context{nullptr};
  };

  Subscription subscriptions_[kMaxSubscriptions]{};
  PendingRequest pendingRequests_[kMaxPendingRequests]{};
};

}  // namespace sozo::node
