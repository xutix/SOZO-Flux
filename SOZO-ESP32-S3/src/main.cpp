#include <Arduino.h>
#include <ArduinoOTA.h>
#include <Adafruit_NeoPixel.h>
#include <SpatialLightCore.h>

#include <AudioAnalyzer.h>
#include <BleCentralAdapter.h>
#include <CommandRouter.h>
#include <LightingController.h>
#include <NetworkManager.h>
#include <NodeCoordinator.h>
#include <SerialConsole.h>
#include <SettingsStore.h>
#include <S3LightingOutput.h>
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
constexpr char kOtaPassword[] = SOZO_OTA_PASSWORD;
constexpr uint8_t kLedPin = 18;
constexpr uint8_t kStatusLedPin = 48;
constexpr uint8_t kStatusLedCount = 1;
constexpr uint8_t kStatusLedBrightness = 60;
constexpr uint32_t kSerialReportIntervalMs = 500;

sozo::SettingsStore settingsStore;
sozo::NetworkManager networkManager(settingsStore);
sozo::s3::S3LightingOutput lightingOutput(spatial_light::kMaxLedCount,
                                          kLedPin);
sozo::LightingController lightingController(lightingOutput);
sozo::AudioAnalyzer audioAnalyzer;
sozo::CommandRouter commandRouter(lightingController, settingsStore);
sozo::BleCentralAdapter bleNodeTransport;
sozo::NodeCoordinator nodeCoordinator(bleNodeTransport);
sozo::SerialConsole serialConsole(Serial, commandRouter);
Adafruit_NeoPixel statusLed(kStatusLedCount, kStatusLedPin,
                            NEO_GRB + NEO_KHZ800);

void restartDevice();
sozo::WebApi webApi(commandRouter, networkManager, audioAnalyzer,
                    nodeCoordinator,
                    buildSpatialLightPage, restartDevice);

uint32_t lastSerialReport = 0;
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

void initLedStrip() { lightingController.begin(lightingController.state()); }

void loadConfiguration() {
  const sozo::PersistedLightingState state = settingsStore.loadLightingState();
  lightingController.setState(state);
  audioAnalyzer.setTuning(state.audio);
}

void saveConfigurationIfReady() { commandRouter.tick(millis()); }

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
  Serial.printf("WS2812 strip: GPIO%u, active=%u, maximum=%u pixels\n", kLedPin,
                lightingController.state().layout.activeCount,
                spatial_light::kMaxLedCount);
  Serial.printf("Onboard status WS2812: GPIO%u (disabled)\n", kStatusLedPin);
  Serial.printf("SOZO Flux node bus: %s\n",
                nodeCoordinator.nodeReady() ? "NODE READY" : "SCANNING");
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
  Serial.printf("[LED] Strip: READY | GPIO: %u | Active: %u/%u | Mode: %s | "
                "Brightness: %u\n",
                kLedPin, lightingController.state().layout.activeCount,
                spatial_light::kMaxLedCount,
                modeName(lightingController.state().mode),
                lightingController.state().brightness);
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
  Serial.printf(
      "[LIVE] MIC=%s Frames=%lu | Mode=%s | Volume=%.1f | Beat=%.1f | "
      "RMS=%.1f | Brightness=%u | FreeHeap=%lu bytes\n",
      audio.frame.available ? "READY" : "ERROR",
      static_cast<unsigned long>(audio.frame.framesRead),
      modeName(lightingController.state().mode), audio.frame.volume,
      audio.frame.beatPulse, audio.selectedRawRms,
      lightingController.state().brightness,
      static_cast<unsigned long>(ESP.getFreeHeap()));
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);

  loadConfiguration();
  Serial.printf("[BOOT] Loaded saved mode: %s\n",
                modeName(lightingController.state().mode));
  initStatusLed();
  Serial.println(F("[BOOT] Onboard GPIO48 WS2812 disabled."));
  initLedStrip();
  Serial.printf("[BOOT] WS2812 strip initialized: GPIO%u, %u pixels, "
                "brightness %u.\n",
                kLedPin, lightingController.state().layout.activeCount,
                lightingController.state().brightness);
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
  handleOta();
  serialConsole.tick();
  webApi.handleClient();
  saveConfigurationIfReady();
  audioAnalyzer.tick();
  lightingController.tick(millis(), audioAnalyzer.snapshot().frame);
  nodeCoordinator.tick(millis(), lightingController.state(),
                       lightingController.snapshot(),
                       audioAnalyzer.snapshot().frame);
  networkManager.tick();
  printRuntimeStatus();
  delay(1);
}
