#pragma once

#include <stddef.h>
#include <stdint.h>

#include <SpaceSceneCoordinator.h>
#include <SozoNodeProtocol.h>

namespace sozo {

using LightingSceneId = uint32_t;
using LightingTargetId = node::NodeId;

constexpr LightingTargetId kLocalLightingTargetId = 0xFFFFFFFEU;
constexpr LightingSceneId kDirectLightControlSource = 0U;

struct SceneAssignment {
  LightingTargetId targetId{0U};
  LightingScene scene{};
};

struct NamedLightingScene {
  static constexpr size_t kNameBytes = 49U;
  static constexpr size_t kMaxAssignments = 8U;

  LightingSceneId id{0U};
  char name[kNameBytes]{};
  SceneAssignment assignments[kMaxAssignments]{};
  size_t assignmentCount{0U};

  bool setName(const char *value);
};

struct DesiredLightingState {
  LightingTargetId targetId{0U};
  LightingScene scene{};
  uint32_t revision{0U};
  uint32_t deliveredRevision{0U};
  LightingSceneId sourceSceneId{kDirectLightControlSource};
  bool configured{false};

  bool pending() const { return configured && revision != deliveredRevision; }
};

struct LightingSceneOrchestratorState {
  static constexpr uint32_t kSchemaVersion = 1U;

  uint32_t schemaVersion{kSchemaVersion};
  NamedLightingScene scenes[8U]{};
  DesiredLightingState desired[9U]{};
  uint32_t revision{0U};
  uint8_t sceneCount{0U};
  uint8_t desiredCount{0U};
};

class LightingSceneOrchestrator {
 public:
  static constexpr size_t kMaxScenes = 8U;
  static constexpr size_t kMaxTargets = 9U;

  bool saveScene(const NamedLightingScene &scene);
  bool eraseScene(LightingSceneId sceneId);
  const NamedLightingScene *sceneById(LightingSceneId sceneId) const;
  const NamedLightingScene *sceneAt(size_t index) const;
  size_t sceneCount() const;

  bool activateScene(LightingSceneId sceneId);
  bool applyDirect(LightingTargetId targetId, const LightingScene &scene);

  const DesiredLightingState *desiredFor(LightingTargetId targetId) const;
  const DesiredLightingState *desiredAt(size_t index) const;
  size_t desiredCount() const;
  bool nextPending(DesiredLightingState &desired) const;
  bool markDelivered(LightingTargetId targetId, uint32_t revision);
  LightingSceneOrchestratorState state() const;
  bool restore(const LightingSceneOrchestratorState &state);

 private:
  static bool validScene(const LightingScene &scene);
  static bool validDefinition(const NamedLightingScene &scene);
  DesiredLightingState *mutableDesiredFor(LightingTargetId targetId);
  bool assign(LightingTargetId targetId, const LightingScene &scene,
              LightingSceneId sourceSceneId, uint32_t revision);
  uint32_t nextRevision();

  NamedLightingScene scenes_[kMaxScenes]{};
  DesiredLightingState desired_[kMaxTargets]{};
  size_t sceneCount_{0U};
  size_t desiredCount_{0U};
  uint32_t revision_{0U};
};

}  // namespace sozo
