#include <NetworkManager.h>

#include <ESPmDNS.h>
#include <WiFi.h>

#include <algorithm>

namespace sozo {
namespace {

constexpr char kProvisioningApSsid[] = "SOZO-FLUX-SETUP";
constexpr char kProvisioningApPassword[] = "12345678";
constexpr char kMdnsHostname[] = "sozo-flux";
constexpr uint32_t kConnectTimeoutMs = 30000;

}  // namespace

const char *networkStateName(const NetworkState state) {
  switch (state) {
    case NetworkState::Connecting:
      return "connecting";
    case NetworkState::Connected:
      return "connected";
    case NetworkState::Provisioning:
      return "provisioning";
    case NetworkState::Failed:
    default:
      return "failed";
  }
}

NetworkManager::NetworkManager(SettingsStore &settings)
    : settings_(settings),
      savedSsid_(),
      savedPassword_(),
      status_{NetworkState::Failed, false, String(), String(), String(), 0, 0} {}

bool NetworkManager::begin() {
  savedSsid_ = String();
  savedPassword_ = String();
  settings_.loadWiFiCredentials(savedSsid_, savedPassword_);
  if (savedSsid_.isEmpty()) {
    return startProvisioningAccessPoint();
  }
  return connectToSavedWiFi() || startProvisioningAccessPoint();
}

void NetworkManager::tick() {
  if (status_.state == NetworkState::Connected) {
    status_.ip = WiFi.localIP().toString();
    status_.rssi = WiFi.RSSI();
  } else if (status_.state == NetworkState::Provisioning) {
    status_.ip = WiFi.softAPIP().toString();
    status_.clientCount = WiFi.softAPgetStationNum();
  }
}

const NetworkStatus &NetworkManager::status() const { return status_; }

std::vector<ScannedNetwork> NetworkManager::scan() {
  std::vector<ScannedNetwork> networks;
  const int found = WiFi.scanNetworks();
  if (found < 0) {
    return networks;
  }
  for (int index = 0; index < found; ++index) {
    const String ssid = WiFi.SSID(index);
    if (ssid.isEmpty()) {
      continue;
    }
    const int32_t rssi = WiFi.RSSI(index);
    const bool secure = WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
    bool duplicate = false;
    for (ScannedNetwork &network : networks) {
      if (network.ssid == ssid) {
        if (rssi > network.rssi) {
          network.rssi = rssi;
          network.secure = secure;
        }
        duplicate = true;
        break;
      }
    }
    if (!duplicate) {
      networks.push_back({ssid, rssi, secure});
    }
  }
  WiFi.scanDelete();
  std::sort(networks.begin(), networks.end(),
            [](const ScannedNetwork &left, const ScannedNetwork &right) {
              return left.rssi > right.rssi;
            });
  return networks;
}

bool NetworkManager::saveCredentials(const String &ssid, const String &password) {
  String normalizedSsid = ssid;
  normalizedSsid.trim();
  if (!settings_.saveWiFiCredentials(normalizedSsid, password)) {
    return false;
  }
  savedSsid_ = normalizedSsid;
  savedPassword_ = password;
  return true;
}

bool NetworkManager::resetCredentials() {
  if (!settings_.clearWiFiCredentials()) {
    return false;
  }
  savedSsid_ = String();
  savedPassword_ = String();
  return true;
}

bool NetworkManager::connectToSavedWiFi() {
  status_.state = NetworkState::Connecting;
  status_.mdnsAvailable = false;
  status_.ssid = savedSsid_;
  status_.mdnsHostname = String(kMdnsHostname);
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setHostname(kMdnsHostname);
  WiFi.begin(savedSsid_.c_str(), savedPassword_.c_str());

  const uint32_t startedAt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startedAt < kConnectTimeoutMs) {
    delay(500);
  }
  if (WiFi.status() != WL_CONNECTED) {
    status_.state = NetworkState::Failed;
    WiFi.disconnect(true);
    return false;
  }

  status_.state = NetworkState::Connected;
  status_.ip = WiFi.localIP().toString();
  status_.rssi = WiFi.RSSI();
  status_.clientCount = 0;
  status_.mdnsAvailable = startMdns();
  return true;
}

bool NetworkManager::startProvisioningAccessPoint() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP_STA);
  if (!WiFi.softAP(kProvisioningApSsid, kProvisioningApPassword)) {
    status_.state = NetworkState::Failed;
    return false;
  }
  status_.state = NetworkState::Provisioning;
  status_.mdnsAvailable = false;
  status_.ssid = String(kProvisioningApSsid);
  status_.ip = WiFi.softAPIP().toString();
  status_.mdnsHostname = String();
  status_.rssi = 0;
  status_.clientCount = WiFi.softAPgetStationNum();
  return true;
}

bool NetworkManager::startMdns() {
  if (!MDNS.begin(kMdnsHostname)) {
    return false;
  }
  MDNS.addService("http", "tcp", 80);
  return true;
}

}  // namespace sozo
