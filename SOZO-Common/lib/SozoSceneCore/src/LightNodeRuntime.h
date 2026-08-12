#pragma once

#include <stdint.h>

#include <LightingScene.h>

namespace sozo {

class LightNodeSink {
 public:
  virtual ~LightNodeSink() = default;
  virtual void begin(const PersistedLightingState &state) = 0;
  virtual const PersistedLightingState &state() const = 0;
  virtual void applyState(const PersistedLightingState &state) = 0;
  virtual void setLitPixelCount(uint16_t count) = 0;
  virtual void tick(uint32_t now, const AudioFrame &audio) = 0;
};

enum class LightSceneApplyResult : uint8_t {
  Applied = 0,
  Duplicate,
  Stale,
  Invalid,
};

enum class LightSceneTarget : uint8_t {
  FollowSpace = 0,
  Independent = 1,
};

enum class LightControlMode : uint8_t {
  FollowScene = 0,
  Independent = 1,
};

struct LightNodeControlState {
  static constexpr uint32_t kSchemaVersion = 1U;

  uint32_t schemaVersion{kSchemaVersion};
  LightControlMode controlMode{LightControlMode::FollowScene};
  PersistedLightingState independentState{};
  int16_t independentManualLitPixelCount{-1};
  uint32_t independentRevision{0U};
  bool hasIndependentScene{false};
};

class LightNodeRuntime {
 public:
  explicit LightNodeRuntime(LightNodeSink &lighting);

  void begin(const PersistedLightingState &localState);
  LightSceneApplyResult applyScene(const LightingScene &scene,
                                   uint32_t sceneRevision,
                                   uint32_t coordinatorTimestampMs,
                                   uint32_t localNowMs,
                                   LightSceneTarget target =
                                       LightSceneTarget::FollowSpace);
  bool setLocalLedCount(uint16_t ledCount);
  bool setLocalLayout(const spatial_light::SpatialLayout &layout);
  void updateLocalConfiguration(
      const LocalLightConfiguration &configuration,
      const AudioTuning &audio);
  bool setControlMode(LightControlMode mode);
  LightControlMode controlMode() const;
  LightNodeControlState controlState() const;
  bool restoreControlState(const LightNodeControlState &state);
  bool applyAudioFrame(const AudioFrame &audio, uint32_t sequence);
  void tick(uint32_t localNowMs);
  void onDisconnected();
  uint32_t lastAppliedSceneRevision() const;
  uint32_t synchronizedNow(uint32_t localNowMs) const;

 private:
  struct SceneSlot {
    PersistedLightingState state{};
    int16_t manualLitPixelCount{-1};
    uint32_t revision{0U};
    bool available{false};
  };

  SceneSlot &slotFor(LightSceneTarget target);
  const SceneSlot &slotFor(LightSceneTarget target) const;
  void applySlot(const SceneSlot &slot);
  void applyManualPixelCount(int16_t count);
  static void applyLocalConfigurationToState(
      PersistedLightingState &state,
      const LocalLightConfiguration &configuration,
      const AudioTuning &audio);

  LightNodeSink &lighting_;
  AudioFrame audio_{};
  SceneSlot followScene_{};
  SceneSlot independentScene_{};
  LightControlMode controlMode_{LightControlMode::FollowScene};
  uint32_t lastAudioSequence_{0U};
  uint32_t clockOffsetMs_{0U};
  bool hasAudioSequence_{false};
};

}  // namespace sozo
