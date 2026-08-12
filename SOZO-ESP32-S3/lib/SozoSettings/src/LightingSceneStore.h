#pragma once

#include <Preferences.h>

#include <LightingSceneOrchestrator.h>
#include <LightingPersistencePorts.h>

namespace sozo {

class LightingSceneStore final : public LightingSceneRepository {
 public:
  bool load(LightingSceneOrchestrator &orchestrator);
  bool save(const LightingSceneOrchestrator &orchestrator);
  void markDirty(uint32_t nowMs);
  bool tick(uint32_t nowMs, const LightingSceneOrchestrator &orchestrator);

  bool loadLightingScenes(LightingSceneOrchestrator &scenes) override {
    return load(scenes);
  }
  bool saveLightingScenes(
      const LightingSceneOrchestrator &scenes) override {
    return save(scenes);
  }
  void markLightingScenesDirty(uint32_t nowMs) override { markDirty(nowMs); }
  bool persistLightingScenes(
      uint32_t nowMs, const LightingSceneOrchestrator &scenes) override {
    return tick(nowMs, scenes);
  }

 private:
  static constexpr char kNamespace[] = "sozo-scenes";
  static constexpr char kStateKey[] = "state";
  static constexpr uint32_t kSaveDelayMs = 1000U;

  Preferences preferences_{};
  uint32_t dirtyAtMs_{0U};
  bool dirty_{false};
};

}  // namespace sozo
