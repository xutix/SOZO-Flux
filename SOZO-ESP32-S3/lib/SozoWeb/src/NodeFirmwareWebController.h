#pragma once

#include <WebServer.h>

#include <NodeFleetCoordinator.h>

namespace sozo {

class NodeFirmwareWebController {
 public:
  NodeFirmwareWebController(WebServer &server, NodeFleetCoordinator &nodes);
  void registerRoutes();
  void tick();

 private:
  void handleUploadData();
  void handleUploadComplete();
  void handleStatus();
  void sendResult(bool ok, const char *message = "");

  WebServer &server_;
  NodeFleetCoordinator &nodes_;
  uint8_t *image_{nullptr};
  size_t imageSize_{0U};
  size_t received_{0U};
  node::NodeId target_{0U};
  String uploadError_{};
  bool uploadComplete_{false};
};

}  // namespace sozo
