#pragma once

#include <LightingController.h>
#include <SettingsStore.h>

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
};

class CommandRouter {
 public:
  CommandRouter(LightingController &lighting, SettingsStore &settings);

  CommandResult dispatch(const ControlCommand &command);
  StateSnapshot snapshot() const;
  void setAudioTuning(const AudioTuning &tuning);
  void markStateDirty();
  void tick(uint32_t now);

 private:
  LightingController &lighting_;
  SettingsStore &settings_;
};

}  // namespace sozo
