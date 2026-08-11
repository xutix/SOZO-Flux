#pragma once

#include <LightingSceneOrchestrator.h>
#include <NodeFleetCoordinator.h>

namespace sozo {

class LocalLightingTarget {
 public:
  virtual ~LocalLightingTarget() = default;
  virtual bool available() const = 0;
  virtual bool apply(const LightingScene &scene, uint32_t revision,
                     uint32_t nowMs) = 0;
};

class SceneDeliveryCoordinator {
 public:
  SceneDeliveryCoordinator(LightingSceneOrchestrator &scenes,
                           LocalLightingTarget &localTarget,
                           NodeFleetCoordinator &nodes);

  void tick(uint32_t nowMs);

 private:
  struct Attempt {
    LightingTargetId targetId{0U};
    uint32_t revision{0U};
    uint32_t receiptBefore{0U};
    uint32_t attemptedAtMs{0U};
  };

  Attempt *attemptFor(LightingTargetId targetId);
  void deliverLocal(uint32_t nowMs);
  void deliverRemote(const NodeRecord &record, uint32_t nowMs);

  static constexpr uint32_t kRetryDelayMs = 3000U;

  LightingSceneOrchestrator &scenes_;
  LocalLightingTarget &localTarget_;
  NodeFleetCoordinator &nodes_;
  Attempt attempts_[LightingSceneOrchestrator::kMaxTargets]{};
};

}  // namespace sozo
