#pragma once

#include <Arduino.h>
#include <WebServer.h>

#include <AudioAnalyzer.h>
#include <LightingControlApplication.h>
#include <NetworkManager.h>
#include <NodeNameStore.h>
#include <NodeFleetCoordinator.h>
#include <NodeFirmwareWebController.h>

namespace sozo {

class WebApi {
 public:
  using RestartCallback = void (*)();
  using PageBuilder = String (*)();

  WebApi(LightingControlApplication &lighting, NetworkManager &network,
         AudioAnalyzer &audio, NodeFleetCoordinator &nodes,
         NodeNameStore &nodeNames,
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
  void handleSetNodeLighting();
  void handleSetNodeLedCount();
  void handleSetNodeLayout();
  void handleSetNodeName();
  void handleGetScenes();
  void handleSaveScene();
  void handleSetSceneAssignment();
  void handleActivateScene();
  void handleDeleteScene();
  void handleSetTargetLighting();
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
  WebServer server_;
  LightingControlApplication &lighting_;
  NetworkManager &network_;
  AudioAnalyzer &audio_;
  NodeFleetCoordinator &nodes_;
  NodeNameStore &nodeNames_;
  PageBuilder spatialPage_;
  RestartCallback restart_;
  NodeFirmwareWebController nodeFirmware_;
};

}  // namespace sozo
