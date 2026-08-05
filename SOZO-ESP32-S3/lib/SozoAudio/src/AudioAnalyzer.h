#pragma once

#include <SozoDomain.h>

namespace sozo {

constexpr float clampAudioLevel(const float value) {
  return value < 0.0F ? 0.0F : (value > 1.0F ? 1.0F : value);
}

struct AudioSnapshot {
  AudioFrame frame;
  float rawLeftRms;
  float rawRightRms;
  float selectedRawRms;

  constexpr AudioSnapshot(AudioFrame frameValue = AudioFrame(),
                          float rawLeftRmsValue = 0.0F,
                          float rawRightRmsValue = 0.0F,
                          float selectedRawRmsValue = 0.0F)
      : frame(frameValue),
        rawLeftRms(rawLeftRmsValue),
        rawRightRms(rawRightRmsValue),
        selectedRawRms(selectedRawRmsValue) {}
};

class AudioAnalyzer {
 public:
  AudioAnalyzer();

  bool begin();
  void setTuning(const AudioTuning &tuning);
  const AudioTuning &tuning() const;
  void tick();
  const AudioSnapshot &snapshot() const;

 private:
  AudioTuning tuning_;
  AudioSnapshot snapshot_;
  float fastEnergyEnvelope_;
  float slowEnergyEnvelope_;
};

}  // namespace sozo
