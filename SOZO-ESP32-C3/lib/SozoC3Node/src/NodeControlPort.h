#pragma once

#include <LightNodeRuntime.h>

namespace sozo::c3 {

using NodeControlState = LightNodeControlState;

class NodeControlRepository {
 public:
  virtual ~NodeControlRepository() = default;

  virtual NodeControlState load() = 0;
  virtual bool save(const NodeControlState &state) = 0;
  virtual bool clear() = 0;
};

}  // namespace sozo::c3
