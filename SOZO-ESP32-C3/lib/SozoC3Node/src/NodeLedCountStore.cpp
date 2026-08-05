#include <NodeLedCountStore.h>

namespace sozo::c3 {

constexpr char NodeLedCountStore::kNamespace[];
constexpr char NodeLedCountStore::kStateKey[];

namespace {

bool isValid(const NodeLedCountState &state) {
  return state.schemaVersion == NodeLedCountState::kSchemaVersion &&
         state.ledCount > 0U;
}

}  // namespace

NodeLedCountState NodeLedCountStore::load() {
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) return {};
  NodeLedCountState state{};
  const size_t expectedBytes = sizeof(NodeLedCountState);
  const bool read = preferences.getBytesLength(kStateKey) == expectedBytes &&
                    preferences.getBytes(kStateKey, &state, expectedBytes) ==
                        expectedBytes;
  preferences.end();
  return read && isValid(state) ? state : NodeLedCountState{};
}

bool NodeLedCountStore::save(const NodeLedCountState &state) {
  if (!isValid(state)) return false;
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) return false;
  const bool saved =
      preferences.putBytes(kStateKey, &state, sizeof(state)) == sizeof(state);
  preferences.end();
  return saved;
}

}  // namespace sozo::c3
