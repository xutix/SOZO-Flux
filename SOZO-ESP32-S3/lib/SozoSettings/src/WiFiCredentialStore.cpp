#include "WiFiCredentialStore.h"

namespace sozo {

WiFiCredentialStore::WiFiCredentialStore(const char *nvsNamespace)
    : preferences_(), nvsNamespace_(nvsNamespace) {}

bool WiFiCredentialStore::load(String &ssid, String &password) {
  ssid = "";
  password = "";
  if (!preferences_.begin(nvsNamespace_, true)) return false;
  if (preferences_.getBool("wifi_valid", false)) {
    ssid = preferences_.getString("wifi_ssid", "");
    password = preferences_.getString("wifi_pass", "");
  }
  preferences_.end();
  return true;
}

bool WiFiCredentialStore::save(const String &ssid, const String &password) {
  String normalizedSsid = ssid;
  normalizedSsid.trim();
  if (normalizedSsid.isEmpty() || normalizedSsid.length() > 32 ||
      password.length() > 63 || !preferences_.begin(nvsNamespace_, false)) {
    return false;
  }

  const bool saved = preferences_.putString("wifi_ssid", normalizedSsid) > 0 &&
                     preferences_.putString("wifi_pass", password) >= 0 &&
                     preferences_.putBool("wifi_valid", true) > 0;
  preferences_.end();
  return saved;
}

bool WiFiCredentialStore::clear() {
  if (!preferences_.begin(nvsNamespace_, false)) return false;
  preferences_.remove("wifi_ssid");
  preferences_.remove("wifi_pass");
  preferences_.remove("wifi_valid");
  preferences_.end();
  return true;
}

}  // namespace sozo
