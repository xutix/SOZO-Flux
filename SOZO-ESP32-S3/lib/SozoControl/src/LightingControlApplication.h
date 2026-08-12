#pragma once

#include <LightingSceneOrchestrator.h>
#include <LightingPersistencePorts.h>

namespace sozo {

enum class CommandResultCode : uint8_t {
  Applied,
  InvalidCommand,
  SourceNotAllowed,
  Unsupported,
};

struct CommandResult {
  CommandResultCode code;
  bool changed;

  constexpr bool accepted() const { return code == CommandResultCode::Applied; }
};

struct LightingApplicationSnapshot {
  PersistedLightingState lighting{};
  int16_t manualLitPixelCount{-1};
  uint32_t sceneRevision{0U};
};

class LightingControlApplication {
 public:
  LightingControlApplication(LightingConfigurationRepository &configuration,
                             LightingSceneRepository &scenes);

  void begin(const PersistedLightingState &persisted, bool localLightEnabled);
  CommandResult dispatch(const ControlCommand &command, uint32_t nowMs);
  LightingApplicationSnapshot snapshot() const;
  const LocalLightConfiguration &localLightConfiguration() const;
  void setAudioTuning(const AudioTuning &tuning, uint32_t nowMs);

  bool saveScene(const NamedLightingScene &scene, uint32_t nowMs);
  bool upsertScene(LightingSceneId requestedId, const char *name,
                   const LightingTargetId *targets, size_t targetCount,
                   const LightingScene &fallback, uint32_t nowMs,
                   LightingSceneId &savedId);
  bool updateSceneAssignment(LightingSceneId sceneId,
                             LightingTargetId targetId,
                             const LightingScene &scene, uint32_t nowMs);
  bool eraseScene(LightingSceneId sceneId, uint32_t nowMs);
  bool activateScene(LightingSceneId sceneId, uint32_t nowMs);
  bool applyDirect(LightingTargetId targetId, const LightingScene &scene,
                   uint32_t nowMs);
  const NamedLightingScene *sceneById(LightingSceneId sceneId) const;
  const NamedLightingScene *sceneAt(size_t index) const;
  size_t sceneCount() const;
  const DesiredLightingState *desiredFor(LightingTargetId targetId) const;
  const DesiredLightingState *desiredAt(size_t index) const;
  size_t desiredCount() const;
  bool markDelivered(LightingTargetId targetId, uint32_t revision);

  void tick(uint32_t nowMs);

 private:
  LightingScene currentLocalScene() const;
  bool applyLegacyLightingCommand(const ControlCommand &command,
                                  uint32_t nowMs);
  bool applyLegacyParameter(LightingScene &scene,
                            const ControlCommand &command);
  void markConfigurationDirty(uint32_t nowMs);
  void markScenesDirty(uint32_t nowMs);
  void bumpLocalConfigurationRevision();

  LightingConfigurationRepository &configurationRepository_;
  LightingSceneRepository &sceneRepository_;
  LightingSceneOrchestrator scenes_{};
  LocalLightConfiguration localConfiguration_{};
  AudioTuning audio_{};
  EffectMode modeBeforeOff_{EffectMode::Static};
};

}  // namespace sozo
