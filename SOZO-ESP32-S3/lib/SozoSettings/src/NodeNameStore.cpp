#include "NodeNameStore.h"

namespace sozo {

NodeNameStore::NodeNameStore(const char *nvsNamespace)
    : preferences_(), nvsNamespace_(nvsNamespace) {}

String NodeNameStore::localName() {
  if (!localLoaded_) {
    String loadedName;
    if (loadKey("local_name", loadedName)) {
      localName_ = loadedName;
      localLoaded_ = true;
    }
  }
  return localName_;
}

String NodeNameStore::nodeName(const uint32_t nodeId) {
  if (nodeId == 0U || nodeId == UINT32_MAX) return String();
  CachedNodeName *entry = cachedNode(nodeId);
  char key[10]{};
  buildNodeKey(nodeId, key);
  if (entry == nullptr) {
    String loadedName;
    return loadKey(key, loadedName) ? loadedName : String();
  }
  if (!entry->loaded) {
    String loadedName;
    if (loadKey(key, loadedName)) {
      entry->name = loadedName;
      entry->loaded = true;
    }
  }
  return entry->name;
}

NodeNameSaveResult NodeNameStore::saveLocalName(
    const String &candidate, String &normalizedName) {
  if (!normalizeName(candidate, normalizedName)) {
    return NodeNameSaveResult::InvalidName;
  }
  if (!saveKey("local_name", normalizedName)) {
    return NodeNameSaveResult::StorageFailure;
  }
  localName_ = normalizedName;
  localLoaded_ = true;
  return NodeNameSaveResult::Saved;
}

NodeNameSaveResult NodeNameStore::saveNodeName(
    const uint32_t nodeId, const String &candidate, String &normalizedName) {
  if (nodeId == 0U || nodeId == UINT32_MAX ||
      !normalizeName(candidate, normalizedName)) {
    return NodeNameSaveResult::InvalidName;
  }
  char key[10]{};
  buildNodeKey(nodeId, key);
  if (!saveKey(key, normalizedName)) {
    return NodeNameSaveResult::StorageFailure;
  }
  if (CachedNodeName *entry = cachedNode(nodeId)) {
    entry->name = normalizedName;
    entry->loaded = true;
  }
  return NodeNameSaveResult::Saved;
}

bool NodeNameStore::normalizeName(const String &candidate,
                                  String &normalizedName) {
  NodeNameSpan span{};
  if (!inspectNodeName(candidate.c_str(), candidate.length(), span)) {
    return false;
  }
  normalizedName = candidate.substring(span.begin, span.end);
  return true;
}

void NodeNameStore::buildNodeKey(const uint32_t nodeId, char key[10]) {
  snprintf(key, 10, "n%08lx", static_cast<unsigned long>(nodeId));
}

bool NodeNameStore::loadKey(const char *key, String &name) {
  // Open read/write so a fresh device creates the namespace once instead of
  // logging a read-only NOT_FOUND error on every status poll.
  if (!preferences_.begin(nvsNamespace_, false)) return false;
  String value = preferences_.getString(key, "");
  preferences_.end();
  String normalized;
  name = normalizeName(value, normalized) ? normalized : String();
  return true;
}

bool NodeNameStore::saveKey(const char *key, const String &name) {
  if (!preferences_.begin(nvsNamespace_, false)) return false;
  bool saved = false;
  if (name.isEmpty()) {
    saved = !preferences_.isKey(key) || preferences_.remove(key);
  } else {
    saved = preferences_.putString(key, name) == name.length();
  }
  preferences_.end();
  return saved;
}

NodeNameStore::CachedNodeName *NodeNameStore::cachedNode(
    const uint32_t nodeId) {
  for (CachedNodeName &entry : remoteNames_) {
    if (entry.occupied && entry.nodeId == nodeId) return &entry;
  }
  for (CachedNodeName &entry : remoteNames_) {
    if (entry.occupied) continue;
    entry.occupied = true;
    entry.nodeId = nodeId;
    return &entry;
  }
  return nullptr;
}

}  // namespace sozo
