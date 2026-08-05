#pragma once

#include <NodeControlPort.h>
#include <SozoDomain.h>
#include <SozoNodeMessages.h>

namespace sozo::c3 {

class NodeLightingSink {
 public:
  virtual ~NodeLightingSink() = default;
  virtual void begin(const PersistedLightingState &state) = 0;
  virtual const PersistedLightingState &state() const = 0;
  virtual void applyState(const PersistedLightingState &state) = 0;
  virtual void setLitPixelCount(uint16_t count) = 0;
  virtual void tick(uint32_t now, const AudioFrame &audio) = 0;
};

enum class SceneApplyResult : uint8_t {
  Applied = 0,
  Duplicate,
  Stale,
  Invalid,
};

enum class SceneTarget : uint8_t {
  FollowMain = 0,
  Independent = 1,
};

class NodeSceneRuntime {
 public:
  explicit NodeSceneRuntime(NodeLightingSink &lighting);

  SceneApplyResult applyScene(const node::SceneSnapshotPayload &scene,
                              uint32_t sceneRevision,
                              uint32_t coordinatorTimestampMs,
                              uint32_t localNowMs,
                              SceneTarget target = SceneTarget::FollowMain);
  bool setLocalLedCount(uint16_t ledCount);
  bool setControlMode(node::NodeControlMode mode);
  node::NodeControlMode controlMode() const;
  NodeControlState controlState() const;
  bool restoreControlState(const NodeControlState &state);
  bool applyAudioFeatures(const node::AudioFeaturesPayload &features,
                          uint32_t sequence);
  void tick(uint32_t localNowMs);
  void onDisconnected();

  uint32_t lastAppliedSceneRevision() const;
  uint32_t synchronizedNow(uint32_t localNowMs) const;

 private:
  struct SceneSlot {
    PersistedLightingState state{};
    int16_t manualLitPixelCount{-1};
    uint32_t revision{0};
    bool available{false};
  };

  SceneSlot &slotFor(SceneTarget target);
  const SceneSlot &slotFor(SceneTarget target) const;
  void applySlot(const SceneSlot &slot);

  NodeLightingSink &lighting_;
  AudioFrame audio_{};
  SceneSlot followScene_{};
  SceneSlot independentScene_{};
  node::NodeControlMode controlMode_{node::NodeControlMode::FollowMain};
  uint32_t lastAudioSequence_{0};
  uint32_t clockOffsetMs_{0};
  bool hasAudioSequence_{false};
};

}  // namespace sozo::c3
