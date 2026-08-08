#pragma once

#include <SpaceSceneCoordinator.h>
#include <SozoNodeMessages.h>

namespace sozo {

node::SceneSnapshotPayload makeSceneSnapshot(const LightingScene &scene);
node::AudioFeaturesPayload makeAudioFeatures(const AudioFrame &frame);
bool sameSceneSnapshot(const node::SceneSnapshotPayload &left,
                       const node::SceneSnapshotPayload &right);

}  // namespace sozo
