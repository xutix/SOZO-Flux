#pragma once

#include <NodeControlPort.h>
#include <Preferences.h>

namespace sozo::c3 {

class NodeControlStore final : public NodeControlRepository {
 public:
  NodeControlState load() override;
  bool save(const NodeControlState &state) override;
  bool clear() override;

 private:
  static constexpr char kNamespace[] = "sozo-ctrl";
  static constexpr char kStateKey[] = "state";
};

}  // namespace sozo::c3
