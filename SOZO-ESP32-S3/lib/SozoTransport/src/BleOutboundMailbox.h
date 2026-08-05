#pragma once

#include <SozoNodeMessages.h>

namespace sozo {

class BleOutboundMailbox {
 public:
  static constexpr size_t kControlCapacity = 8;

  bool push(const node::Envelope &envelope);
  bool pop(node::Envelope &envelope);
  void clear();

  size_t controlCount() const;
  bool hasAudio() const;

 private:
  node::Envelope controls_[kControlCapacity]{};
  node::Envelope latestAudio_{};
  size_t controlHead_{0};
  size_t controlTail_{0};
  size_t controlCount_{0};
  bool hasAudio_{false};
};

}  // namespace sozo
