#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include <AudioAnalyzer.h>
#include <CommandRouter.h>
#include <NetworkManager.h>
#include <NodeFleetCoordinator.h>

namespace sozo {

class WebApi {
 public:
  using RestartCallback = void (*)();
  using PageBuilder = String (*)();

  WebApi(CommandRouter &commands, NetworkManager &network,
         AudioAnalyzer &audio, NodeFleetCoordinator &nodes,
         PageBuilder spatialPage,
         RestartCallback restart);

  void begin();
  void handleClient();

 private:
  void registerRoutes();
  void sendApiResult(bool ok, const char *message = "");
  void handleRoot();
  void handleApiStatus();
  void handleGetNodes();
  void handleOpenNodePairing();
  void handleWiFiScan();
  void handleWiFiSave();
  void handleWiFiReset();
  void handleGetLayout();
  void handleSetLayout();
  void handleSetLighting();
  void handleSetNodeMode();
  void handleSetNodeLighting();
  void handleSetNodeLedCount();
  void handleSetMode();
  void handleSetColor();
  void handleSetBrightness();
  void handleSetEffects();
  void handleSetStartup();
  void handleSetAudio();
  void handleRestart();

  bool dispatchWebControl(ControlCommandType type,
                          LightingParameter parameter =
                              LightingParameter::None,
                          int32_t value = 0, Rgb color = {0, 0, 0});
  bool dispatchWebLayout(const spatial_light::SpatialLayout &layout);
  String buildLayoutJson() const;
  String buildLightingSettingsJson() const;
  bool parseLightingRequest(PersistedLightingState &next,
                            EffectMode &requestedMode, bool allowNodeId,
                            String &error);

  WebServer server_;
  CommandRouter &commands_;
  NetworkManager &network_;
  AudioAnalyzer &audio_;
  NodeFleetCoordinator &nodes_;
  PageBuilder spatialPage_;
  RestartCallback restart_;
};

}  // namespace sozo
