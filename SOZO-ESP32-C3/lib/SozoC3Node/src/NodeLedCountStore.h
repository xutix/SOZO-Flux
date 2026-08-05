#pragma once

#include <NodeLedCountPort.h>
#include <Preferences.h>

namespace sozo::c3 {

class NodeLedCountStore final : public NodeLedCountRepository {
 public:
  NodeLedCountState load() override;
  bool save(const NodeLedCountState &state) override;

 private:
  static constexpr char kNamespace[] = "sozo-leds";
  static constexpr char kStateKey[] = "count";
};

}  // namespace sozo::c3
