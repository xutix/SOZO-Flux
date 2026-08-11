#include <Arduino.h>
#include <ArduinoOTA.h>
#include <Adafruit_NeoPixel.h>
#include <SpatialLightCore.h>

#include <AudioAnalyzer.h>
#include <BleFleetAdapter.h>
#include <CommandRouter.h>
#include <LightNodeRuntime.h>
#include <LightingController.h>
#include <LightingControllerNodeSink.h>
#include <LightingSceneOrchestrator.h>
#include <LightingSceneStore.h>
#include <NetworkManager.h>
#include <NodeNameStore.h>
#include <NodeFleetCoordinator.h>
#include <SceneDeliveryCoordinator.h>
#include <SerialConsole.h>
#include <SettingsStore.h>
#include <S3LightingOutput.h>
#include <SpaceSceneCoordinator.h>
#include <SozoDomain.h>
#include <SozoVersion.h>
#include <WebApi.h>

#include "SpatialLightPage.h"

namespace {

constexpr char kProjectName[] = "SOZO Flux Gateway S3";
constexpr char kOtaHostname[] = "sozo-flux";
#ifndef SOZO_OTA_PASSWORD
#define SOZO_OTA_PASSWORD ""
#endif
#ifndef SOZO_LOCAL_LIGHT_ENABLED
#define SOZO_LOCAL_LIGHT_ENABLED 1
#endif
constexpr char kOtaPassword[] = SOZO_OTA_PASSWORD;
constexpr uint8_t kLedPin = 18;
constexpr uint8_t kStatusLedPin = 48;
constexpr uint8_t kStatusLedCount = 1;
constexpr uint8_t kStatusLedBrightness = 60;
constexpr uint32_t kSerialReportIntervalMs = 500;

sozo::SettingsStore settingsStore;
sozo::NodeNameStore nodeNameStore;
sozo::LightingSceneStore lightingSceneStore;
sozo::LightingSceneOrchestrator lightingScenes;
sozo::NetworkManager networkManager(settingsStore);
sozo::SpaceSceneCoordinator sceneCoordinator;
#if SOZO_LOCAL_LIGHT_ENABLED
sozo::s3::S3LightingOutput lightingOutput(spatial_light::kMaxLedCount,
                                          kLedPin);
sozo::LightingController lightingController(lightingOutput);
sozo::LightingControllerNodeSink localLightSink(lightingController);
sozo::LightNodeRuntime localLightNode(localLightSink);

class LocalLightingTargetAdapter final : public sozo::LocalLightingTarget {
 public:
  explicit LocalLightingTargetAdapter(sozo::LightNodeRuntime &runtime)
      : runtime_(runtime) {}
  bool available() const override { return true; }
  bool apply(const sozo::LightingScene &scene, const uint32_t revision,
             const uint32_t nowMs) override {
    runtime_.setControlMode(sozo::node::NodeControlMode::Independent);
    const sozo::LightSceneApplyResult result = runtime_.applyScene(
        scene, revision, nowMs, nowMs, sozo::LightSceneTarget::Independent);
    return result == sozo::LightSceneApplyResult::Applied ||
           result == sozo::LightSceneApplyResult::Duplicate;
  }

