#include <CommandRouter.h>

#include <Arduino.h>

namespace sozo {

CommandRouter::CommandRouter(LightingController &lighting, SettingsStore &settings)
    : lighting_(lighting), settings_(settings) {}

CommandResult CommandRouter::dispatch(const ControlCommand &command) {
  if (!isCommandWellFormed(command)) {
    return {CommandResultCode::InvalidCommand, false};
  }
  if (!isSourceAllowedForCommand(command.source, command.type)) {
    return {CommandResultCode::SourceNotAllowed, false};
  }
  if (!lighting_.apply(command)) {
    return {CommandResultCode::Unsupported, false};
  }
  markStateDirty();
  return {CommandResultCode::Applied, true};
}

StateSnapshot CommandRouter::snapshot() const { return {lighting_.state()}; }

void CommandRouter::setAudioTuning(const AudioTuning &tuning) {
  PersistedLightingState next = lighting_.state();
  next.audio = tuning;
  lighting_.setState(next);
  markStateDirty();
}

void CommandRouter::markStateDirty() { settings_.markDirty(millis()); }

void CommandRouter::tick(const uint32_t now) {
  settings_.tick(now, lighting_.persistedState());
}

}  // namespace sozo
