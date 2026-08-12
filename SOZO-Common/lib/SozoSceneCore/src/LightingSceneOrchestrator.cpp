#include "LightingSceneOrchestrator.h"

#include <string.h>

namespace sozo {

bool NamedLightingScene::setName(const char *value) {
  if (value == nullptr) return false;
  const size_t length = strlen(value);
  if (length == 0U || length >= kNameBytes) return false;
  memcpy(name, value, length + 1U);
  return true;
}

bool LightingSceneOrchestrator::saveScene(
    const NamedLightingScene &scene) {
  if (!validDefinition(scene)) return false;
  for (size_t index = 0U; index < sceneCount_; ++index) {
    if (scenes_[index].id == scene.id) {
      scenes_[index] = scene;
      return true;
    }
  }
  if (sceneCount_ >= kMaxScenes) return false;
  scenes_[sceneCount_++] = scene;
  return true;
}

bool LightingSceneOrchestrator::eraseScene(const LightingSceneId sceneId) {
  if (sceneId == 0U) return false;
  for (size_t index = 0U; index < sceneCount_; ++index) {
    if (scenes_[index].id != sceneId) continue;
    for (size_t move = index + 1U; move < sceneCount_; ++move) {
      scenes_[move - 1U] = scenes_[move];
    }
    scenes_[--sceneCount_] = NamedLightingScene{};
    return true;
  }
  return false;
}

const NamedLightingScene *LightingSceneOrchestrator::sceneById(
    const LightingSceneId sceneId) const {
  for (size_t index = 0U; index < sceneCount_; ++index) {
    if (scenes_[index].id == sceneId) return &scenes_[index];
  }
  return nullptr;
}

const NamedLightingScene *LightingSceneOrchestrator::sceneAt(
    const size_t index) const {
  return index < sceneCount_ ? &scenes_[index] : nullptr;
}

size_t LightingSceneOrchestrator::sceneCount() const { return sceneCount_; }

bool LightingSceneOrchestrator::activateScene(
    const LightingSceneId sceneId) {
  const NamedLightingScene *scene = sceneById(sceneId);
  if (scene == nullptr) return false;
  size_t newTargets = 0U;
  for (size_t index = 0U; index < scene->assignmentCount; ++index) {
    if (desiredFor(scene->assignments[index].targetId) == nullptr) {
      ++newTargets;
    }
  }
  if (newTargets > kMaxTargets - desiredCount_) return false;
  const uint32_t revision = nextRevision();
  for (size_t index = 0U; index < scene->assignmentCount; ++index) {
    if (!assign(scene->assignments[index].targetId,
                scene->assignments[index].scene, sceneId, revision)) {
      return false;
    }
  }
  return true;
}

bool LightingSceneOrchestrator::applyDirect(
    const LightingTargetId targetId, const LightingScene &scene) {
  if (targetId == 0U || !validScene(scene) ||
      (mutableDesiredFor(targetId) == nullptr &&
       desiredCount_ >= kMaxTargets)) {
    return false;
  }
  return assign(targetId, scene, kDirectLightControlSource, nextRevision());
}

const DesiredLightingState *LightingSceneOrchestrator::desiredFor(
    const LightingTargetId targetId) const {
  for (size_t index = 0U; index < desiredCount_; ++index) {
    if (desired_[index].targetId == targetId) return &desired_[index];
  }
  return nullptr;
}

const DesiredLightingState *LightingSceneOrchestrator::desiredAt(
    const size_t index) const {
  return index < desiredCount_ ? &desired_[index] : nullptr;
}

size_t LightingSceneOrchestrator::desiredCount() const {
  return desiredCount_;
}

bool LightingSceneOrchestrator::nextPending(
    DesiredLightingState &desired) const {
  for (size_t index = 0U; index < desiredCount_; ++index) {
    if (!desired_[index].pending()) continue;
    desired = desired_[index];
    return true;
  }
  return false;
}

bool LightingSceneOrchestrator::markDelivered(
    const LightingTargetId targetId, const uint32_t revision) {
  DesiredLightingState *desired = mutableDesiredFor(targetId);
  if (desired == nullptr || desired->revision != revision) return false;
  desired->deliveredRevision = revision;
  return true;
}

LightingSceneOrchestratorState LightingSceneOrchestrator::state() const {
  LightingSceneOrchestratorState state{};
  state.revision = revision_;
  state.sceneCount = static_cast<uint8_t>(sceneCount_);
  state.desiredCount = static_cast<uint8_t>(desiredCount_);
  for (size_t index = 0U; index < sceneCount_; ++index) {
    state.scenes[index] = scenes_[index];
  }
  for (size_t index = 0U; index < desiredCount_; ++index) {
    state.desired[index] = desired_[index];
  }
  return state;
}

bool LightingSceneOrchestrator::restore(
    const LightingSceneOrchestratorState &state) {
  if (state.schemaVersion != LightingSceneOrchestratorState::kSchemaVersion ||
      state.sceneCount > kMaxScenes || state.desiredCount > kMaxTargets) {
    return false;
  }
  for (size_t index = 0U; index < state.sceneCount; ++index) {
    if (!validDefinition(state.scenes[index])) return false;
  }
  for (size_t index = 0U; index < state.desiredCount; ++index) {
    const DesiredLightingState &desired = state.desired[index];
    if (!desired.configured || desired.targetId == 0U ||
        desired.revision == 0U || !validScene(desired.scene)) {
      return false;
    }
    for (size_t previous = 0U; previous < index; ++previous) {
      if (state.desired[previous].targetId == desired.targetId) return false;
    }
  }
  sceneCount_ = state.sceneCount;
  desiredCount_ = state.desiredCount;
  revision_ = state.revision;
  for (size_t index = 0U; index < kMaxScenes; ++index) {
    scenes_[index] = index < sceneCount_ ? state.scenes[index]
                                         : NamedLightingScene{};
  }
  for (size_t index = 0U; index < kMaxTargets; ++index) {
    desired_[index] = index < desiredCount_ ? state.desired[index]
                                            : DesiredLightingState{};
  }
  return true;
}

bool LightingSceneOrchestrator::validScene(const LightingScene &scene) {
  return static_cast<uint8_t>(scene.mode) <=
             static_cast<uint8_t>(EffectMode::Off) &&
         scene.settings.rainbowStyle <= 2U && scene.settings.flowSpeed >= 1U &&
         scene.settings.flowSpeed <= 100U;
}

bool LightingSceneOrchestrator::validDefinition(
    const NamedLightingScene &scene) {
  if (scene.id == 0U || scene.name[0] == '\0' ||
      scene.assignmentCount == 0U ||
      scene.assignmentCount > NamedLightingScene::kMaxAssignments) {
    return false;
  }
  for (size_t index = 0U; index < scene.assignmentCount; ++index) {
    const SceneAssignment &assignment = scene.assignments[index];
    if (assignment.targetId == 0U || !validScene(assignment.scene)) {
      return false;
    }
    for (size_t previous = 0U; previous < index; ++previous) {
      if (scene.assignments[previous].targetId == assignment.targetId) {
        return false;
      }
    }
  }
  return true;
}

DesiredLightingState *LightingSceneOrchestrator::mutableDesiredFor(
    const LightingTargetId targetId) {
  for (size_t index = 0U; index < desiredCount_; ++index) {
    if (desired_[index].targetId == targetId) return &desired_[index];
  }
  return nullptr;
}

bool LightingSceneOrchestrator::assign(
    const LightingTargetId targetId, const LightingScene &scene,
    const LightingSceneId sourceSceneId, const uint32_t revision) {
  if (targetId == 0U || revision == 0U || !validScene(scene)) return false;
  DesiredLightingState *desired = mutableDesiredFor(targetId);
  if (desired == nullptr) {
    if (desiredCount_ >= kMaxTargets) return false;
    desired = &desired_[desiredCount_++];
    *desired = {};
    desired->targetId = targetId;
  }
  desired->scene = scene;
  desired->revision = revision;
  desired->sourceSceneId = sourceSceneId;
  desired->configured = true;
  return true;
}

uint32_t LightingSceneOrchestrator::nextRevision() {
  ++revision_;
  if (revision_ == 0U) ++revision_;
  return revision_;
}

}  // namespace sozo