 private:
  sozo::LightNodeRuntime &runtime_;
};

LocalLightingTargetAdapter localLightingTarget(localLightNode);
#else
class LocalLightingTargetAdapter final : public sozo::LocalLightingTarget {
 public:
  bool available() const override { return false; }
  bool apply(const sozo::LightingScene &, uint32_t, uint32_t) override {
    return false;
  }
};

LocalLightingTargetAdapter localLightingTarget;
#endif
sozo::AudioAnalyzer audioAnalyzer;
sozo::CommandRouter commandRouter(sceneCoordinator, settingsStore);
sozo::BleFleetAdapter bleNodeTransport;
sozo::NodeFleetCoordinator nodeCoordinator(bleNodeTransport);
sozo::SceneDeliveryCoordinator sceneDelivery(lightingScenes,
                                              localLightingTarget,
                                              nodeCoordinator);
sozo::SerialConsole serialConsole(Serial, commandRouter);
Adafruit_NeoPixel statusLed(kStatusLedCount, kStatusLedPin,
                            NEO_GRB + NEO_KHZ800);

void restartDevice();
sozo::WebApi webApi(commandRouter, networkManager, audioAnalyzer,
                    nodeCoordinator, nodeNameStore, lightingScenes,
                    lightingSceneStore,
                    buildSpatialLightPage, restartDevice);

uint32_t lastSerialReport = 0;
uint32_t lastLocalLightConfigRevision = 0U;
uint32_t lastLegacySceneRevision = 0U;
bool otaAvailable = false;
uint8_t lastOtaProgressPercent = 255;

const char *modeName(const sozo::EffectMode mode) {
  switch (mode) {
    case sozo::EffectMode::Static:
      return "static";
    case sozo::EffectMode::Rainbow:
      return "rainbow";
    case sozo::EffectMode::Breathe:
      return "breathe";
    case sozo::EffectMode::Music:
      return "music";
    case sozo::EffectMode::Comet:
      return "comet";
    case sozo::EffectMode::Aurora:
      return "aurora";
    case sozo::EffectMode::FlameAudio:
      return "flame_audio";
    case sozo::EffectMode::GlassFlow:
      return "glass_flow";
    case sozo::EffectMode::CornerPulse:
      return "corner_pulse";
    case sozo::EffectMode::BassRipple:
      return "bass_ripple";
    case sozo::EffectMode::Focus:
      return "focus";
    case sozo::EffectMode::Off:
    default:
      return "off";
  }
}

void restartDevice() {
  delay(250);
  ESP.restart();
}

void initOta() {
  if (networkManager.status().state != sozo::NetworkState::Connected) {
    Serial.println(F("[BOOT] PlatformIO OTA disabled outside Wi-Fi STA mode."));
    return;
  }

  ArduinoOTA.setHostname(kOtaHostname);
  if (kOtaPassword[0] != '\0') ArduinoOTA.setPassword(kOtaPassword);
  // NetworkManager already owns the sozo-flux.local mDNS responder.
  ArduinoOTA.setMdnsEnabled(false);
  ArduinoOTA.onStart([]() {
    lastOtaProgressPercent = 255;
    Serial.println(F("[OTA] Firmware upload started."));
  });
  ArduinoOTA.onProgress([](const unsigned int progress,
                           const unsigned int total) {
    const uint8_t percent =
        total == 0U ? 0U : static_cast<uint8_t>((progress * 100U) / total);
    if (percent == lastOtaProgressPercent || percent % 10U != 0U) return;
    lastOtaProgressPercent = percent;
    Serial.printf("[OTA] Progress: %u%%\n", percent);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println(F("[OTA] Upload complete; restarting with the new firmware."));
  });
  ArduinoOTA.onError([](const ota_error_t error) {
    Serial.printf("[OTA] Upload failed, error=%u.\n",
                  static_cast<unsigned int>(error));
  });
  ArduinoOTA.begin();
  otaAvailable = true;
  Serial.printf("[BOOT] PlatformIO OTA ready: %s.local:3232 (password %s).\n",
                kOtaHostname, kOtaPassword[0] != '\0' ? "enabled" : "disabled");
}

void handleOta() {
  if (otaAvailable) ArduinoOTA.handle();
}

void initStatusLed() {
  statusLed.begin();
  statusLed.setBrightness(kStatusLedBrightness);
  statusLed.clear();
  statusLed.show();
}

void initLocalLightNode() {
#if SOZO_LOCAL_LIGHT_ENABLED
  const sozo::PersistedLightingState state = sceneCoordinator.lightingState();
  const sozo::SpaceSceneSnapshot &scene = sceneCoordinator.snapshot();
  const uint32_t now = millis();
  localLightNode.begin(state);
  localLightNode.setControlMode(sozo::node::NodeControlMode::Independent);
  lastLocalLightConfigRevision =
      sceneCoordinator.localLightConfiguration().revision;
#endif
}

void loadConfiguration() {
  const sozo::PersistedLightingState state = settingsStore.loadLightingState();
  sceneCoordinator.begin(state);
  audioAnalyzer.setTuning(state.audio);
  lastLegacySceneRevision = sceneCoordinator.snapshot().revision;
  if (!lightingSceneStore.load(lightingScenes)) {
#if SOZO_LOCAL_LIGHT_ENABLED
    sozo::NamedLightingScene defaultScene{};
    defaultScene.id = 1U;
    defaultScene.setName("默认空间");
    defaultScene.assignments[0] = {sozo::kLocalLightingTargetId,
                                   sceneCoordinator.snapshot().lighting};
    defaultScene.assignmentCount = 1U;
    lightingScenes.saveScene(defaultScene);
    lightingScenes.activateScene(defaultScene.id);
    lightingSceneStore.save(lightingScenes);
#endif
  } else if (
#if SOZO_LOCAL_LIGHT_ENABLED
      lightingScenes.desiredFor(sozo::kLocalLightingTargetId) == nullptr
#else
      false
#endif
  ) {
    lightingScenes.applyDirect(sozo::kLocalLightingTargetId,
                               sceneCoordinator.snapshot().lighting);
    lightingSceneStore.save(lightingScenes);
  }
}

void saveConfigurationIfReady() {
  const uint32_t now = millis();
  commandRouter.tick(now);
  lightingSceneStore.tick(now, lightingScenes);
}

void printSystemInfo() {
  const bool hasPsram = psramFound();
  const sozo::NetworkStatus &network = networkManager.status();
  Serial.println();
  Serial.println(F("================================================"));
  Serial.printf("Project: %s\n", kProjectName);
  Serial.printf("Firmware: %s | Platform: %s | Protocol: %u\n",
                sozo::version::kGatewayS3, sozo::version::kPlatform,
                sozo::node::kProtocolVersion);
  Serial.printf("Wi-Fi state: %s | Mode: %s | SSID: %s | IP: %s\n",
                sozo::networkStateName(network.state),
                sozo::isProvisioningNetworkState(network.state) ? "AP" : "STA",
                network.ssid.c_str(), network.ip.c_str());
  Serial.printf("Flash size: %lu MB\n",
                static_cast<unsigned long>(
                    ESP.getFlashChipSize() / (1024U * 1024U)));
  Serial.printf("PSRAM available: %s\n", hasPsram ? "Yes" : "No");
  Serial.printf("PSRAM size: %lu MB\n",
                static_cast<unsigned long>(
                    hasPsram ? ESP.getPsramSize() / (1024U * 1024U) : 0));
  Serial.printf("Free heap: %lu bytes\n",
                static_cast<unsigned long>(ESP.getFreeHeap()));
  Serial.printf("INMP441 analyzer: %s\n",
                audioAnalyzer.snapshot().frame.available ? "READY" : "ERROR");
#if SOZO_LOCAL_LIGHT_ENABLED
  Serial.printf("Local light node: GPIO%u, active=%u, maximum=%u pixels\n",
                kLedPin,
                sceneCoordinator.localLightConfiguration().layout.activeCount,
                spatial_light::kMaxLedCount);
#else
  Serial.println(F("Local light node: NOT INSTALLED"));
#endif
  Serial.printf("Onboard status WS2812: GPIO%u (disabled)\n", kStatusLedPin);
  Serial.printf("SOZO Flux node bus: %u/%u nodes online\n",
                static_cast<unsigned int>(nodeCoordinator.onlineCount()),
                static_cast<unsigned int>(nodeCoordinator.capacity()));
  Serial.printf("PlatformIO OTA: %s\n", otaAvailable ? "READY" : "DISABLED");
  if (sozo::isProvisioningNetworkState(network.state)) {
    Serial.println(
        F("Connect to SOZO-FLUX-SETUP and open http://192.168.4.1."));
  } else if (network.state == sozo::NetworkState::Connected) {
    if (network.mdnsAvailable) {
      Serial.printf("Open http://%s.local (fallback: http://%s).\n",
                    network.mdnsHostname.c_str(), network.ip.c_str());
    } else {
      Serial.printf("Open http://%s in your browser.\n", network.ip.c_str());
    }
  }
  Serial.println(F("================================================"));
}

void printDeviceStatus() {
  const bool hasPsram = psramFound();
  const sozo::AudioSnapshot &audio = audioAnalyzer.snapshot();
  const sozo::AudioTuning &tuning = audioAnalyzer.tuning();
  const sozo::NetworkStatus &network = networkManager.status();
  const sozo::PersistedLightingState lighting =
      sceneCoordinator.lightingState();
  Serial.println();
  Serial.println(F("---------------- DEVICE STATUS ----------------"));
  Serial.printf("[Wi-Fi] State: %s | Mode: %s | SSID: %s | IP: %s",
                sozo::networkStateName(network.state),
                sozo::isProvisioningNetworkState(network.state) ? "AP" : "STA",
                network.ssid.c_str(), network.ip.c_str());
  if (sozo::isProvisioningNetworkState(network.state)) {
    Serial.printf(" | Clients: %u", network.clientCount);
  } else if (network.state == sozo::NetworkState::Connected) {
    Serial.printf(" | RSSI: %d dBm", network.rssi);
    if (network.mdnsAvailable) {
      Serial.printf(" | mDNS: %s.local", network.mdnsHostname.c_str());
    }
  }
  Serial.println();
  Serial.printf(
      "[MIC] I2S: %s | Frames: %lu | Channel: LEFT (L/R=GND) | "
      "RMS: %.1f | Volume: %.1f | Beat: %.1f\n",
      audio.frame.available ? "READY" : "ERROR",
      static_cast<unsigned long>(audio.frame.framesRead), audio.selectedRawRms,
      audio.frame.volume, audio.frame.beatPulse);
  Serial.printf(
      "[MIC] Gain: %.1fx | Noise floor: %.0f | Full scale: %.0f | "
      "Attack: %.0f%% | Release: %.0f%%\n",
      tuning.gain, tuning.noiseFloor, tuning.fullScale, tuning.attack * 100.0F,
      tuning.release * 100.0F);
#if SOZO_LOCAL_LIGHT_ENABLED
  Serial.printf(
      "[NODE local-light] READY | GPIO: %u | Active: %u/%u | Mode: %s | "
      "Brightness: %u\n",
      kLedPin, lighting.layout.activeCount, spatial_light::kMaxLedCount,
      modeName(lighting.mode), lighting.brightness);
#else
  Serial.println(F("[NODE local-light] NOT INSTALLED"));
#endif
  Serial.printf("[LED] Onboard status LED: DISABLED | GPIO: %u\n",
                kStatusLedPin);
  Serial.printf("[MEM] Flash: %lu MB | PSRAM: %s, %lu MB | Free heap: %lu "
                "bytes\n",
                static_cast<unsigned long>(
                    ESP.getFlashChipSize() / (1024U * 1024U)),
                hasPsram ? "READY" : "NOT FOUND",
                static_cast<unsigned long>(
                    hasPsram ? ESP.getPsramSize() / (1024U * 1024U) : 0),
                static_cast<unsigned long>(ESP.getFreeHeap()));
  Serial.println(F("------------------------------------------------"));
}

void printRuntimeStatus() {
  const uint32_t now = millis();
  if (now - lastSerialReport < kSerialReportIntervalMs) return;
  lastSerialReport = now;
  const sozo::AudioSnapshot &audio = audioAnalyzer.snapshot();
  const sozo::SpaceSceneSnapshot &scene = sceneCoordinator.snapshot();
  Serial.printf(
      "[LIVE] MIC=%s Frames=%lu | Mode=%s | Volume=%.1f | Beat=%.1f | "
      "RMS=%.1f | Brightness=%u | FreeHeap=%lu bytes\n",
      audio.frame.available ? "READY" : "ERROR",
      static_cast<unsigned long>(audio.frame.framesRead),
      modeName(scene.lighting.mode), audio.frame.volume,
      audio.frame.beatPulse, audio.selectedRawRms,
      scene.lighting.brightness,
      static_cast<unsigned long>(ESP.getFreeHeap()));
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);

