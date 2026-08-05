#pragma once

#include <LightingController.h>
#include <SozoNodeMessages.h>

namespace sozo {

node::SceneSnapshotPayload makeSceneSnapshot(
    const PersistedLightingState &state, const LightingSnapshot &runtime);
node::AudioFeaturesPayload makeAudioFeatures(const AudioFrame &frame);
bool sameSceneSnapshot(const node::SceneSnapshotPayload &left,
                       const node::SceneSnapshotPayload &right);

}  // namespace sozo
