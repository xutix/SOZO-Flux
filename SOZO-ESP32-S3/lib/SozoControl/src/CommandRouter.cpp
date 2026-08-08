#include <CommandRouter.h>

#include <Arduino.h>

namespace sozo {

CommandRouter::CommandRouter(SpaceSceneCoordinator &scenes,
                             SettingsStore &settings)
    : scenes_(scenes), settings_(settings) {}

CommandResult CommandRouter::dispatch(const ControlCommand &command) {
  if (!isCommandWellFormed(command)) {
    return {CommandResultCode::InvalidCommand, false};
  }
  if (!isSourceAllowedForCommand(command.source, command.type)) {
    return {CommandResultCode::SourceNotAllowed, false};
  }
  if (!scenes_.apply(command)) {
    return {CommandResultCode::Unsupported, false};
  }
  markStateDirty();
  return {CommandResultCode::Applied, true};
}

StateSnapshot CommandRouter::snapshot() const {
  const SpaceSceneSnapshot &scene = scenes_.snapshot();
  return {scenes_.lightingState(), scene.lighting.manualLitPixelCount,
          scene.revision};
}

void CommandRouter::setAudioTuning(const AudioTuning &tuning) {
  scenes_.setAudioTuning(tuning);
  markStateDirty();
}

void CommandRouter::markStateDirty() { settings_.markDirty(millis()); }

void CommandRouter::tick(const uint32_t now) {
  settings_.tick(now, scenes_.persistedLightingState());
}

}  // namespace sozo