  loadConfiguration();
  Serial.printf("[BOOT] Loaded saved mode: %s\n",
                modeName(sceneCoordinator.snapshot().lighting.mode));
  initStatusLed();
  Serial.println(F("[BOOT] Onboard GPIO48 WS2812 disabled."));
  initLocalLightNode();
#if SOZO_LOCAL_LIGHT_ENABLED
  Serial.printf("[BOOT] Local light node initialized: GPIO%u, %u pixels, "
                "brightness %u.\n",
                kLedPin,
                sceneCoordinator.localLightConfiguration().layout.activeCount,
                sceneCoordinator.snapshot().lighting.brightness);
#else
  Serial.println(F("[BOOT] Local light node not installed."));
#endif
  const bool micAvailable = audioAnalyzer.begin();
  Serial.printf("[BOOT] INMP441 I2S initialization: %s\n",
                micAvailable ? "SUCCESS" : "FAILED");
  const bool wifiAvailable = networkManager.begin();
  Serial.printf("[BOOT] Wi-Fi initialization: %s\n",
                wifiAvailable ? "SUCCESS" : "FAILED");
  initOta();
  const bool nodeBusAvailable = nodeCoordinator.begin();
  Serial.printf("[BOOT] SOZO Flux node bus: %s\n",
                nodeBusAvailable ? "SUCCESS" : "FAILED");
  webApi.begin();
  Serial.println(F("[BOOT] HTTP WebServer started on port 80."));
  printSystemInfo();
  printDeviceStatus();
  serialConsole.begin();
}

