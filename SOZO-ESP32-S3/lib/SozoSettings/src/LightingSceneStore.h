#pragma once

#include <Preferences.h>

#include <LightingSceneOrchestrator.h>

namespace sozo {

class LightingSceneStore {
 public:
  bool load(LightingSceneOrchestrator &orchestrator);
  bool save(const LightingSceneOrchestrator &orchestrator);
  void markDirty(uint32_t nowMs);
  bool tick(uint32_t nowMs, const LightingSceneOrchestrator &orchestrator);

 private:
  static constexpr char kNamespace[] = "sozo-scenes";
  static constexpr char kStateKey[] = "state";
  static constexpr uint32_t kSaveDelayMs = 1000U;

  Preferences preferences_{};
  uint32_t dirtyAtMs_{0U};
  bool dirty_{false};
};

}  // namespace sozo
