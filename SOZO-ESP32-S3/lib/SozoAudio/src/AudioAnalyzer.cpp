#include <AudioAnalyzer.h>

#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>

namespace sozo {
namespace {

constexpr uint8_t kMicBclkPin = 4;
constexpr uint8_t kMicWsPin = 5;
constexpr uint8_t kMicDataPin = 6;
constexpr uint32_t kMicSampleRate = 16000;
constexpr i2s_port_t kMicI2sPort = I2S_NUM_0;

}  // namespace

AudioAnalyzer::AudioAnalyzer()
    : tuning_(), snapshot_(), fastEnergyEnvelope_(0.0F), slowEnergyEnvelope_(0.0F) {}

bool AudioAnalyzer::begin() {
  const i2s_config_t i2sConfig = {
      .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX),
      .sample_rate = kMicSampleRate,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
      .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 4,
      .dma_buf_len = 256,
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0,
  };
  const i2s_pin_config_t pinConfig = {
      .mck_io_num = I2S_PIN_NO_CHANGE,
      .bck_io_num = kMicBclkPin,
      .ws_io_num = kMicWsPin,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = kMicDataPin,
  };

  if (i2s_driver_install(kMicI2sPort, &i2sConfig, 0, nullptr) != ESP_OK) {
    snapshot_.frame.available = false;
    return false;
  }
  if (i2s_set_pin(kMicI2sPort, &pinConfig) != ESP_OK) {
    i2s_driver_uninstall(kMicI2sPort);
    snapshot_.frame.available = false;
    return false;
  }
  i2s_zero_dma_buffer(kMicI2sPort);
  snapshot_.frame.available = true;
  return true;
}

void AudioAnalyzer::setTuning(const AudioTuning &tuning) { tuning_ = tuning; }

const AudioTuning &AudioAnalyzer::tuning() const { return tuning_; }

void AudioAnalyzer::tick() {
  if (!snapshot_.frame.available) {
    return;
  }

  int32_t samples[256];
  size_t bytesRead = 0;
  if (i2s_read(kMicI2sPort, samples, sizeof(samples), &bytesRead, 0) != ESP_OK ||
      bytesRead == 0) {
    return;
  }

  const size_t sampleCount = bytesRead / sizeof(samples[0]);
  int64_t sampleSum = 0;
  for (size_t index = 0; index < sampleCount; ++index) {
    sampleSum += samples[index] >> 8;
  }
  const int32_t average =
      sampleCount > 0 ? static_cast<int32_t>(sampleSum / sampleCount) : 0;
  uint64_t sumSquares = 0;
  for (size_t index = 0; index < sampleCount; ++index) {
    const int32_t deviation = (samples[index] >> 8) - average;
    sumSquares += static_cast<int64_t>(deviation) * deviation;
  }

  snapshot_.rawLeftRms = sampleCount > 0
                              ? sqrt(static_cast<double>(sumSquares) / sampleCount)
                              : 0.0F;
  snapshot_.rawRightRms = 0.0F;
  snapshot_.selectedRawRms = snapshot_.rawLeftRms;
  ++snapshot_.frame.framesRead;
  snapshot_.frame.rawRms = snapshot_.selectedRawRms;

  const float cleanRms = max(0.0F, snapshot_.selectedRawRms - tuning_.noiseFloor);
  const float normalized = clampAudioLevel(
      cleanRms * tuning_.gain / max(1.0F, tuning_.fullScale));
  const float currentLevel = normalized * 255.0F;
  const float smoothingFactor = currentLevel > snapshot_.frame.volume
                                    ? tuning_.attack
                                    : tuning_.release;
  snapshot_.frame.volume +=
      (currentLevel - snapshot_.frame.volume) * smoothingFactor;

  fastEnergyEnvelope_ += (currentLevel - fastEnergyEnvelope_) * 0.55F;
  slowEnergyEnvelope_ += (currentLevel - slowEnergyEnvelope_) * 0.035F;
  const float threshold = max(4.0F, slowEnergyEnvelope_ * tuning_.beatSensitivity);
  if (fastEnergyEnvelope_ > threshold) {
    const float transient = fastEnergyEnvelope_ - threshold;
    snapshot_.frame.beatPulse = max(
        snapshot_.frame.beatPulse,
        min(255.0F, transient * 3.0F + tuning_.beatBoost));
  }
  snapshot_.frame.beatPulse *= 0.78F;
  if (snapshot_.frame.beatPulse < 0.5F) {
    snapshot_.frame.beatPulse = 0.0F;
  }
  if (snapshot_.frame.volume < 0.5F) {
    snapshot_.frame.volume = 0.0F;
  }
  snapshot_.frame.fastEnergy = fastEnergyEnvelope_;
  snapshot_.frame.slowEnergy = slowEnergyEnvelope_;
}

const AudioSnapshot &AudioAnalyzer::snapshot() const { return snapshot_; }

}  // namespace sozo