void loop() {
  const uint32_t now = millis();
  handleOta();
  serialConsole.tick();
  webApi.handleClient();
  saveConfigurationIfReady();
  audioAnalyzer.tick();
  const sozo::AudioFrame &audio = audioAnalyzer.snapshot().frame;
  const sozo::SpaceSceneSnapshot &scene = sceneCoordinator.snapshot();
#if SOZO_LOCAL_LIGHT_ENABLED
  if (scene.revision != lastLegacySceneRevision) {
    lightingScenes.applyDirect(sozo::kLocalLightingTargetId, scene.lighting);
    lightingSceneStore.markDirty(now);
    lastLegacySceneRevision = scene.revision;
  }
#endif
#if SOZO_LOCAL_LIGHT_ENABLED
  const sozo::LocalLightConfiguration &configuration =
      sceneCoordinator.localLightConfiguration();
  if (configuration.revision != lastLocalLightConfigRevision) {
    localLightNode.updateLocalConfiguration(
        configuration, sceneCoordinator.lightingState().audio);
    lastLocalLightConfigRevision = configuration.revision;
  }
  nodeCoordinator.tick(now, scene, audio);
  sceneDelivery.tick(now);
  localLightNode.applyAudioFrame(audio, audio.framesRead);
  localLightNode.tick(now);
#else
  nodeCoordinator.tick(now, scene, audio);
  sceneDelivery.tick(now);
#endif
  networkManager.tick();
  printRuntimeStatus();
  delay(1);
}
