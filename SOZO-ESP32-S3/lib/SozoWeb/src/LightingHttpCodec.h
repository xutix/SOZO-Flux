#pragma once

#include <WebServer.h>

#include <LightingScene.h>

namespace sozo::web {

const char *modeName(EffectMode mode);
const char *effectId(EffectMode mode);
bool parseLegacyMode(const String &value, EffectMode &mode);
String escapeJson(const String &value);
void appendLightingSceneJson(String &json, const LightingScene &scene);
bool parseLightingRequest(WebServer &server, LightingScene &next,
                          const LightingScene &fallback, bool allowNodeId,
                          String &error);

}  // namespace sozo::web
