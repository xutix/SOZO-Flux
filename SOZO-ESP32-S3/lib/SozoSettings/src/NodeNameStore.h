#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <NodeNamePolicy.h>

namespace sozo {

enum class NodeNameSaveResult : uint8_t {
  Saved = 0,
  InvalidName,
  StorageFailure,
};

class NodeNameStore {
 public:
  static constexpr size_t kMaxNameCodePoints = kMaxNodeNameCodePoints;
  static constexpr size_t kMaxNameBytes = kMaxNodeNameBytes;
  static constexpr size_t kRemoteCacheCapacity = 8U;

  explicit NodeNameStore(const char *nvsNamespace = "sozo-names");

  String localName();
  String nodeName(uint32_t nodeId);
  NodeNameSaveResult saveLocalName(const String &candidate,
                                   String &normalizedName);
  NodeNameSaveResult saveNodeName(uint32_t nodeId, const String &candidate,
                                  String &normalizedName);

 private:
  struct CachedNodeName {
    bool occupied{false};
    bool loaded{false};
    uint32_t nodeId{0U};
    String name{};
  };

  static bool normalizeName(const String &candidate, String &normalizedName);
  static void buildNodeKey(uint32_t nodeId, char key[10]);
  bool loadKey(const char *key, String &name);
  bool saveKey(const char *key, const String &name);
  CachedNodeName *cachedNode(uint32_t nodeId);

  Preferences preferences_;
  const char *nvsNamespace_;
  bool localLoaded_{false};
  String localName_{};
  CachedNodeName remoteNames_[kRemoteCacheCapacity]{};
};

}  // namespace sozo
