#include "SettingsStore.h"

namespace {

template <typename T>
T clampStoredValue(const T value, const T minimum, const T maximum) {
  return value < minimum ? minimum : (value > maximum ? maximum : value);
}

uint8_t layoutProfileValue(const spatial_light::LayoutProfile profile) {
  return profile == spatial_light::LayoutProfile::Segmented ? 1 : 0;
}

spatial_light::LayoutProfile layoutProfileFromValue(const uint8_t value) {
  return value == 1 ? spatial_light::LayoutProfile::Segmented
                    : spatial_light::LayoutProfile::Continuous;
}

}  // namespace

namespace sozo {

SettingsStore::SettingsStore(const char *nvsNamespace)
    : preferences_(), nvsNamespace_(nvsNamespace), dirty_(false), dirtyAt_(0) {}

PersistedLightingState SettingsStore::loadLightingState() {
  PersistedLightingState state = makeDefaultPersistedLightingState();
  if (!preferences_.begin(nvsNamespace_, true)) {
    return state;
  }

  state.brightness = preferences_.getUChar("brightness", state.brightness);
  state.primaryColor.red = preferences_.getUChar("static_r", state.primaryColor.red);
  state.primaryColor.green =
      preferences_.getUChar("static_g", state.primaryColor.green);
  state.primaryColor.blue =
      preferences_.getUChar("static_b", state.primaryColor.blue);
  state.audioColorStyle = clampStoredValue<uint8_t>(
      preferences_.getUChar("audio_style", state.audioColorStyle), 0, 4);
  state.cometColorStyle = clampStoredValue<uint8_t>(
      preferences_.getUChar("comet_style", state.cometColorStyle), 0, 4);

  LightingSettings &settings = state.lighting;
  settings.rainbowStyle = clampStoredValue<uint8_t>(
      preferences_.getUChar("rnb_style", settings.rainbowStyle), 0, 2);
  settings.flowSpeed = clampStoredValue<uint8_t>(
      preferences_.getUChar("flow_speed", settings.flowSpeed), 1, 100);
  settings.cometTail = clampStoredValue<uint8_t>(
      preferences_.getUChar("c_tail", settings.cometTail), 5, 80);
  settings.cometSpeed = clampStoredValue<uint8_t>(
      preferences_.getUChar("c_speed", settings.cometSpeed), 1, 50);
  settings.cometDensity = clampStoredValue<uint8_t>(
      preferences_.getUChar("c_dense", settings.cometDensity), 1, 8);
  settings.cometBackground = clampStoredValue<uint8_t>(
      preferences_.getUChar("c_bg", settings.cometBackground), 0, 60);
  settings.cometRandom = preferences_.getBool("c_rand", settings.cometRandom);
  settings.audioSensitivityX100 = clampStoredValue<uint16_t>(
      preferences_.getUShort("a_sens", settings.audioSensitivityX100), 10, 500);
  settings.audioColorGainX100 = clampStoredValue<uint16_t>(
      preferences_.getUShort("a_color", settings.audioColorGainX100), 0, 500);
  settings.audioHueDrive = clampStoredValue<uint8_t>(
      preferences_.getUChar("a_hue", settings.audioHueDrive), 0, 3);
  settings.breathFloorPercent = clampStoredValue<uint8_t>(
      preferences_.getUChar("br_floor", settings.breathFloorPercent), 0, 60);
  settings.secondaryRed = preferences_.getUChar("sec_r", settings.secondaryRed);
  settings.secondaryGreen =
      preferences_.getUChar("sec_g", settings.secondaryGreen);
  settings.secondaryBlue = preferences_.getUChar("sec_b", settings.secondaryBlue);
  settings.pulseAmplitudePercent = clampStoredValue<uint8_t>(
      preferences_.getUChar("pulse_amp", settings.pulseAmplitudePercent), 0, 100);
  settings.pulseHeightPercent = clampStoredValue<uint8_t>(
      preferences_.getUChar("pulse_hgt", settings.pulseHeightPercent), 0, 100);
  settings.animationBrightness =
      preferences_.getUChar("anim_bri", settings.animationBrightness);

  const spatial_light::SpatialLayout storedLayout = {
      layoutProfileFromValue(preferences_.getUChar("layout_p", 0)),
      preferences_.getUShort("led_count", state.layout.activeCount),
      preferences_.getUShort("center_idx", state.layout.centerIndex),
      preferences_.getUShort("left_cnt", state.layout.leftCount),
      preferences_.getUShort("mid_cnt", state.layout.centerCount),
      preferences_.getUShort("right_cnt", state.layout.rightCount),
      preferences_.getBool("reversed", state.layout.reversed),
  };
  state.layout = spatial_light::normalizeLayout(storedLayout);

  state.startupColor.red = preferences_.getUChar("startup_r", state.startupColor.red);
  state.startupColor.green =
      preferences_.getUChar("startup_g", state.startupColor.green);
  state.startupColor.blue =
      preferences_.getUChar("startup_b", state.startupColor.blue);
  state.startupAnimationSpeed =
      preferences_.getUShort("startup_spd", 800) / 1000.0F;

  AudioTuning &audio = state.audio;
  audio.noiseFloor = preferences_.getFloat("noise", audio.noiseFloor);
  audio.fullScale = preferences_.getFloat("full_scale", audio.fullScale);
  audio.gain = preferences_.getFloat("audio_gain", audio.gain);
  audio.attack = preferences_.getFloat("attack", audio.attack);
  audio.release = preferences_.getFloat("release", audio.release);
  audio.beatSensitivity = preferences_.getFloat("beat_sens", audio.beatSensitivity);
  audio.beatBoost = preferences_.getFloat("beat_boost", audio.beatBoost);

  const uint8_t storedMode = preferences_.getUChar(
      "last_mode", static_cast<uint8_t>(state.mode));
  state.mode = isKnownEffect(static_cast<EffectMode>(storedMode))
                   ? static_cast<EffectMode>(storedMode)
                   : EffectMode::Static;
  preferences_.end();
  return state;
}

void SettingsStore::markDirty(const uint32_t now) {
  dirty_ = true;
  dirtyAt_ = now;
}

bool SettingsStore::tick(const uint32_t now,
                         const PersistedLightingState &state) {
  if (!isSettingsSaveDue(dirty_, dirtyAt_, now)) {
    return false;
  }
  if (!saveLightingState(state)) {
    return false;
  }
  dirty_ = false;
  return true;
}

bool SettingsStore::loadWiFiCredentials(String &ssid, String &password) {
  ssid = "";
  password = "";
  if (!preferences_.begin(nvsNamespace_, true)) {
    return false;
  }
  if (preferences_.getBool("wifi_valid", false)) {
    ssid = preferences_.getString("wifi_ssid", "");
    password = preferences_.getString("wifi_pass", "");
  }
  preferences_.end();
  return true;
}

bool SettingsStore::saveWiFiCredentials(const String &ssid,
                                        const String &password) {
  String normalizedSsid = ssid;
  normalizedSsid.trim();
  if (normalizedSsid.isEmpty() || normalizedSsid.length() > 32 ||
      password.length() > 63 || !preferences_.begin(nvsNamespace_, false)) {
    return false;
  }

  const bool saved = preferences_.putString("wifi_ssid", normalizedSsid) > 0 &&
                     preferences_.putString("wifi_pass", password) >= 0 &&
                     preferences_.putBool("wifi_valid", true) > 0;
  preferences_.end();
  return saved;
}

bool SettingsStore::clearWiFiCredentials() {
  if (!preferences_.begin(nvsNamespace_, false)) {
    return false;
  }
  preferences_.remove("wifi_ssid");
  preferences_.remove("wifi_pass");
  preferences_.remove("wifi_valid");
  preferences_.end();
  return true;
}

bool SettingsStore::saveLightingState(const PersistedLightingState &state) {
  if (!preferences_.begin(nvsNamespace_, false)) {
    return false;
  }

  const EffectMode mode = isKnownEffect(state.mode) ? state.mode : EffectMode::Static;
  preferences_.putUChar("last_mode", static_cast<uint8_t>(mode));
  preferences_.putUChar("brightness", state.brightness);
  preferences_.putUChar("static_r", state.primaryColor.red);
  preferences_.putUChar("static_g", state.primaryColor.green);
  preferences_.putUChar("static_b", state.primaryColor.blue);
  preferences_.putUChar("audio_style", state.audioColorStyle);
  preferences_.putUChar("comet_style", state.cometColorStyle);

  const LightingSettings &settings = state.lighting;
  preferences_.putUChar("rnb_style", settings.rainbowStyle);
  preferences_.putUChar("flow_speed", settings.flowSpeed);
  preferences_.putUChar("c_tail", settings.cometTail);
  preferences_.putUChar("c_speed", settings.cometSpeed);
  preferences_.putUChar("c_dense", settings.cometDensity);
  preferences_.putUChar("c_bg", settings.cometBackground);
  preferences_.putBool("c_rand", settings.cometRandom);
  preferences_.putUShort("a_sens", settings.audioSensitivityX100);
  preferences_.putUShort("a_color", settings.audioColorGainX100);
  preferences_.putUChar("a_hue", settings.audioHueDrive);
  preferences_.putUChar("br_floor", settings.breathFloorPercent);
  preferences_.putUChar("sec_r", settings.secondaryRed);
  preferences_.putUChar("sec_g", settings.secondaryGreen);
  preferences_.putUChar("sec_b", settings.secondaryBlue);
  preferences_.putUChar("pulse_amp", settings.pulseAmplitudePercent);
  preferences_.putUChar("pulse_hgt", settings.pulseHeightPercent);
  preferences_.putUChar("anim_bri", settings.animationBrightness);

  preferences_.putUChar("layout_p", layoutProfileValue(state.layout.profile));
  preferences_.putUShort("led_count", state.layout.activeCount);
  preferences_.putUShort("center_idx", state.layout.centerIndex);
  preferences_.putUShort("left_cnt", state.layout.leftCount);
  preferences_.putUShort("mid_cnt", state.layout.centerCount);
  preferences_.putUShort("right_cnt", state.layout.rightCount);
  preferences_.putBool("reversed", state.layout.reversed);

  preferences_.putUChar("startup_r", state.startupColor.red);
  preferences_.putUChar("startup_g", state.startupColor.green);
  preferences_.putUChar("startup_b", state.startupColor.blue);
  preferences_.putUShort(
      "startup_spd",
      static_cast<uint16_t>(clampStoredValue<float>(state.startupAnimationSpeed,
                                                     0.1F, 5.0F) *
                            1000.0F));
  preferences_.putFloat("noise", state.audio.noiseFloor);
  preferences_.putFloat("full_scale", state.audio.fullScale);
  preferences_.putFloat("audio_gain", state.audio.gain);
  preferences_.putFloat("attack", state.audio.attack);
  preferences_.putFloat("release", state.audio.release);
  preferences_.putFloat("beat_sens", state.audio.beatSensitivity);
  preferences_.putFloat("beat_boost", state.audio.beatBoost);
  preferences_.end();
  return true;
}

}  // namespace sozo
