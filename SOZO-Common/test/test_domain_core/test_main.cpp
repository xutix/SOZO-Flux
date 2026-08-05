#include <SozoDomain.h>
#include <SpatialLightCore.h>

#include "../TestHarness.h"

namespace {

void test_shared_domain_retains_default_spatial_layout() {
  const sozo::PersistedLightingState state =
      sozo::makeDefaultPersistedLightingState();
  CHECK_EQ(spatial_light::kDefaultLedCount, state.layout.activeCount);
  CHECK_TRUE(sozo::isKnownEffect(sozo::EffectMode::Aurora));
}

void test_shared_spatial_layout_normalizes_invalid_segmented_counts() {
  const spatial_light::SpatialLayout invalid{
      spatial_light::LayoutProfile::Segmented, 60U, 0U, 10U, 20U, 10U, false};
  const spatial_light::SpatialLayout normalized =
      spatial_light::normalizeLayout(invalid);
  CHECK_EQ(spatial_light::LayoutProfile::Continuous, normalized.profile);
  CHECK_EQ(60U, normalized.activeCount);
  CHECK_EQ(60U, normalized.centerCount);
}

}  // namespace

int main(int, char **) {
  test_shared_domain_retains_default_spatial_layout();
  test_shared_spatial_layout_normalizes_invalid_segmented_counts();
  return sozo::test::finish("shared domain core tests");
}
