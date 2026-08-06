#pragma once

#include <NodeRegistry.h>
#include <NodeTransport.h>
#include <SceneMessageMapper.h>
#include <SozoBus.h>

namespace sozo {

class NodeCoordinator {
 public:
  explicit NodeCoordinator(NodeTransport &transport);
  NodeCoordinator(NodeTransport &transport, NodeRegistry &registry);

  bool begin();
  void tick(uint32_t nowMs, const PersistedLightingState &lightingState,
            const LightingSnapshot &lightingRuntime,
            const AudioFrame &audioFrame);

  const NodeRegistry &registry() const;
  NodeTransportState transportState() const;
  bool nodeReady() const;
  node::NodeId activeNodeId() const;
  void setBindingAllowed(bool allowed);
  const char *operationName() const;
  bool workerBusy() const;
  uint32_t timeoutCount() const;
  bool requestNodeControlMode(node::NodeId nodeId,
                              node::NodeControlMode mode,
                              uint32_t nowMs);
  bool requestIndependentScene(node::NodeId nodeId,
                               const PersistedLightingState &state,
                               uint32_t nowMs);
  bool requestNodeLedCount(node::NodeId nodeId, uint16_t ledCount,
                           uint32_t nowMs);

 private:
  static void onBindResponse(node::RequestOutcome outcome,
                             const node::Envelope *response, void *context);
  static void onSceneReceipt(node::RequestOutcome outcome,
                             const node::Envelope *response, void *context);
  static void onStatusResponse(node::RequestOutcome outcome,
                               const node::Envelope *response, void *context);
  static void onControlModeReceipt(node::RequestOutcome outcome,
                                   const node::Envelope *response,
                                   void *context);
  static void onIndependentSceneReceipt(node::RequestOutcome outcome,
                                        const node::Envelope *response,
                                        void *context);
  static void onLedCountReceipt(node::RequestOutcome outcome,
                                const node::Envelope *response,
                                void *context);

  void observeScene(const PersistedLightingState &lightingState,
                    const LightingSnapshot &lightingRuntime);
  void observeTransportLifecycle(uint32_t nowMs);
  void handleReadyGeneration(uint32_t nowMs);
  void handleInbound(uint32_t nowMs);
  void requestBinding(uint32_t nowMs);
  bool sendScene(uint32_t nowMs);
  bool requestStatus(uint32_t nowMs);
  bool isAvailableLightNode(node::NodeId nodeId) const;
  void sendAudio(uint32_t nowMs, EffectMode mode,
                 const AudioFrame &audioFrame);
  bool isAudioEffect(EffectMode mode) const;

  NodeTransport &transport_;
  NodeRegistry ownedRegistry_{};
  NodeRegistry &registry_;
  node::SozoBus bus_{};
  node::SceneSnapshotPayload currentScene_{};
  uint32_t sceneRevision_{0};
  uint32_t outboundSequence_{1};
  uint32_t correlationSequence_{1};
  uint32_t handledReadyGeneration_{0};
  uint32_t sceneSentGeneration_{0};
  uint32_t acknowledgedSceneRevision_{0};
  uint32_t pendingBindCorrelation_{0};
  uint32_t pendingSceneCorrelation_{0};
  uint32_t pendingSceneRevision_{0};
  uint32_t statusRequestedGeneration_{0};
  uint32_t pendingStatusCorrelation_{0};
  uint32_t pendingControlModeCorrelation_{0};
  uint32_t pendingIndependentSceneCorrelation_{0};
  uint32_t pendingLedCountCorrelation_{0};
  uint32_t independentSceneRevision_{0};
  uint32_t lastAudioSentMs_{0};
  uint32_t lastTickMs_{0};
  node::NodeId activeNodeId_{0};
  bool hasScene_{false};
  bool bindingReady_{false};
  bool bindingRequested_{false};
  bool transportWasReady_{false};
  bool bindingAllowed_{true};
};

}  // namespace sozo
