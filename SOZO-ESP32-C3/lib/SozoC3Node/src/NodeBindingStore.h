#pragma once

#include <NodeBindingPort.h>
#include <Preferences.h>

namespace sozo::c3 {

class NodeBindingStore final : public NodeBindingRepository {
 public:
  NodeBinding load() override;
  bool save(node::NodeId coordinatorNodeId, uint64_t bleIdentityAddress,
            uint8_t bleIdentityAddressType) override;
  bool clear() override;

 private:
  static constexpr char kNamespace[] = "sozo-bind";
};

}  // namespace sozo::c3
