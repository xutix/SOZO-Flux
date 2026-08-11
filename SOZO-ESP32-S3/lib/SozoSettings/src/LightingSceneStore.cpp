#include "LightingSceneStore.h"

namespace sozo {

constexpr char LightingSceneStore::kNamespace[];
constexpr char LightingSceneStore::kStateKey[];

bool LightingSceneStore::load(LightingSceneOrchestrator &orchestrator) {
  if (!preferences_.begin(kNamespace, true)) return false;
  LightingSceneOrchestratorState state{};
  const size_t bytes = sizeof(state);
  const bool loaded = preferences_.getBytesLength(kStateKey) == bytes &&
                      preferences_.getBytes(kStateKey, &state, bytes) == bytes;
  preferences_.end();
  return loaded && orchestrator.restore(state);
}

bool LightingSceneStore::save(
    const LightingSceneOrchestrator &orchestrator) {
  if (!preferences_.begin(kNamespace, false)) return false;
  const LightingSceneOrchestratorState state = orchestrator.state();
  const bool saved = preferences_.putBytes(kStateKey, &state, sizeof(state)) ==
                     sizeof(state);
  preferences_.end();
  if (saved) dirty_ = false;
  return saved;
}

void LightingSceneStore::markDirty(const uint32_t nowMs) {
  dirty_ = true;
  dirtyAtMs_ = nowMs;
}

bool LightingSceneStore::tick(
    const uint32_t nowMs,
    const LightingSceneOrchestrator &orchestrator) {
  if (!dirty_ || static_cast<uint32_t>(nowMs - dirtyAtMs_) < kSaveDelayMs) {
    return false;
  }
  return save(orchestrator);
}

}  // namespace sozo
