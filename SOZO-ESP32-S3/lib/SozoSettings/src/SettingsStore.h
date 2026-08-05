#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <SozoDomain.h>

namespace sozo {

constexpr uint32_t kSettingsSaveDelayMs = 1000;

constexpr bool isSettingsSaveDue(const bool dirty, const uint32_t dirtyAt,
                                 const uint32_t now) {
  return dirty && now - dirtyAt >= kSettingsSaveDelayMs;
}

class SettingsStore {
 public:
  explicit SettingsStore(const char *nvsNamespace = "sozo-light");

  PersistedLightingState loadLightingState();
  void markDirty(uint32_t now);
  bool tick(uint32_t now, const PersistedLightingState &state);

  bool loadWiFiCredentials(String &ssid, String &password);
  bool saveWiFiCredentials(const String &ssid, const String &password);
  bool clearWiFiCredentials();

 private:
  bool saveLightingState(const PersistedLightingState &state);

  Preferences preferences_;
  const char *nvsNamespace_;
  bool dirty_;
  uint32_t dirtyAt_;
};

}  // namespace sozo
