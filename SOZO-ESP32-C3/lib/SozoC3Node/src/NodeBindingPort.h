#pragma once

#include <SozoNodeProtocol.h>

namespace sozo::c3 {

struct NodeBinding {
  bool bound{false};
  node::NodeId coordinatorNodeId{0};
  uint64_t bleIdentityAddress{0};
  uint8_t bleIdentityAddressType{0};
};

class NodeBindingRepository {
 public:
  virtual ~NodeBindingRepository() = default;

  virtual NodeBinding load() = 0;
  virtual bool save(node::NodeId coordinatorNodeId, uint64_t identityAddress,
                    uint8_t identityAddressType) = 0;
  virtual bool clear() = 0;
};

}  // namespace sozo::c3
