#pragma once

#include <Arduino.h>

namespace sozo {

class NetworkCredentialRepository {
 public:
  virtual ~NetworkCredentialRepository() = default;
  virtual bool load(String &ssid, String &password) = 0;
  virtual bool save(const String &ssid, const String &password) = 0;
  virtual bool clear() = 0;
};

}  // namespace sozo
