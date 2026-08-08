#pragma once

#include <SettingsStore.h>
#include <SpaceSceneCoordinator.h>

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

struct StateSnapshot {
  PersistedLightingState lighting;
  int16_t manualLitPixelCount{-1};
  uint32_t sceneRevision{0U};
};

class CommandRouter {
 public:
  CommandRouter(SpaceSceneCoordinator &scenes, SettingsStore &settings);

  CommandResult dispatch(const ControlCommand &command);
  StateSnapshot snapshot() const;
  void setAudioTuning(const AudioTuning &tuning);
  void markStateDirty();
  void tick(uint32_t now);

 private:
  SpaceSceneCoordinator &scenes_;
  SettingsStore &settings_;
};

}  // namespace sozo
