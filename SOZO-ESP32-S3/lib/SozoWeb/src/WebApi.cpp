#include <WebApi.h>

#include <SpatialLightCore.h>

namespace {

using sozo::EffectMode;
using sozo::PersistedLightingState;

constexpr uint16_t kMaxLedCount = spatial_light::kMaxLedCount;

const char *modeName(const EffectMode mode) {
  switch (mode) {
    case EffectMode::Static:
      return "static";
    case EffectMode::Rainbow:
      return "rainbow";
    case EffectMode::Breathe:
      return "breathe";
    case EffectMode::Music:
      return "music";
    case EffectMode::Comet:
      return "comet";
    case EffectMode::Aurora:
      return "aurora";
    case EffectMode::FlameAudio:
      return "flame_audio";
    case EffectMode::GlassFlow:
      return "glass_flow";
    case EffectMode::CornerPulse:
      return "corner_pulse";
    case EffectMode::BassRipple:
      return "bass_ripple";
    case EffectMode::Focus:
      return "focus";
    case EffectMode::Off:
    default:
      return "off";
  }
}

const char *lightingEffectId(const EffectMode mode) {
  switch (mode) {
    case EffectMode::Static:
      return "SOLID";
    case EffectMode::Rainbow:
      return "RAINBOW";
    case EffectMode::Comet:
      return "COMET";
    case EffectMode::Music:
      return "AUDIO";
    case EffectMode::Breathe:
      return "BREATH";
    case EffectMode::Aurora:
      return "AURORA";
    case EffectMode::FlameAudio:
      return "FLAME_AUDIO";
    case EffectMode::GlassFlow:
      return "GLASS_FLOW";
    case EffectMode::CornerPulse:
      return "CORNER_PULSE";
    case EffectMode::BassRipple:
      return "BASS_RIPPLE";
    case EffectMode::Focus:
      return "FOCUS";
    case EffectMode::Off:
    default:
      return "OFF";
  }
}

bool parseMode(const String &value, EffectMode &mode) {
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

bool parseLightingEffect(const String &value, EffectMode &mode) {
  if (value == "SOLID") mode = EffectMode::Static;
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

String escapeJson(const String &value) {
  String escaped;
  escaped.reserve(value.length() + 8);
  for (size_t index = 0; index < value.length(); ++index) {
    const char character = value[index];
    switch (character) {
      case '\\':
        escaped += F("\\\\");
        break;
      case '"':
        escaped += F("\\\"");
        break;
      case '\n':
        escaped += F("\\n");
        break;
      case '\r':
        escaped += F("\\r");
        break;
      case '\t':
        escaped += F("\\t");
        break;
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

bool supportedLightingArgument(const String &name) {
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

String buildProvisioningPage() {
  static const char page[] PROGMEM = R"HTML(
<!doctype html><html lang="zh-CN"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>SOZO Flux 网络设置</title><style>
*{box-sizing:border-box}body{margin:0;min-height:100vh;background:#090d18;color:#f4f7ff;font:16px -apple-system,BlinkMacSystemFont,"Segoe UI","Microsoft YaHei",sans-serif;padding:18px}main{max-width:560px;margin:auto}.card{background:#151b2a;border:1px solid #28334c;border-radius:18px;padding:20px;margin:14px 0;box-shadow:0 12px 32px #0004}h1{font-size:1.45rem;margin:0 0 8px}p{color:#b8c2d6;line-height:1.55}.networks{display:grid;gap:8px;margin:14px 0}.network{border:1px solid #354362;border-radius:12px;background:#101727;color:#fff;padding:12px;text-align:left;cursor:pointer}.network.selected{border-color:#4f91ff;background:#18345f}.network small{display:block;color:#aebbd1;margin-top:4px}label{display:block;margin:15px 0 6px;color:#d7deec}input{width:100%;padding:12px;border-radius:10px;border:1px solid #3b4968;background:#0d1320;color:#fff;font-size:1rem}button{border:0;border-radius:11px;padding:12px;color:#fff;background:#3478f6;font-size:1rem;font-weight:650;cursor:pointer}button.secondary{background:#36435f}.actions{display:grid;grid-template-columns:1fr 1fr;gap:10px;margin-top:18px}.message{min-height:1.4em;margin-top:14px;color:#7ee9a6}.error{color:#ff9aa6}.muted{font-size:.9rem;color:#8fa0bd}
</style></head><body><main><section class="card"><h1>SOZO Flux 网络设置</h1><p>选择 2.4 GHz 路由器，输入密码并保存。设备重启后会接入该网络。</p><div id="networks" class="networks"><p class="muted">正在扫描附近网络…</p></div><button class="secondary" type="button" onclick="refreshNetworks()">重新扫描</button></section><section class="card"><form id="setupForm"><label for="ssid">路由器名称（SSID）</label><input id="ssid" maxlength="32" autocomplete="off" required><label for="password">路由器密码</label><input id="password" type="password" maxlength="63" autocomplete="current-password"><div class="actions"><button type="submit">保存并重启</button><button class="secondary" type="button" onclick="togglePassword()">显示密码</button></div><div id="message" class="message"></div></form></section></main><script>
const networkList=document.getElementById('networks'),ssidInput=document.getElementById('ssid'),passwordInput=document.getElementById('password'),message=document.getElementById('message');
function setMessage(text,error=false){message.textContent=text;message.classList.toggle('error',error)}
function selectNetwork(ssid,button){ssidInput.value=ssid;document.querySelectorAll('.network').forEach(x=>x.classList.remove('selected'));button.classList.add('selected')}
async function refreshNetworks(){setMessage('');networkList.innerHTML='<p class="muted">正在扫描附近网络…</p>';try{const response=await fetch('/api/wifi/scan',{cache:'no-store'}),data=await response.json();if(!data.ok)throw new Error(data.error||'扫描失败');networkList.innerHTML='';if(!data.networks.length){networkList.innerHTML='<p class="muted">没有发现网络，也可以手动输入名称。</p>';return}data.networks.forEach(network=>{const button=document.createElement('button');button.type='button';button.className='network';const name=document.createElement('strong'),details=document.createElement('small');name.textContent=network.ssid;details.textContent=network.rssi+' dBm · '+(network.secure?'需要密码':'开放网络');button.append(name,details);button.addEventListener('click',()=>selectNetwork(network.ssid,button));networkList.appendChild(button)})}catch(error){networkList.innerHTML='<p class="muted">扫描失败，请手动输入路由器名称。</p>';setMessage(error.message,true)}}
function togglePassword(){passwordInput.type=passwordInput.type==='password'?'text':'password'}
document.getElementById('setupForm').addEventListener('submit',async event=>{event.preventDefault();setMessage('正在保存网络设置…');try{const body=new URLSearchParams({ssid:ssidInput.value,password:passwordInput.value}),response=await fetch('/api/wifi/save',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body}),data=await response.json();if(!data.ok)throw new Error(data.error||'保存失败');setMessage('已保存。设备正在重启，请让手机或电脑连接到同一路由器后打开 sozo-flux.local。')}catch(error){setMessage(error.message,true)}});refreshNetworks();
</script></body></html>
)HTML";
  return FPSTR(page);
}

}  // namespace

namespace sozo {

WebApi::WebApi(CommandRouter &commands, NetworkManager &network,
               AudioAnalyzer &audio, NodeFleetCoordinator &nodes,
               const PageBuilder spatialPage,
               const RestartCallback restart)
    : server_(80),
      commands_(commands),
      network_(network),
      audio_(audio),
      nodes_(nodes),
      spatialPage_(spatialPage),
      restart_(restart) {}

void WebApi::begin() {
  registerRoutes();
  server_.begin();
}

void WebApi::handleClient() { server_.handleClient(); }

void WebApi::registerRoutes() {
  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/api/status", HTTP_GET, [this]() { handleApiStatus(); });
  server_.on("/api/nodes", HTTP_GET, [this]() { handleGetNodes(); });
  server_.on("/api/nodes/pairing", HTTP_POST,
             [this]() { handleOpenNodePairing(); });
  server_.on("/api/wifi/scan", HTTP_GET, [this]() { handleWiFiScan(); });
  server_.on("/api/wifi/save", HTTP_POST, [this]() { handleWiFiSave(); });
  server_.on("/api/wifi/reset", HTTP_GET, [this]() { handleWiFiReset(); });
  server_.on("/api/layout", HTTP_GET, [this]() { handleGetLayout(); });
  server_.on("/api/layout", HTTP_POST, [this]() { handleSetLayout(); });
  server_.on("/api/lighting", HTTP_POST, [this]() { handleSetLighting(); });
  server_.on("/api/node/mode", HTTP_POST,
             [this]() { handleSetNodeMode(); });
  server_.on("/api/node/lighting", HTTP_POST,
             [this]() { handleSetNodeLighting(); });
  server_.on("/api/node/led-count", HTTP_POST,
             [this]() { handleSetNodeLedCount(); });
  server_.on("/api/mode", HTTP_GET, [this]() { handleSetMode(); });
  server_.on("/api/color", HTTP_GET, [this]() { handleSetColor(); });
  server_.on("/api/brightness", HTTP_GET,
             [this]() { handleSetBrightness(); });
  server_.on("/api/effects", HTTP_GET, [this]() { handleSetEffects(); });
  server_.on("/api/startup", HTTP_GET, [this]() { handleSetStartup(); });
  server_.on("/api/audio", HTTP_GET, [this]() { handleSetAudio(); });
  server_.on("/api/restart", HTTP_GET, [this]() { handleRestart(); });
  server_.onNotFound([this]() {
    server_.send(404, "application/json",
                 "{\"ok\":false,\"error\":\"Not found\"}");
  });
}

void WebApi::sendApiResult(const bool ok, const char *message) {
  String json = F("{\"ok\":");
  json += ok ? F("true") : F("false");
  if (message[0] != '\0') {
    json += F(",\"error\":\"");
    json += message;
    json += '"';
  }
  json += '}';
  server_.send(ok ? 200 : 400, "application/json; charset=utf-8", json);
}

bool WebApi::dispatchWebControl(const ControlCommandType type,
                                const LightingParameter parameter,
                                const int32_t value, const Rgb color) {
  const ControlCommand command{
      kControlProtocolVersion, ControlSource::Web, 0, type, parameter, value,
      color, makeDefaultSpatialLayout()};
  return commands_.dispatch(command).accepted();
}

bool WebApi::dispatchWebLayout(const spatial_light::SpatialLayout &layout) {
  const ControlCommand command{
      kControlProtocolVersion,
      ControlSource::Web,
      0,
      ControlCommandType::SetLayout,
      LightingParameter::None,
      0,
      {0, 0, 0},
      layout,
  };
  return commands_.dispatch(command).accepted();
}

void WebApi::handleRoot() {
  if (isProvisioningNetworkState(network_.status().state)) {
    server_.send(200, "text/html; charset=utf-8", buildProvisioningPage());
    return;
  }
  server_.send(200, "text/html; charset=utf-8", spatialPage_());
}

void WebApi::handleWiFiScan() {
  if (!isProvisioningNetworkState(network_.status().state)) {
    sendApiResult(false, "Wi-Fi setup is unavailable");
    return;
  }
  const std::vector<ScannedNetwork> networks = network_.scan();
  String json = F("{\"ok\":true,\"networks\":[");
  for (size_t index = 0; index < networks.size(); ++index) {
    if (index > 0) json += ',';
    json += F("{\"ssid\":\"");
    json += escapeJson(networks[index].ssid);
    json += F("\",\"rssi\":");
    json += networks[index].rssi;
    json += F(",\"secure\":");
    json += networks[index].secure ? F("true") : F("false");
    json += '}';
  }
  json += F("]}");
  server_.send(200, "application/json; charset=utf-8", json);
}

void WebApi::handleWiFiSave() {
  if (!isProvisioningNetworkState(network_.status().state)) {
    sendApiResult(false, "Wi-Fi setup is unavailable");
    return;
  }
  if (!server_.hasArg("ssid") ||
      !network_.saveCredentials(server_.arg("ssid"), server_.arg("password"))) {
    sendApiResult(false, "SSID must be 1-32 chars; password at most 63 chars");
    return;
  }
  server_.send(200, "application/json; charset=utf-8",
               "{\"ok\":true,\"restarting\":true}");
  Serial.printf("[Wi-Fi] Saved credentials for SSID: %s. Restarting.\n",
                network_.status().ssid.c_str());
  restart_();
}

void WebApi::handleWiFiReset() {
  if (network_.status().state != NetworkState::Connected) {
    sendApiResult(false, "Wi-Fi reset is unavailable");
    return;
  }
  if (!network_.resetCredentials()) {
    sendApiResult(false, "Failed to clear Wi-Fi settings");
    return;
  }
  server_.send(200, "application/json; charset=utf-8",
               "{\"ok\":true,\"restarting\":true}");
  Serial.println(F("[Wi-Fi] Saved router credentials cleared. Restarting."));
  restart_();
}

String WebApi::buildLayoutJson() const {
  const StateSnapshot snapshot = commands_.snapshot();
  const spatial_light::SpatialLayout &layout = snapshot.lighting.layout;
  String json;
  json.reserve(180);
  json += F("{\"profile\":\"");
  json += layout.profile == spatial_light::LayoutProfile::Segmented
              ? "segmented"
              : "continuous";
  json += F("\",\"activeCount\":");
  json += layout.activeCount;
  json += F(",\"maxLedCount\":");
  json += kMaxLedCount;
  json += F(",\"centerIndex\":");
  json += layout.centerIndex;
  json += F(",\"leftCount\":");
  json += layout.leftCount;
  json += F(",\"centerCount\":");
  json += layout.centerCount;
  json += F(",\"rightCount\":");
  json += layout.rightCount;
  json += F(",\"reversed\":");
  json += layout.reversed ? F("true") : F("false");
  json += '}';
  return json;
}

String WebApi::buildLightingSettingsJson() const {
  const StateSnapshot snapshot = commands_.snapshot();
  const PersistedLightingState &state = snapshot.lighting;
  char colorHex[8];
  char secondaryHex[8];
  snprintf(colorHex, sizeof(colorHex), "#%02x%02x%02x", state.primaryColor.red,
           state.primaryColor.green, state.primaryColor.blue);
  snprintf(secondaryHex, sizeof(secondaryHex), "#%02x%02x%02x",
           state.lighting.secondaryRed, state.lighting.secondaryGreen,
           state.lighting.secondaryBlue);
  String json;
  json.reserve(500);
  json += F("{\"brightness\":");
  json += state.brightness;
  json += F(",\"color\":\"");
  json += colorHex;
  json += F("\",\"rainbowStyle\":");
  json += state.lighting.rainbowStyle;
  json += F(",\"flowSpeed\":");
  json += state.lighting.flowSpeed;
  json += F(",\"cometTail\":");
  json += state.lighting.cometTail;
  json += F(",\"cometSpeed\":");
  json += state.lighting.cometSpeed;
  json += F(",\"cometDensity\":");
  json += state.lighting.cometDensity;
  json += F(",\"cometBackground\":");
  json += state.lighting.cometBackground;
  json += F(",\"effectFlags\":");
  json += state.lighting.cometRandom ? 1 : 0;
  json += F(",\"sensitivityX100\":");
  json += state.lighting.audioSensitivityX100;
  json += F(",\"style\":");
  json += state.mode == EffectMode::Comet ? state.cometColorStyle
                                           : state.audioColorStyle;
  json += F(",\"audioColorGainX100\":");
  json += state.lighting.audioColorGainX100;
  json += F(",\"audioHueDrive\":");
  json += state.lighting.audioHueDrive;
  json += F(",\"breathFloorPercent\":");
  json += state.lighting.breathFloorPercent;
  json += F(",\"secondaryColor\":\"");
  json += secondaryHex;
  json += F("\",\"pulseAmplitudePercent\":");
  json += state.lighting.pulseAmplitudePercent;
  json += F(",\"pulseHeightPercent\":");
  json += state.lighting.pulseHeightPercent;
  json += F(",\"animationBrightness\":");
  json += state.lighting.animationBrightness;
  json += '}';
  return json;
}

void WebApi::handleGetLayout() {
  server_.send(200, "application/json; charset=utf-8", buildLayoutJson());
}

void WebApi::handleSetLayout() {
  if (!server_.hasArg("activeCount")) {
    sendApiResult(false, "activeCount is required");
    return;
  }
  const long requestedCount = server_.arg("activeCount").toInt();
  if (requestedCount < 1 || requestedCount > kMaxLedCount) {
    sendApiResult(false, "activeCount must be 1-1024");
    return;
  }
  const String profile =
      server_.hasArg("profile") ? server_.arg("profile") : "continuous";
  if (profile != "continuous" && profile != "segmented") {
    sendApiResult(false, "profile must be continuous or segmented");
    return;
  }
  const StateSnapshot snapshot = commands_.snapshot();
  spatial_light::SpatialLayout candidate = snapshot.lighting.layout;
  candidate.activeCount = static_cast<uint16_t>(requestedCount);
  candidate.reversed =
      server_.hasArg("reversed")
          ? server_.arg("reversed") == "1" || server_.arg("reversed") == "true"
          : snapshot.lighting.layout.reversed;
  if (profile == "segmented") {
    if (!server_.hasArg("leftCount") || !server_.hasArg("centerCount") ||
        !server_.hasArg("rightCount")) {
      sendApiResult(false, "segmented layout needs left, center and right");
      return;
    }
    const long left = server_.arg("leftCount").toInt();
    const long center = server_.arg("centerCount").toInt();
    const long right = server_.arg("rightCount").toInt();
    if (left < 0 || center < 1 || right < 0 || left > candidate.activeCount ||
        center > candidate.activeCount || right > candidate.activeCount ||
        !spatial_light::isValidSegmentedLayout(
            candidate.activeCount, static_cast<uint16_t>(left),
            static_cast<uint16_t>(center), static_cast<uint16_t>(right))) {
      sendApiResult(false, "segmented counts must equal activeCount");
      return;
    }
    candidate.profile = spatial_light::LayoutProfile::Segmented;
    candidate.leftCount = static_cast<uint16_t>(left);
    candidate.centerCount = static_cast<uint16_t>(center);
    candidate.rightCount = static_cast<uint16_t>(right);
  } else {
    candidate.profile = spatial_light::LayoutProfile::Continuous;
    const long center =
        server_.hasArg("centerIndex")
            ? server_.arg("centerIndex").toInt()
            : spatial_light::resolveCenterIndex(candidate.activeCount, 0, 0, 0,
                                                false);
    if (center < 0 || center >= candidate.activeCount) {
      sendApiResult(false, "centerIndex is outside activeCount");
      return;
    }
    candidate.centerIndex = static_cast<uint16_t>(center);
  }
  if (!dispatchWebLayout(candidate)) {
    sendApiResult(false, "layout command was rejected");
    return;
  }
  String json = F("{\"ok\":true,\"layout\":");
  json += buildLayoutJson();
  json += '}';
  server_.send(200, "application/json; charset=utf-8", json);
}

bool WebApi::parseLightingRequest(PersistedLightingState &next,
                                  EffectMode &requestedMode,
                                  const bool allowNodeId,
                                  String &error) {
  if (!server_.hasArg("effect")) {
    error = F("effect is required");
    return false;
  }
  for (uint8_t index = 0; index < server_.args(); ++index) {
    const String name = server_.argName(index);
    if ((allowNodeId && name == "id") || supportedLightingArgument(name)) {
      continue;
    }
    error = F("unsupported lighting parameter");
    return false;
  }
  if (server_.hasArg("audioSource") && server_.arg("audioSource") != "mic" &&
      server_.arg("audioSource") != "1") {
    error = F("only device microphone is available");
    return false;
  }
  if (!parseLightingEffect(server_.arg("effect"), requestedMode)) {
    error = F("unsupported effect");
    return false;
  }
  next = commands_.snapshot().lighting;
  next.mode = requestedMode;
  if (server_.hasArg("brightness"))
    next.brightness = static_cast<uint8_t>(
        constrain(server_.arg("brightness").toInt(), 0, 255));
  if (server_.hasArg("color") &&
      !parseHexColor(server_.arg("color"), next.primaryColor.red,
                     next.primaryColor.green, next.primaryColor.blue)) {
    error = F("color must be #RRGGBB");
    return false;
  }
  if (server_.hasArg("rainbowStyle"))
    next.lighting.rainbowStyle = static_cast<uint8_t>(
        constrain(server_.arg("rainbowStyle").toInt(), 0, 2));
  if (server_.hasArg("flowSpeed"))
    next.lighting.flowSpeed =
        static_cast<uint8_t>(constrain(server_.arg("flowSpeed").toInt(), 1, 100));
  if (server_.hasArg("style")) {
    if (requestedMode == EffectMode::Comet)
      next.cometColorStyle = static_cast<uint8_t>(
          constrain(server_.arg("style").toInt(), 0, 4));
    else if (requestedMode == EffectMode::Music)
      next.audioColorStyle = static_cast<uint8_t>(
          constrain(server_.arg("style").toInt(), 0, 4));
  }
  if (server_.hasArg("cometTail"))
    next.lighting.cometTail = static_cast<uint8_t>(
        constrain(server_.arg("cometTail").toInt(), 5, 80));
  if (server_.hasArg("cometSpeed"))
    next.lighting.cometSpeed = static_cast<uint8_t>(
        constrain(server_.arg("cometSpeed").toInt(), 1, 50));
  if (server_.hasArg("cometDensity"))
    next.lighting.cometDensity = static_cast<uint8_t>(
        constrain(server_.arg("cometDensity").toInt(), 1, 8));
  if (server_.hasArg("cometBackground"))
    next.lighting.cometBackground = static_cast<uint8_t>(
        constrain(server_.arg("cometBackground").toInt(), 0, 60));
  if (server_.hasArg("effectFlags"))
    next.lighting.cometRandom = (server_.arg("effectFlags").toInt() & 1) != 0;
  if (server_.hasArg("sensitivityX100"))
    next.lighting.audioSensitivityX100 = static_cast<uint16_t>(
        constrain(server_.arg("sensitivityX100").toInt(), 10, 500));
  if (server_.hasArg("audioColorGainX100"))
    next.lighting.audioColorGainX100 = static_cast<uint16_t>(
        constrain(server_.arg("audioColorGainX100").toInt(), 0, 500));
  if (server_.hasArg("audioHueDrive"))
    next.lighting.audioHueDrive = static_cast<uint8_t>(
        constrain(server_.arg("audioHueDrive").toInt(), 0, 3));
  if (server_.hasArg("breathFloorPercent"))
    next.lighting.breathFloorPercent = static_cast<uint8_t>(
        constrain(server_.arg("breathFloorPercent").toInt(), 0, 60));
  if (server_.hasArg("pulseAmplitudePercent"))
    next.lighting.pulseAmplitudePercent = static_cast<uint8_t>(
        constrain(server_.arg("pulseAmplitudePercent").toInt(), 0, 100));
  if (server_.hasArg("pulseHeightPercent"))
    next.lighting.pulseHeightPercent = static_cast<uint8_t>(
        constrain(server_.arg("pulseHeightPercent").toInt(), 0, 100));
  if (server_.hasArg("animationBrightness"))
    next.lighting.animationBrightness = static_cast<uint8_t>(
        constrain(server_.arg("animationBrightness").toInt(), 0, 255));
  if (server_.hasArg("secondaryColor") &&
      !parseHexColor(server_.arg("secondaryColor"), next.lighting.secondaryRed,
                     next.lighting.secondaryGreen,
                     next.lighting.secondaryBlue)) {
    error = F("secondaryColor must be #RRGGBB");
    return false;
  }
  return true;
}

bool parseNodeId(const String &value, sozo::node::NodeId &nodeId) {
  if (value.length() == 0) return false;
  char *end = nullptr;
  const unsigned long parsed = strtoul(value.c_str(), &end, 16);
  if (end == nullptr || *end != '\0' || parsed == 0UL ||
      parsed == sozo::node::kBroadcastNodeId ||
      parsed == sozo::node::kCoordinatorNodeId) {
    return false;
  }
  nodeId = static_cast<sozo::node::NodeId>(parsed);
  return true;
}

bool parseLedCount(const String &value, uint16_t &ledCount) {
  if (value.length() == 0U) return false;
  char *end = nullptr;
  const unsigned long parsed = strtoul(value.c_str(), &end, 10);
  if (end == nullptr || *end != '\0' || parsed == 0UL || parsed > 512UL) {
    return false;
  }
  ledCount = static_cast<uint16_t>(parsed);
  return true;
}

void WebApi::handleSetLighting() {
  PersistedLightingState next{};
  EffectMode requestedMode{};
  String error;
  if (!parseLightingRequest(next, requestedMode, false, error)) {
    sendApiResult(false, error.c_str());
    return;
  }
  const int32_t primaryColor =
      (static_cast<int32_t>(next.primaryColor.red) << 16) |
      (static_cast<int32_t>(next.primaryColor.green) << 8) |
      next.primaryColor.blue;
  const int32_t secondaryColor =
      (static_cast<int32_t>(next.lighting.secondaryRed) << 16) |
      (static_cast<int32_t>(next.lighting.secondaryGreen) << 8) |
      next.lighting.secondaryBlue;
  const bool accepted =
      dispatchWebControl(ControlCommandType::SetParameter,
                         LightingParameter::Brightness, next.brightness) &&
      dispatchWebControl(ControlCommandType::SetParameter,
                         LightingParameter::PrimaryColor, primaryColor,
                         next.primaryColor) &&
      dispatchWebControl(ControlCommandType::SetParameter,
                         LightingParameter::RainbowStyle,
                         next.lighting.rainbowStyle) &&
      dispatchWebControl(ControlCommandType::SetParameter,
                         LightingParameter::FlowSpeed, next.lighting.flowSpeed) &&
      dispatchWebControl(ControlCommandType::SetParameter,
                         LightingParameter::AudioColorStyle,
                         next.audioColorStyle) &&
      dispatchWebControl(ControlCommandType::SetParameter,
                         LightingParameter::CometColorStyle,
                         next.cometColorStyle) &&
      dispatchWebControl(ControlCommandType::SetParameter,
                         LightingParameter::CometTail, next.lighting.cometTail) &&
      dispatchWebControl(ControlCommandType::SetParameter,
                         LightingParameter::CometSpeed, next.lighting.cometSpeed) &&
      dispatchWebControl(ControlCommandType::SetParameter,
                         LightingParameter::CometDensity,
                         next.lighting.cometDensity) &&
      dispatchWebControl(ControlCommandType::SetParameter,
                         LightingParameter::CometBackground,
                         next.lighting.cometBackground) &&
      dispatchWebControl(ControlCommandType::SetParameter,
                         LightingParameter::CometRandom,
                         next.lighting.cometRandom ? 1 : 0) &&
      dispatchWebControl(ControlCommandType::SetParameter,
                         LightingParameter::AudioSensitivity,
                         next.lighting.audioSensitivityX100) &&
      dispatchWebControl(ControlCommandType::SetParameter,
                         LightingParameter::AudioColorGain,
                         next.lighting.audioColorGainX100) &&
      dispatchWebControl(ControlCommandType::SetParameter,
                         LightingParameter::AudioHueDrive,
                         next.lighting.audioHueDrive) &&
      dispatchWebControl(ControlCommandType::SetParameter,
                         LightingParameter::BreathFloor,
                         next.lighting.breathFloorPercent) &&
      dispatchWebControl(ControlCommandType::SetParameter,
                         LightingParameter::SecondaryColor, secondaryColor,
                         {next.lighting.secondaryRed,
                          next.lighting.secondaryGreen,
                          next.lighting.secondaryBlue}) &&
      dispatchWebControl(ControlCommandType::SetParameter,
                         LightingParameter::PulseAmplitude,
                         next.lighting.pulseAmplitudePercent) &&
      dispatchWebControl(ControlCommandType::SetParameter,
                         LightingParameter::PulseHeight,
                         next.lighting.pulseHeightPercent) &&
      dispatchWebControl(ControlCommandType::SetParameter,
                         LightingParameter::AnimationBrightness,
                         next.lighting.animationBrightness) &&
      dispatchWebControl(ControlCommandType::SetEffect, LightingParameter::None,
                         static_cast<int32_t>(requestedMode));
  if (!accepted) {
    sendApiResult(false, "lighting command was rejected");
    return;
  }
  const StateSnapshot updated = commands_.snapshot();
  String json = F("{\"ok\":true,\"effect\":\"");
  json += lightingEffectId(updated.lighting.mode);
  json += F("\",\"settings\":");
  json += buildLightingSettingsJson();
  json += '}';
  server_.send(200, "application/json; charset=utf-8", json);
}

void WebApi::handleSetNodeMode() {
  node::NodeId nodeId{0};
  if (!server_.hasArg("id") || !server_.hasArg("mode") ||
      !parseNodeId(server_.arg("id"), nodeId)) {
    sendApiResult(false, "valid node id and mode are required");
    return;
  }

  node::NodeControlMode mode{};
  if (server_.arg("mode") == "follow") {
    mode = node::NodeControlMode::FollowMain;
  } else if (server_.arg("mode") == "independent") {
    mode = node::NodeControlMode::Independent;
  } else {
    sendApiResult(false, "mode must be follow or independent");
    return;
  }

  if (!nodes_.requestNodeControlMode(nodeId, mode, millis())) {
    sendApiResult(false, "selected node is unavailable or busy");
    return;
  }
  String json = F("{\"ok\":true,\"pending\":true,\"mode\":\"");
  json += mode == node::NodeControlMode::Independent ? F("independent")
                                                      : F("follow");
  json += F("\"}");
  server_.send(202, "application/json; charset=utf-8", json);
}

void WebApi::handleSetNodeLighting() {
  node::NodeId nodeId{0};
  if (!server_.hasArg("id") || !parseNodeId(server_.arg("id"), nodeId)) {
    sendApiResult(false, "valid node id is required");
    return;
  }

  PersistedLightingState next{};
  EffectMode requestedMode{};
  String error;
  if (!parseLightingRequest(next, requestedMode, true, error)) {
    sendApiResult(false, error.c_str());
    return;
  }
  if (!nodes_.requestIndependentScene(nodeId, next, millis())) {
    sendApiResult(false, "node must be online and in independent mode");
    return;
  }

  String json = F("{\"ok\":true,\"pending\":true,\"effect\":\"");
  json += lightingEffectId(requestedMode);
  json += F("\"}");
  server_.send(202, "application/json; charset=utf-8", json);
}

void WebApi::handleSetNodeLedCount() {
  node::NodeId nodeId{0};
  uint16_t ledCount{0};
  if (!server_.hasArg("id") || !server_.hasArg("ledCount") ||
      !parseNodeId(server_.arg("id"), nodeId) ||
      !parseLedCount(server_.arg("ledCount"), ledCount)) {
    sendApiResult(false, "ledCount must be an integer from 1 to 512");
    return;
  }
  if (!nodes_.requestNodeLedCount(nodeId, ledCount, millis())) {
    sendApiResult(false, "selected node is unavailable, busy, or rejected the count");
    return;
  }
  String json = F("{\"ok\":true,\"pending\":true,\"ledCount\":");
  json += ledCount;
  json += '}';
  server_.send(202, "application/json; charset=utf-8", json);
}

void WebApi::handleApiStatus() {
  const StateSnapshot snapshot = commands_.snapshot();
  const PersistedLightingState &state = snapshot.lighting;
  const AudioSnapshot &audio = audio_.snapshot();
  const AudioTuning &tuning = audio_.tuning();
  const NetworkStatus &network = network_.status();
  char staticColorHex[8];
  char startupColorHex[8];
  snprintf(staticColorHex, sizeof(staticColorHex), "#%02x%02x%02x",
           state.primaryColor.red, state.primaryColor.green, state.primaryColor.blue);
  snprintf(startupColorHex, sizeof(startupColorHex), "#%02x%02x%02x",
           state.startupColor.red, state.startupColor.green, state.startupColor.blue);
  String json;
  json.reserve(1700);
  json += F("{\"wifi\":");
  json += network.state != NetworkState::Failed ? F("true") : F("false");
  json += F(",\"wifiState\":\"");
  json += networkStateName(network.state);
  json += F("\",\"wifiMode\":\"");
  json += isProvisioningNetworkState(network.state) ? "AP" : "STA";
  json += F("\",\"ssid\":\"");
  json += escapeJson(network.ssid);
  json += '"';
  json += F(",\"mdns\":\"");
  if (network.mdnsAvailable) {
    json += network.mdnsHostname;
    json += F(".local");
  }
  json += '"';
  json += F(",\"mode\":\"");
  json += modeName(state.mode);
  json += F("\",\"brightness\":");
  json += state.brightness;
  json += F(",\"ledCount\":");
  json += state.layout.activeCount;
  json += F(",\"maxLedCount\":");
  json += kMaxLedCount;
  json += F(",\"audioStyle\":");
  json += state.audioColorStyle;
  json += F(",\"cometStyle\":");
  json += state.cometColorStyle;
  json += F(",\"staticColor\":\"");
  json += staticColorHex;
  json += F("\",\"startupColor\":\"");
  json += startupColorHex;
  json += F("\",\"startupSpeed\":");
  json += String(state.startupAnimationSpeed, 1);
  json += F(",\"volume\":");
  json += String(audio.frame.volume, 1);
  json += F(",\"micAvailable\":");
  json += audio.frame.available ? F("true") : F("false");
  json += F(",\"audioFrames\":");
  json += audio.frame.framesRead;
  json += F(",\"rawLeftRms\":");
  json += String(audio.rawLeftRms, 1);
  json += F(",\"rawRightRms\":");
  json += String(audio.rawRightRms, 1);
  json += F(",\"selectedRawRms\":");
  json += String(audio.selectedRawRms, 1);
  json += F(",\"audioGain\":");
  json += String(tuning.gain, 1);
  json += F(",\"noiseFloor\":");
  json += String(tuning.noiseFloor, 0);
  json += F(",\"fullScale\":");
  json += String(tuning.fullScale, 0);
  json += F(",\"attack\":");
  json += String(tuning.attack, 2);
  json += F(",\"release\":");
  json += String(tuning.release, 2);
  json += F(",\"beatSensitivity\":");
  json += String(tuning.beatSensitivity, 2);
  json += F(",\"beatBoost\":");
  json += String(tuning.beatBoost, 0);
  json += F(",\"beatPulse\":");
  json += String(audio.frame.beatPulse, 1);
  json += F(",\"freeHeap\":");
  json += ESP.getFreeHeap();
  json += F(",\"flashSize\":");
  json += ESP.getFlashChipSize() / (1024U * 1024U);
  json += F(",\"psramSize\":");
  json += psramFound() ? ESP.getPsramSize() / (1024U * 1024U) : 0;
  json += F(",\"ip\":\"");
  json += network.ip;
  json += F("\",\"effect\":\"");
  json += lightingEffectId(state.mode);
  json += F("\",\"settings\":");
  json += buildLightingSettingsJson();
  json += F(",\"layout\":");
  json += buildLayoutJson();
  json += '}';
  server_.send(200, "application/json; charset=utf-8", json);
}

void WebApi::handleGetNodes() {
  const auto transportStateName =
      [](const NodeTransportState state) -> const char * {
    switch (state) {
      case NodeTransportState::Idle:
        return "idle";
      case NodeTransportState::Searching:
        return "searching";
      case NodeTransportState::Connecting:
        return "connecting";
      case NodeTransportState::Discovering:
        return "discovering";
      case NodeTransportState::Authenticating:
        return "authenticating";
      case NodeTransportState::Ready:
        return "ready";
      case NodeTransportState::Backoff:
      default:
        return "backoff";
    }
  };
  const auto connectionStateName =
      [](const NodeConnectionState state) -> const char * {
    switch (state) {
      case NodeConnectionState::Discovered:
        return "discovered";
      case NodeConnectionState::Connecting:
        return "connecting";
      case NodeConnectionState::Ready:
        return "ready";
      case NodeConnectionState::Offline:
        return "offline";
      case NodeConnectionState::Error:
      default:
        return "error";
    }
  };

  String json;
  json.reserve(1100);
  json += F("{\"ok\":true,\"bleState\":\"");
  json += transportStateName(nodes_.transportState());
  json += F("\",\"ready\":");
  json += nodes_.nodeReady() ? F("true") : F("false");
  json += F(",\"operationStage\":\"");
  json += nodes_.operationName();
  json += F("\",\"workerBusy\":");
  json += nodes_.workerBusy() ? F("true") : F("false");
  json += F(",\"bleTimeouts\":");
  json += nodes_.timeoutCount();
  json += F(",\"knownCount\":");
  json += nodes_.registry().size();
  json += F(",\"onlineCount\":");
  json += nodes_.onlineCount();
  json += F(",\"capacity\":");
  json += nodes_.capacity();
  json += F(",\"scanning\":");
  json += nodes_.scanning() ? F("true") : F("false");
  json += F(",\"pairingWindowOpen\":");
  json += nodes_.pairingWindowOpen(millis()) ? F("true") : F("false");
  json += F(",\"pairingRemainingMs\":");
  json += nodes_.pairingRemainingMs(millis());
  json += F(",\"nodes\":[");
  bool first = true;
  const NodeRegistry &registry = nodes_.registry();
  for (size_t index = 0; index < NodeRegistry::kCapacity; ++index) {
    const NodeRecord *record = registry.recordAt(index);
    if (record == nullptr) continue;
    if (!first) json += ',';
    first = false;
    char nodeId[9]{};
    snprintf(nodeId, sizeof(nodeId), "%08lx",
             static_cast<unsigned long>(record->nodeId));
    json += F("{\"id\":\"");
    json += nodeId;
    json += F("\",\"state\":\"");
    json += connectionStateName(record->connectionState);
    json += F("\",\"capabilities\":");
    json += record->capabilities.capabilityBits;
    json += F(",\"ledCount\":");
    json += record->status.ledCount;
    json += F(",\"bound\":");
    json += record->capabilities.bound ? F("true") : F("false");
    json += F(",\"controlMode\":");
    json += static_cast<unsigned int>(record->status.controlMode);
    json += F(",\"lastLinkChangeMs\":");
    json += record->lastLinkChangeMs;
    json += F(",\"lastReceiptMs\":");
    json += record->lastReceiptMs;
    json += F(",\"lastStatusMs\":");
    json += record->lastStatusMs;
    json += F(",\"lastSceneRevision\":");
    json += record->lastAppliedSceneRevision;
    json += F(",\"lastCommandError\":");
    json += record->lastCommandError;
    json += F(",\"freeHeap\":");
    json += record->status.freeHeapBytes;
    json += F(",\"pairingWindowOpen\":");
    json += record->status.pairingWindowOpen ? F("true") : F("false");
    json += '}';
  }
  json += F("]}");
  server_.send(200, "application/json; charset=utf-8", json);
}

void WebApi::handleOpenNodePairing() {
  const uint32_t nowMs = millis();
  if (!nodes_.openPairingWindow(nowMs)) {
    sendApiResult(false, "No free node connection slot");
    return;
  }
  String json;
  json.reserve(96);
  json += F("{\"ok\":true,\"pairingWindowOpen\":true,\"durationMs\":");
  json += NodeFleetCoordinator::kDefaultPairingWindowMs;
  json += '}';
  server_.send(200, "application/json; charset=utf-8", json);
}

void WebApi::handleSetMode() {
  EffectMode requestedMode;
  if (!server_.hasArg("value")) {
    sendApiResult(false, "Missing mode");
    return;
  }
  const String requestedValue = server_.arg("value");
  bool turnOn = false;
  if (requestedValue == "on") {
    turnOn = true;
    requestedMode = EffectMode::Static;
  } else if (!parseMode(requestedValue, requestedMode)) {
    sendApiResult(false, "Invalid mode");
    return;
  }
  const bool accepted =
      turnOn
          ? dispatchWebControl(ControlCommandType::TurnOn)
          : (requestedMode == EffectMode::Off
                 ? dispatchWebControl(ControlCommandType::TurnOff)
                 : dispatchWebControl(ControlCommandType::SetEffect,
                                      LightingParameter::None,
                                      static_cast<int32_t>(requestedMode)));
  if (!accepted) {
    sendApiResult(false, "mode command was rejected");
    return;
  }
  sendApiResult(true);
}

void WebApi::handleSetColor() {
  if (!server_.hasArg("r") || !server_.hasArg("g") || !server_.hasArg("b")) {
    sendApiResult(false, "Missing r, g or b");
    return;
  }
  const Rgb color = {
      static_cast<uint8_t>(constrain(server_.arg("r").toInt(), 0, 255)),
      static_cast<uint8_t>(constrain(server_.arg("g").toInt(), 0, 255)),
      static_cast<uint8_t>(constrain(server_.arg("b").toInt(), 0, 255))};
  EffectMode retainedMode = EffectMode::Static;
  if (server_.hasArg("keepMode")) {
    EffectMode parsedMode;
    if (parseMode(server_.arg("keepMode"), parsedMode) &&
        parsedMode != EffectMode::Off && parsedMode != EffectMode::Rainbow)
      retainedMode = parsedMode;
  }
  const int32_t packedColor = (static_cast<int32_t>(color.red) << 16) |
                              (static_cast<int32_t>(color.green) << 8) |
                              color.blue;
  if (!dispatchWebControl(ControlCommandType::SetParameter,
                          LightingParameter::PrimaryColor, packedColor, color) ||
      !dispatchWebControl(ControlCommandType::SetEffect,
                          LightingParameter::None,
                          static_cast<int32_t>(retainedMode))) {
    sendApiResult(false, "color command was rejected");
    return;
  }
  sendApiResult(true);
}

void WebApi::handleSetBrightness() {
  if (!server_.hasArg("value")) {
    sendApiResult(false, "Missing brightness");
    return;
  }
  const int32_t brightness = constrain(server_.arg("value").toInt(), 1, 255);
  if (!dispatchWebControl(ControlCommandType::SetParameter,
                          LightingParameter::Brightness, brightness)) {
    sendApiResult(false, "brightness command was rejected");
    return;
  }
  sendApiResult(true);
}

void WebApi::handleSetEffects() {
  bool accepted = true;
  if (server_.hasArg("audioStyle"))
    accepted = dispatchWebControl(
        ControlCommandType::SetParameter, LightingParameter::AudioColorStyle,
        constrain(server_.arg("audioStyle").toInt(), 0, 4));
  if (accepted && server_.hasArg("cometStyle"))
    accepted = dispatchWebControl(
        ControlCommandType::SetParameter, LightingParameter::CometColorStyle,
        constrain(server_.arg("cometStyle").toInt(), 0, 4));
  if (!accepted) {
    sendApiResult(false, "effect style command was rejected");
    return;
  }
  sendApiResult(true);
}

void WebApi::handleSetStartup() {
  PersistedLightingState next = commands_.snapshot().lighting;
  if (server_.hasArg("r"))
    next.startupColor.red =
        static_cast<uint8_t>(constrain(server_.arg("r").toInt(), 0, 255));
  if (server_.hasArg("g"))
    next.startupColor.green =
        static_cast<uint8_t>(constrain(server_.arg("g").toInt(), 0, 255));
  if (server_.hasArg("b"))
    next.startupColor.blue =
        static_cast<uint8_t>(constrain(server_.arg("b").toInt(), 0, 255));
  if (server_.hasArg("speed"))
    next.startupAnimationSpeed =
        constrain(server_.arg("speed").toFloat(), 0.1F, 5.0F);
  const int32_t startupColor =
      (static_cast<int32_t>(next.startupColor.red) << 16) |
      (static_cast<int32_t>(next.startupColor.green) << 8) |
      next.startupColor.blue;
  const int32_t startupSpeed =
      static_cast<int32_t>(next.startupAnimationSpeed * 100.0F + 0.5F);
  if (!dispatchWebControl(ControlCommandType::SetParameter,
                          LightingParameter::StartupColor, startupColor,
                          next.startupColor) ||
      !dispatchWebControl(ControlCommandType::SetParameter,
                          LightingParameter::StartupAnimationSpeed,
                          startupSpeed)) {
    sendApiResult(false, "startup command was rejected");
    return;
  }
  sendApiResult(true);
}

void WebApi::handleSetAudio() {
  AudioTuning tuning = audio_.tuning();
  if (server_.hasArg("gain"))
    tuning.gain = constrain(server_.arg("gain").toFloat(), 0.1F, 20.0F);
  if (server_.hasArg("noise"))
    tuning.noiseFloor =
        constrain(server_.arg("noise").toFloat(), 0.0F, 500000.0F);
  if (server_.hasArg("fullScale"))
    tuning.fullScale =
        constrain(server_.arg("fullScale").toFloat(), 500.0F, 1000000.0F);
  if (server_.hasArg("attack"))
    tuning.attack = constrain(server_.arg("attack").toFloat() / 100.0F, 0.01F,
                              1.0F);
  if (server_.hasArg("release"))
    tuning.release = constrain(server_.arg("release").toFloat() / 100.0F,
                               0.01F, 1.0F);
  if (server_.hasArg("beatSensitivity"))
    tuning.beatSensitivity =
        constrain(server_.arg("beatSensitivity").toFloat(), 1.05F, 3.0F);
  if (server_.hasArg("beatBoost"))
    tuning.beatBoost =
        constrain(server_.arg("beatBoost").toFloat(), 20.0F, 255.0F);
  audio_.setTuning(tuning);
  commands_.setAudioTuning(tuning);
  sendApiResult(true);
}

void WebApi::handleRestart() {
  server_.send(200, "application/json; charset=utf-8",
               "{\"ok\":true,\"restarting\":true}");
  Serial.println(F("[SYSTEM] Restart requested from web interface."));
  restart_();
}

}  // namespace sozo
