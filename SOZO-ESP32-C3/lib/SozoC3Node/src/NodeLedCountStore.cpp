#include <NodeLedCountStore.h>

namespace sozo::c3 {

constexpr char NodeLedCountStore::kNamespace[];
constexpr char NodeLedCountStore::kStateKey[];

namespace {

struct LegacyNodeLedCountState {
  uint32_t schemaVersion{1U};
  uint16_t ledCount{0U};
};

bool isValid(const NodeLedCountState &state) {
  return state.schemaVersion == NodeLedCountState::kSchemaVersion &&
         state.ledCount > 0U && state.layoutProfile <= 1U &&
         ((state.layoutProfile == 0U && state.centerIndex < state.ledCount) ||
          (state.layoutProfile == 1U && state.centerCount > 0U &&
           static_cast<uint32_t>(state.leftCount) + state.centerCount +
                   state.rightCount ==
               state.ledCount));
}

}  // namespace

NodeLedCountState NodeLedCountStore::load() {
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) return {};
  NodeLedCountState state{};
  const size_t expectedBytes = sizeof(NodeLedCountState);
  const size_t storedBytes = preferences.getBytesLength(kStateKey);
  const bool read = storedBytes == expectedBytes &&
                    preferences.getBytes(kStateKey, &state, expectedBytes) ==
                        expectedBytes;
  if (!read && storedBytes == sizeof(LegacyNodeLedCountState)) {
    LegacyNodeLedCountState legacy{};
    if (preferences.getBytes(kStateKey, &legacy, sizeof(legacy)) ==
            sizeof(legacy) &&
        legacy.schemaVersion == 1U && legacy.ledCount > 0U) {
      state = NodeLedCountState{};
      state.ledCount = legacy.ledCount;
      state.centerIndex = static_cast<uint16_t>((legacy.ledCount - 1U) / 2U);
      state.centerCount = legacy.ledCount;
      preferences.putBytes(kStateKey, &state, sizeof(state));
    }
  }
  preferences.end();
  return isValid(state) ? state : NodeLedCountState{};
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
