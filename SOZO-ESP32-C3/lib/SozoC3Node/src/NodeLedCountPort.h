#pragma once

#include <stdint.h>

namespace sozo::c3 {

struct NodeLedCountState {
  static constexpr uint32_t kSchemaVersion = 2U;

  uint32_t schemaVersion{kSchemaVersion};
  uint16_t ledCount{0};
  uint8_t layoutProfile{0};
  uint16_t centerIndex{0};
  uint16_t leftCount{0};
  uint16_t centerCount{0};
  uint16_t rightCount{0};
  bool reversed{false};
};

class NodeLedCountRepository {
 public:
  virtual ~NodeLedCountRepository() = default;

  virtual NodeLedCountState load() = 0;
  virtual bool save(const NodeLedCountState &state) = 0;
};

}  // namespace sozo::c3
