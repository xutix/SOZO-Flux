#include <NodeControlStore.h>

namespace sozo::c3 {

constexpr char NodeControlStore::kNamespace[];
constexpr char NodeControlStore::kStateKey[];

namespace {

bool hasSupportedMode(const LightControlMode mode) {
  return mode == LightControlMode::FollowScene ||
         mode == LightControlMode::Independent;
}

bool isValid(const NodeControlState &state) {
  return state.schemaVersion == NodeControlState::kSchemaVersion &&
         hasSupportedMode(state.controlMode) &&
         (state.controlMode != LightControlMode::Independent ||
          state.hasIndependentScene);
}

}  // namespace

NodeControlState NodeControlStore::load() {
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) return {};
  NodeControlState state{};
  const size_t expectedBytes = sizeof(NodeControlState);
  const bool read = preferences.getBytesLength(kStateKey) == expectedBytes &&
                    preferences.getBytes(kStateKey, &state, expectedBytes) ==
                        expectedBytes;
  preferences.end();
  return read && isValid(state) ? state : NodeControlState{};
}

bool NodeControlStore::save(const NodeControlState &state) {
  if (!isValid(state)) return false;
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) return false;
  const bool saved = preferences.putBytes(kStateKey, &state, sizeof(state)) ==
                     sizeof(state);
  preferences.end();
  return saved;
}

bool NodeControlStore::clear() {
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) return false;
  const bool removed = preferences.remove(kStateKey);
  preferences.end();
  return removed;
}

}  // namespace sozo::c3
