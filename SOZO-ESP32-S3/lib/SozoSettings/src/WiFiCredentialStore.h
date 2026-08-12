#pragma once

#include <Preferences.h>

#include <NetworkCredentialRepository.h>

namespace sozo {

class WiFiCredentialStore final : public NetworkCredentialRepository {
 public:
  explicit WiFiCredentialStore(const char *nvsNamespace = "sozo-light");

  bool load(String &ssid, String &password) override;
  bool save(const String &ssid, const String &password) override;
  bool clear() override;

 private:
  Preferences preferences_;
  const char *nvsNamespace_;
};

}  // namespace sozo
