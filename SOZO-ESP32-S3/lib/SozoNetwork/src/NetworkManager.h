#pragma once

#include <Arduino.h>
#include <vector>

#include <SettingsStore.h>

namespace sozo {

enum class NetworkState : uint8_t {
  Connecting,
  Connected,
  Provisioning,
  Failed,
};

constexpr bool isProvisioningNetworkState(const NetworkState state) {
  return state == NetworkState::Provisioning;
}

const char *networkStateName(NetworkState state);

struct NetworkStatus {
  NetworkState state;
  bool mdnsAvailable;
  String ssid;
  String ip;
  String mdnsHostname;
  int32_t rssi;
  uint8_t clientCount;
};

struct ScannedNetwork {
  String ssid;
  int32_t rssi;
  bool secure;
};

class NetworkManager {
 public:
  explicit NetworkManager(SettingsStore &settings);

  bool begin();
  void tick();
  const NetworkStatus &status() const;
  std::vector<ScannedNetwork> scan();
  bool saveCredentials(const String &ssid, const String &password);
  bool resetCredentials();

 private:
  bool connectToSavedWiFi();
  bool startProvisioningAccessPoint();
  bool startMdns();

  SettingsStore &settings_;
  String savedSsid_;
  String savedPassword_;
  NetworkStatus status_;
};

}  // namespace sozo
