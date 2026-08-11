#pragma once

#include <NodeBindingPort.h>
#include <NodeControlPort.h>
#include <NodeLedCountPort.h>
#include <NodeFirmwareReceiver.h>
#include <NodePlatformPorts.h>
#include <NodeSceneRuntime.h>
#include <NodeTransportPort.h>
#include <PairingWindow.h>

namespace sozo::c3 {

struct C3NodeProfile {
  uint16_t defaultLedCount{0};
  uint16_t maxLedCount{0};
  uint16_t hardwareProfile{0};
};

enum class C3NodeEvent : uint8_t {
  None = 0,
  PairingOpened,
  PairingClosed,
  RestartRequested,
  FirmwareRestartRequested,
};

class C3NodeApplication {
 public:
  C3NodeApplication(NodeLightingSink &lighting, NodeButtonInput &button,
                    NodeDiagnosticsPort &diagnostics,
                    PairingWindow &pairingWindow, NodeTransportPort &transport,
                    NodeBindingRepository &bindings,
                    NodeControlRepository &controls,
                    NodeLedCountRepository &ledCounts, C3NodeProfile profile,
                    NodeFirmwareReceiver *firmwareReceiver = nullptr);

  bool begin(node::NodeId nodeId, uint32_t nowMs);
  C3NodeEvent tick(uint32_t nowMs);

  node::NodeId nodeId() const;
  const NodeBinding &binding() const;
  uint32_t lastAppliedSceneRevision() const;

 private:
  PersistedLightingState makeLocalState(const NodeLedCountState &layout) const;
  spatial_light::SpatialLayout makeSpatialLayout(
      const NodeLedCountState &layout) const;
  node::CapabilitiesPayload makeCapabilities() const;
  void handleInbound(const node::Envelope &envelope, uint32_t nowMs);
  void handleBindRequest(const node::Envelope &envelope, uint32_t nowMs);
  void sendBindResult(const node::Envelope &request, bool accepted,
                      uint16_t errorCode, uint32_t nowMs);
  void sendSceneReceipt(const node::Envelope &request,
                        SceneApplyResult result, uint32_t nowMs);
  void sendCommandReceipt(const node::Envelope &request, bool accepted,
                          uint16_t errorCode, uint32_t nowMs);
  void sendStatusSnapshot(const node::Envelope &request, uint32_t nowMs);
  void sendFirmwareStatus(const node::Envelope &request, uint32_t nowMs);

  NodeLightingSink &lighting_;
  NodeButtonInput &button_;
  NodeDiagnosticsPort &diagnostics_;
  PairingWindow &pairingWindow_;
  NodeTransportPort &transport_;
  NodeBindingRepository &bindings_;
  NodeControlRepository &controls_;
  NodeLedCountRepository &ledCounts_;
  NodeFirmwareReceiver *firmwareReceiver_{nullptr};
  C3NodeProfile profile_{};
  NodeSceneRuntime sceneRuntime_;
  NodeBinding binding_{};
  NodeLedCountState ledCountState_{};
  node::NodeId nodeId_{0};
  uint32_t outboundSequence_{1};
  bool transportConnected_{false};
  bool initialized_{false};
};

}  // namespace sozo::c3
