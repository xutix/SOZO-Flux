#include "LightingHttpCodec.h"

namespace sozo::web {
namespace {

bool parseEffect(const String &value, EffectMode &mode) {
  if (value == "OFF") mode = EffectMode::Off;
  else if (value == "SOLID") mode = EffectMode::Static;
  else if (value == "RAINBOW") mode = EffectMode::Rainbow;
  else if (value == "COMET") mode = EffectMode::Comet;
  else if (value == "AUDIO") mode = EffectMode::Music;
  else if (value == "BREATH") mode = EffectMode::Breathe;
  else if (value == "AURORA") mode = EffectMode::Aurora;
  else if (value == "FLAME_AUDIO") mode = EffectMode::FlameAudio;
  else if (value == "GLASS_FLOW") mode = EffectMode::GlassFlow;
  else if (value == "CORNER_PULSE") mode = EffectMode::CornerPulse;
  else if (value == "BASS_RIPPLE") mode = EffectMode::BassRipple;
  else if (value == "FOCUS") mode = EffectMode::Focus;
  else return false;
  return true;
}

bool parseHexColor(const String &value, uint8_t &red, uint8_t &green,
                   uint8_t &blue) {
  if (value.length() != 7 || value[0] != '#') return false;
  char *end = nullptr;
  const unsigned long parsed = strtoul(value.c_str() + 1, &end, 16);
  if (end == nullptr || *end != '\0') return false;
  red = static_cast<uint8_t>((parsed >> 16) & 0xff);
  green = static_cast<uint8_t>((parsed >> 8) & 0xff);
  blue = static_cast<uint8_t>(parsed & 0xff);
  return true;
}

bool supportedArgument(const String &name) {
  return name == "effect" || name == "brightness" || name == "color" ||
         name == "rainbowStyle" || name == "flowSpeed" || name == "style" ||
         name == "cometTail" || name == "cometSpeed" ||
         name == "cometDensity" || name == "cometBackground" ||
         name == "effectFlags" || name == "sensitivityX100" ||
         name == "audioColorGainX100" || name == "audioHueDrive" ||
         name == "breathFloorPercent" || name == "secondaryColor" ||
         name == "pulseAmplitudePercent" || name == "pulseHeightPercent" ||
         name == "animationBrightness" || name == "audioSource";
}

}  // namespace

const char *modeName(const EffectMode mode) {
  switch (mode) {
    case EffectMode::Static: return "static";
    case EffectMode::Rainbow: return "rainbow";
    case EffectMode::Breathe: return "breathe";
    case EffectMode::Music: return "music";
    case EffectMode::Comet: return "comet";
    case EffectMode::Aurora: return "aurora";
    case EffectMode::FlameAudio: return "flame_audio";
    case EffectMode::GlassFlow: return "glass_flow";
    case EffectMode::CornerPulse: return "corner_pulse";
    case EffectMode::BassRipple: return "bass_ripple";
    case EffectMode::Focus: return "focus";
    case EffectMode::Off:
    default: return "off";
  }
}

const char *effectId(const EffectMode mode) {
  switch (mode) {
    case EffectMode::Static: return "SOLID";
    case EffectMode::Rainbow: return "RAINBOW";
    case EffectMode::Comet: return "COMET";
    case EffectMode::Music: return "AUDIO";
    case EffectMode::Breathe: return "BREATH";
    case EffectMode::Aurora: return "AURORA";
    case EffectMode::FlameAudio: return "FLAME_AUDIO";
    case EffectMode::GlassFlow: return "GLASS_FLOW";
    case EffectMode::CornerPulse: return "CORNER_PULSE";
    case EffectMode::BassRipple: return "BASS_RIPPLE";
    case EffectMode::Focus: return "FOCUS";
    case EffectMode::Off:
    default: return "OFF";
  }
}

bool parseLegacyMode(const String &value, EffectMode &mode) {
  if (value == "static") mode = EffectMode::Static;
  else if (value == "rainbow") mode = EffectMode::Rainbow;
  else if (value == "breathe") mode = EffectMode::Breathe;
  else if (value == "music") mode = EffectMode::Music;
  else if (value == "comet") mode = EffectMode::Comet;
  else if (value == "aurora") mode = EffectMode::Aurora;
  else if (value == "flame_audio") mode = EffectMode::FlameAudio;
  else if (value == "glass_flow") mode = EffectMode::GlassFlow;
  else if (value == "corner_pulse") mode = EffectMode::CornerPulse;
  else if (value == "bass_ripple") mode = EffectMode::BassRipple;
  else if (value == "focus") mode = EffectMode::Focus;
  else if (value == "off") mode = EffectMode::Off;
  else return false;
  return true;
}

String escapeJson(const String &value) {
  String escaped;
  escaped.reserve(value.length() + 8);
  for (size_t index = 0; index < value.length(); ++index) {
    const char character = value[index];
    switch (character) {
      case '\\': escaped += F("\\\\"); break;
      case '"': escaped += F("\\\""); break;
      case '\n': escaped += F("\\n"); break;
      case '\r': escaped += F("\\r"); break;
      case '\t': escaped += F("\\t"); break;
      default: {
        const uint8_t byte = static_cast<uint8_t>(character);
        if (byte < 0x20) {
          char encoded[7];
          snprintf(encoded, sizeof(encoded), "\\u%04x", byte);
          escaped += encoded;
        } else {
          escaped += character;
        }
      }
    }
  }
  return escaped;
}

void appendLightingSceneJson(String &json, const LightingScene &scene) {
  char primary[8], secondary[8];
  snprintf(primary, sizeof(primary), "#%02X%02X%02X", scene.primaryColor.red,
           scene.primaryColor.green, scene.primaryColor.blue);
  snprintf(secondary, sizeof(secondary), "#%02X%02X%02X",
           scene.settings.secondaryRed, scene.settings.secondaryGreen,
           scene.settings.secondaryBlue);
  json += F("{\"effect\":\""); json += effectId(scene.mode);
  json += F("\",\"settings\":{\"brightness\":"); json += scene.brightness;
  json += F(",\"color\":\""); json += primary;
  json += F("\",\"rainbowStyle\":"); json += scene.settings.rainbowStyle;
  json += F(",\"flowSpeed\":"); json += scene.settings.flowSpeed;
  json += F(",\"cometTail\":"); json += scene.settings.cometTail;
  json += F(",\"cometSpeed\":"); json += scene.settings.cometSpeed;
  json += F(",\"cometDensity\":"); json += scene.settings.cometDensity;
  json += F(",\"cometBackground\":"); json += scene.settings.cometBackground;
  json += F(",\"effectFlags\":"); json += scene.settings.cometRandom ? 1 : 0;
  json += F(",\"sensitivityX100\":"); json += scene.settings.audioSensitivityX100;
  json += F(",\"style\":");
  json += scene.mode == EffectMode::Comet ? scene.cometColorStyle : scene.audioColorStyle;
  json += F(",\"audioColorGainX100\":"); json += scene.settings.audioColorGainX100;
  json += F(",\"audioHueDrive\":"); json += scene.settings.audioHueDrive;
  json += F(",\"breathFloorPercent\":"); json += scene.settings.breathFloorPercent;
  json += F(",\"secondaryColor\":\""); json += secondary;
  json += F("\",\"pulseAmplitudePercent\":"); json += scene.settings.pulseAmplitudePercent;
  json += F(",\"pulseHeightPercent\":"); json += scene.settings.pulseHeightPercent;
  json += F(",\"animationBrightness\":"); json += scene.settings.animationBrightness;
  json += F("}}");
}

bool parseLightingRequest(WebServer &server, LightingScene &next,
                          const LightingScene &fallback,
                          const bool allowNodeId, String &error) {
  if (!server.hasArg("effect")) { error = F("effect is required"); return false; }
  for (uint8_t index = 0; index < server.args(); ++index) {
    const String name = server.argName(index);
    if ((allowNodeId && name == "id") || name == "sceneId" ||
        name == "name" || name == "targets" || supportedArgument(name)) continue;
    error = F("unsupported lighting parameter"); return false;
  }
  if (server.hasArg("audioSource") && server.arg("audioSource") != "mic" &&
      server.arg("audioSource") != "1") {
    error = F("only device microphone is available"); return false;
  }
  EffectMode requestedMode{};
  if (!parseEffect(server.arg("effect"), requestedMode)) {
    error = F("unsupported effect"); return false;
  }
  next = fallback;
  next.mode = requestedMode;
  if (server.hasArg("brightness")) next.brightness = static_cast<uint8_t>(constrain(server.arg("brightness").toInt(), 0, 255));
  if (server.hasArg("color") && !parseHexColor(server.arg("color"), next.primaryColor.red, next.primaryColor.green, next.primaryColor.blue)) { error = F("color must be #RRGGBB"); return false; }
  if (server.hasArg("rainbowStyle")) next.settings.rainbowStyle = static_cast<uint8_t>(constrain(server.arg("rainbowStyle").toInt(), 0, 2));
  if (server.hasArg("flowSpeed")) next.settings.flowSpeed = static_cast<uint8_t>(constrain(server.arg("flowSpeed").toInt(), 1, 100));
  if (server.hasArg("style")) {
    if (requestedMode == EffectMode::Comet) next.cometColorStyle = static_cast<uint8_t>(constrain(server.arg("style").toInt(), 0, 4));
    else if (requestedMode == EffectMode::Music) next.audioColorStyle = static_cast<uint8_t>(constrain(server.arg("style").toInt(), 0, 4));
  }
  if (server.hasArg("cometTail")) next.settings.cometTail = static_cast<uint8_t>(constrain(server.arg("cometTail").toInt(), 5, 80));
  if (server.hasArg("cometSpeed")) next.settings.cometSpeed = static_cast<uint8_t>(constrain(server.arg("cometSpeed").toInt(), 1, 50));
  if (server.hasArg("cometDensity")) next.settings.cometDensity = static_cast<uint8_t>(constrain(server.arg("cometDensity").toInt(), 1, 8));
  if (server.hasArg("cometBackground")) next.settings.cometBackground = static_cast<uint8_t>(constrain(server.arg("cometBackground").toInt(), 0, 60));
  if (server.hasArg("effectFlags")) next.settings.cometRandom = (server.arg("effectFlags").toInt() & 1) != 0;
  if (server.hasArg("sensitivityX100")) next.settings.audioSensitivityX100 = static_cast<uint16_t>(constrain(server.arg("sensitivityX100").toInt(), 10, 500));
  if (server.hasArg("audioColorGainX100")) next.settings.audioColorGainX100 = static_cast<uint16_t>(constrain(server.arg("audioColorGainX100").toInt(), 0, 500));
  if (server.hasArg("audioHueDrive")) next.settings.audioHueDrive = static_cast<uint8_t>(constrain(server.arg("audioHueDrive").toInt(), 0, 3));
  if (server.hasArg("breathFloorPercent")) next.settings.breathFloorPercent = static_cast<uint8_t>(constrain(server.arg("breathFloorPercent").toInt(), 0, 60));
  if (server.hasArg("pulseAmplitudePercent")) next.settings.pulseAmplitudePercent = static_cast<uint8_t>(constrain(server.arg("pulseAmplitudePercent").toInt(), 0, 100));
  if (server.hasArg("pulseHeightPercent")) next.settings.pulseHeightPercent = static_cast<uint8_t>(constrain(server.arg("pulseHeightPercent").toInt(), 0, 100));
  if (server.hasArg("animationBrightness")) next.settings.animationBrightness = static_cast<uint8_t>(constrain(server.arg("animationBrightness").toInt(), 0, 255));
  if (server.hasArg("secondaryColor") && !parseHexColor(server.arg("secondaryColor"), next.settings.secondaryRed, next.settings.secondaryGreen, next.settings.secondaryBlue)) { error = F("secondaryColor must be #RRGGBB"); return false; }
  return true;
}

}  // namespace sozo::web
