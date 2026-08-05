#pragma once

#include <SozoDomain.h>
#include <SozoNodeProtocol.h>

namespace sozo::c3 {

struct NodeControlState {
  static constexpr uint32_t kSchemaVersion = 1U;

  uint32_t schemaVersion{kSchemaVersion};
  node::NodeControlMode controlMode{node::NodeControlMode::FollowMain};
  PersistedLightingState independentState{};
  int16_t independentManualLitPixelCount{-1};
  uint32_t independentRevision{0};
  bool hasIndependentScene{false};
};

class NodeControlRepository {
 public:
  virtual ~NodeControlRepository() = default;

  virtual NodeControlState load() = 0;
  virtual bool save(const NodeControlState &state) = 0;
  virtual bool clear() = 0;
};

}  // namespace sozo::c3
