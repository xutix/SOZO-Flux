#pragma once

#include <LightingSceneOrchestrator.h>

namespace sozo {

class LightingConfigurationRepository {
 public:
  virtual ~LightingConfigurationRepository() = default;
  virtual void markLightingConfigurationDirty(uint32_t nowMs) = 0;
  virtual bool persistLightingConfiguration(
      uint32_t nowMs, const PersistedLightingState &state) = 0;
};

class LightingSceneRepository {
 public:
  virtual ~LightingSceneRepository() = default;
  virtual bool loadLightingScenes(LightingSceneOrchestrator &scenes) = 0;
  virtual bool saveLightingScenes(
      const LightingSceneOrchestrator &scenes) = 0;
  virtual void markLightingScenesDirty(uint32_t nowMs) = 0;
  virtual bool persistLightingScenes(
      uint32_t nowMs, const LightingSceneOrchestrator &scenes) = 0;
};

}  // namespace sozo
