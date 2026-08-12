#include <LightingSceneOrchestrator.h>

#include "../TestHarness.h"

namespace {

sozo::LightingScene coloredScene(const sozo::EffectMode mode,
                                 const uint8_t red,
                                 const uint8_t green,
                                 const uint8_t blue) {
  sozo::LightingScene scene{};
  scene.mode = mode;
  scene.primaryColor = {red, green, blue};
  return scene;
}

void test_scene_activation_updates_only_its_assignments() {
  sozo::LightingSceneOrchestrator orchestrator;
  sozo::NamedLightingScene movie{};
  movie.id = 11U;
  movie.setName("Movie");
  movie.assignments[0] = {0xA1U,
                          coloredScene(sozo::EffectMode::GlassFlow,
                                       10U, 20U, 30U)};
  movie.assignments[1] = {0xB2U,
                          coloredScene(sozo::EffectMode::Breathe,
                                       40U, 50U, 60U)};
  movie.assignmentCount = 2U;

  CHECK_TRUE(orchestrator.saveScene(movie));
  CHECK_TRUE(orchestrator.applyDirect(
      0xC3U, coloredScene(sozo::EffectMode::Static, 70U, 80U, 90U)));
  const uint32_t untouchedRevision =
      orchestrator.desiredFor(0xC3U)->revision;

  CHECK_TRUE(orchestrator.activateScene(11U));
  CHECK_EQ(sozo::EffectMode::GlassFlow,
           orchestrator.desiredFor(0xA1U)->scene.mode);
  CHECK_EQ(sozo::EffectMode::Breathe,
           orchestrator.desiredFor(0xB2U)->scene.mode);
  CHECK_EQ(untouchedRevision, orchestrator.desiredFor(0xC3U)->revision);
  CHECK_EQ(90U, orchestrator.desiredFor(0xC3U)->scene.primaryColor.blue);
}

void test_overlapping_scenes_and_direct_control_are_last_command_wins() {
  sozo::LightingSceneOrchestrator orchestrator;
  sozo::NamedLightingScene movie{};
  movie.id = 21U;
  movie.setName("Movie");
  movie.assignments[0] = {
      0xA1U, coloredScene(sozo::EffectMode::GlassFlow, 1U, 2U, 3U)};
  movie.assignments[1] = {
      0xB2U, coloredScene(sozo::EffectMode::Breathe, 4U, 5U, 6U)};
  movie.assignmentCount = 2U;
  sozo::NamedLightingScene work{};
  work.id = 22U;
  work.setName("Work");
  work.assignments[0] = {
      0xB2U, coloredScene(sozo::EffectMode::Focus, 7U, 8U, 9U)};
  work.assignments[1] = {
      0xC3U, coloredScene(sozo::EffectMode::Static, 10U, 11U, 12U)};
  work.assignmentCount = 2U;

  CHECK_TRUE(orchestrator.saveScene(movie));
  CHECK_TRUE(orchestrator.saveScene(work));
  CHECK_TRUE(orchestrator.activateScene(movie.id));
  const uint32_t movieARevision = orchestrator.desiredFor(0xA1U)->revision;
  CHECK_TRUE(orchestrator.activateScene(work.id));
  CHECK_EQ(movieARevision, orchestrator.desiredFor(0xA1U)->revision);
  CHECK_EQ(work.id, orchestrator.desiredFor(0xB2U)->sourceSceneId);
  CHECK_EQ(sozo::EffectMode::Focus,
           orchestrator.desiredFor(0xB2U)->scene.mode);

  CHECK_TRUE(orchestrator.applyDirect(
      0xB2U, coloredScene(sozo::EffectMode::Static, 255U, 0U, 0U)));
  CHECK_EQ(sozo::kDirectLightControlSource,
           orchestrator.desiredFor(0xB2U)->sourceSceneId);
  CHECK_EQ(255U, orchestrator.desiredFor(0xB2U)->scene.primaryColor.red);
  CHECK_EQ(work.id, orchestrator.desiredFor(0xC3U)->sourceSceneId);
}

void test_offline_target_stays_pending_until_the_exact_revision_is_delivered() {
  sozo::LightingSceneOrchestrator orchestrator;
  CHECK_TRUE(orchestrator.applyDirect(
      0xD4U, coloredScene(sozo::EffectMode::Rainbow, 1U, 1U, 1U)));
  const uint32_t firstRevision = orchestrator.desiredFor(0xD4U)->revision;
  CHECK_TRUE(orchestrator.desiredFor(0xD4U)->pending());

  CHECK_TRUE(orchestrator.applyDirect(
      0xD4U, coloredScene(sozo::EffectMode::Focus, 2U, 2U, 2U)));
  const uint32_t latestRevision = orchestrator.desiredFor(0xD4U)->revision;
  CHECK_TRUE(!orchestrator.markDelivered(0xD4U, firstRevision));
  CHECK_TRUE(orchestrator.desiredFor(0xD4U)->pending());
  CHECK_TRUE(orchestrator.markDelivered(0xD4U, latestRevision));
  CHECK_TRUE(!orchestrator.desiredFor(0xD4U)->pending());
}

void test_persisted_state_restores_scenes_and_pending_node_targets() {
  sozo::LightingSceneOrchestrator original;
  sozo::NamedLightingScene scene{};
  scene.id = 31U;
  scene.setName("Movie room");
  scene.assignments[0] = {
      0xE5U, coloredScene(sozo::EffectMode::Aurora, 21U, 22U, 23U)};
  scene.assignmentCount = 1U;
  CHECK_TRUE(original.saveScene(scene));
  CHECK_TRUE(original.activateScene(scene.id));

  const sozo::LightingSceneOrchestratorState saved = original.state();
  sozo::LightingSceneOrchestrator restored;
  CHECK_TRUE(restored.restore(saved));
  CHECK_EQ(1U, restored.sceneCount());
  CHECK_EQ(31U, restored.sceneAt(0U)->id);
  CHECK_EQ(23U, restored.desiredFor(0xE5U)->scene.primaryColor.blue);
  CHECK_TRUE(restored.desiredFor(0xE5U)->pending());
}

}  // namespace

int main() {
  test_scene_activation_updates_only_its_assignments();
  test_overlapping_scenes_and_direct_control_are_last_command_wins();
  test_offline_target_stays_pending_until_the_exact_revision_is_delivered();
  test_persisted_state_restores_scenes_and_pending_node_targets();
  return sozo::test::finish("lighting scene orchestrator tests");
}
