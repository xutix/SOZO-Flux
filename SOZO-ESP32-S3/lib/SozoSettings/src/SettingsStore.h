#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <SozoDomain.h>
#include <LightingPersistencePorts.h>

namespace sozo {

constexpr uint32_t kSettingsSaveDelayMs = 1000;

constexpr bool isSettingsSaveDue(const bool dirty, const uint32_t dirtyAt,
                                 const uint32_t now) {
  return dirty && now - dirtyAt >= kSettingsSaveDelayMs;
}

class SettingsStore final : public LightingConfigurationRepository {
 public:
  explicit SettingsStore(const char *nvsNamespace = "sozo-light");

  PersistedLightingState loadLightingState();
  void markDirty(uint32_t now);
  bool tick(uint32_t now, const PersistedLightingState &state);

  void markLightingConfigurationDirty(uint32_t nowMs) override {
    markDirty(nowMs);
  }
  bool persistLightingConfiguration(
      uint32_t nowMs, const PersistedLightingState &state) override {
    return tick(nowMs, state);
  }

 private:
  bool saveLightingState(const PersistedLightingState &state);

  Preferences preferences_;
  const char *nvsNamespace_;
  bool dirty_;
  uint32_t dirtyAt_;
};

}  // namespace sozo
