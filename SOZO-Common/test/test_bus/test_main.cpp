#include <SozoBus.h>

#include "../TestHarness.h"

namespace {

using sozo::node::BusResult;
using sozo::node::Envelope;
using sozo::node::MessageType;
using sozo::node::RequestOutcome;
using sozo::node::SozoBus;
using sozo::node::TopicId;

struct MessageProbe {
  int calls{0};
  uint32_t lastSequence{0};
};

void onMessage(const Envelope &envelope, void *context) {
  auto &probe = *static_cast<MessageProbe *>(context);
  ++probe.calls;
  probe.lastSequence = envelope.sequence;
}

struct RequestProbe {
  int calls{0};
  RequestOutcome outcome{RequestOutcome::Timeout};
  uint32_t responseSequence{0};
};

void onResponse(const RequestOutcome outcome, const Envelope *response,
                void *context) {
  auto &probe = *static_cast<RequestProbe *>(context);
  ++probe.calls;
  probe.outcome = outcome;
  probe.responseSequence = response == nullptr ? 0 : response->sequence;
}

Envelope makeScene(const uint32_t sequence) {
  Envelope envelope{};
  envelope.messageType = MessageType::SceneSnapshot;
  envelope.channelId = static_cast<uint16_t>(TopicId::SpaceScene);
  envelope.sequence = sequence;
  return envelope;
}

void test_publish_delivers_only_matching_type_and_channel() {
  SozoBus bus;
  MessageProbe sceneProbe{};
  MessageProbe heartbeatProbe{};
  CHECK_EQ(BusResult::Ok,
           bus.subscribe(static_cast<uint16_t>(TopicId::SpaceScene),
                         MessageType::SceneSnapshot, onMessage, &sceneProbe));
  CHECK_EQ(BusResult::Ok,
           bus.subscribe(static_cast<uint16_t>(TopicId::NodeState),
                         MessageType::Heartbeat, onMessage, &heartbeatProbe));

  CHECK_EQ(1U, bus.publish(makeScene(42)));
  CHECK_EQ(1, sceneProbe.calls);
  CHECK_EQ(42U, sceneProbe.lastSequence);
  CHECK_EQ(0, heartbeatProbe.calls);
}

void test_duplicate_subscription_is_rejected() {
  SozoBus bus;
  MessageProbe probe{};
  CHECK_EQ(BusResult::Ok,
           bus.subscribe(static_cast<uint16_t>(TopicId::SpaceScene),
                         MessageType::SceneSnapshot, onMessage, &probe));
  CHECK_EQ(BusResult::DuplicateSubscription,
           bus.subscribe(static_cast<uint16_t>(TopicId::SpaceScene),
                         MessageType::SceneSnapshot, onMessage, &probe));
}

void test_subscription_capacity_is_fixed() {
  SozoBus bus;
  MessageProbe probes[SozoBus::kMaxSubscriptions + 1]{};
  for (size_t index = 0; index < SozoBus::kMaxSubscriptions; ++index) {
    CHECK_EQ(BusResult::Ok,
             bus.subscribe(static_cast<uint16_t>(1000 + index),
                           MessageType::Heartbeat, onMessage, &probes[index]));
  }
  CHECK_EQ(BusResult::SubscriptionLimit,
           bus.subscribe(2000, MessageType::Heartbeat, onMessage,
                         &probes[SozoBus::kMaxSubscriptions]));
}

void test_response_resolves_matching_request_exactly_once() {
  SozoBus bus;
  RequestProbe probe{};
  CHECK_EQ(BusResult::Ok,
           bus.awaitResponse(123U, 5000U, onResponse, &probe));

  Envelope response{};
  response.messageType = MessageType::Ack;
  response.flags = sozo::node::kFlagIsResponse;
  response.correlationId = 123U;
  response.sequence = 77U;
  CHECK_TRUE(bus.resolveResponse(response));
  CHECK_EQ(1, probe.calls);
  CHECK_EQ(RequestOutcome::Response, probe.outcome);
  CHECK_EQ(77U, probe.responseSequence);
  CHECK_TRUE(!bus.resolveResponse(response));
  CHECK_EQ(1, probe.calls);
}

void test_request_timeout_and_late_response() {
  SozoBus bus;
  RequestProbe probe{};
  CHECK_EQ(BusResult::Ok,
           bus.awaitResponse(88U, 1000U, onResponse, &probe));
  CHECK_EQ(0U, bus.expireRequests(999U));
  CHECK_EQ(1U, bus.expireRequests(1000U));
  CHECK_EQ(1, probe.calls);
  CHECK_EQ(RequestOutcome::Timeout, probe.outcome);

  Envelope response{};
  response.messageType = MessageType::Ack;
  response.flags = sozo::node::kFlagIsResponse;
  response.correlationId = 88U;
  CHECK_TRUE(!bus.resolveResponse(response));
}

void test_duplicate_correlation_and_invalid_response_are_rejected() {
  SozoBus bus;
  RequestProbe first{};
  RequestProbe second{};
  CHECK_EQ(BusResult::Ok,
           bus.awaitResponse(7U, 100U, onResponse, &first));
  CHECK_EQ(BusResult::CorrelationInUse,
           bus.awaitResponse(7U, 200U, onResponse, &second));

  Envelope notAResponse{};
  notAResponse.messageType = MessageType::Ack;
  notAResponse.correlationId = 7U;
  CHECK_TRUE(!bus.resolveResponse(notAResponse));
  CHECK_EQ(0, first.calls);
}

void test_cancel_removes_only_the_selected_pending_request() {
  SozoBus bus;
  RequestProbe cancelled{};
  RequestProbe retained{};
  CHECK_EQ(BusResult::Ok,
           bus.awaitResponse(11U, 100U, onResponse, &cancelled));
  CHECK_EQ(BusResult::Ok,
           bus.awaitResponse(12U, 100U, onResponse, &retained));
  CHECK_TRUE(bus.cancelResponse(11U));
  CHECK_TRUE(!bus.cancelResponse(11U));
  CHECK_EQ(1U, bus.expireRequests(100U));
  CHECK_EQ(0, cancelled.calls);
  CHECK_EQ(1, retained.calls);
}

}  // namespace

int main(int, char **) {
  test_publish_delivers_only_matching_type_and_channel();
  test_duplicate_subscription_is_rejected();
  test_subscription_capacity_is_fixed();
  test_response_resolves_matching_request_exactly_once();
  test_request_timeout_and_late_response();
  test_duplicate_correlation_and_invalid_response_are_rejected();
  test_cancel_removes_only_the_selected_pending_request();
  return sozo::test::finish("bus tests");
}
