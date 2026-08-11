#pragma once

#include <LightNodeRuntime.h>
#include <NodeControlPort.h>
#include <SozoNodeMessages.h>

namespace sozo::c3 {

using NodeLightingSink = LightNodeSink;
using SceneApplyResult = LightSceneApplyResult;
using SceneTarget = LightSceneTarget;

class NodeSceneRuntime {
 public:
  explicit NodeSceneRuntime(NodeLightingSink &lighting);

  SceneApplyResult applyScene(const node::SceneSnapshotPayload &scene,
                              uint32_t sceneRevision,
                              uint32_t coordinatorTimestampMs,
                              uint32_t localNowMs,
                              SceneTarget target = SceneTarget::FollowSpace);
  bool setLocalLedCount(uint16_t ledCount);
  bool setLocalLayout(const spatial_light::SpatialLayout &layout);
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
  LightNodeRuntime runtime_;
};

}  // namespace sozo::c3
