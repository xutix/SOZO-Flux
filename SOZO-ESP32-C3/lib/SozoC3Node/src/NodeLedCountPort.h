#pragma once

#include <stdint.h>

namespace sozo::c3 {

struct NodeLedCountState {
  static constexpr uint32_t kSchemaVersion = 1U;

  uint32_t schemaVersion{kSchemaVersion};
  uint16_t ledCount{0};
};

class NodeLedCountRepository {
 public:
  virtual ~NodeLedCountRepository() = default;

  virtual NodeLedCountState load() = 0;
  virtual bool save(const NodeLedCountState &state) = 0;
};

}  // namespace sozo::c3
