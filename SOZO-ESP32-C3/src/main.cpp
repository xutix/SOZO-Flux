#include <Arduino.h>

#include <BlePeripheralAdapter.h>
#include <C3LightingHardware.h>
#include <C3NodeApplication.h>
#include <C3PlatformHardware.h>
#include <NodeBindingStore.h>
#include <NodeControlStore.h>
#include <NodeHardwareConfig.h>
#include <NodeLedCountStore.h>

namespace {

sozo::c3::C3LightingOutput lightingOutput(sozo::c3::kMaxLedCount,
                                          sozo::c3::kLedPin);
sozo::LightingController lighting(lightingOutput);
sozo::c3::LightingControllerSink lightingSink(lighting);
sozo::c3::C3BootButton bootButton(sozo::c3::kBootPin);
sozo::c3::C3RuntimeDiagnostics diagnostics;
sozo::c3::PairingWindow pairingWindow(1500U, 60000U);
sozo::c3::BlePeripheralAdapter bleAdapter(pairingWindow);
sozo::c3::NodeBindingStore bindingStore;
sozo::c3::NodeControlStore controlStore;
sozo::c3::NodeLedCountStore ledCountStore;
const sozo::c3::C3NodeProfile nodeProfile{sozo::c3::kDefaultLedCount,
                                           sozo::c3::kMaxLedCount, 1U};
sozo::c3::C3NodeApplication nodeApp(lightingSink, bootButton, diagnostics,
                                     pairingWindow, bleAdapter, bindingStore,
                                     controlStore, ledCountStore, nodeProfile);

sozo::node::NodeId makeNodeId() {
  const uint64_t chipId = ESP.getEfuseMac();
  sozo::node::NodeId value = static_cast<uint32_t>(chipId) ^
                             static_cast<uint32_t>(chipId >> 32U) ^
                             0xC3000000U;
  if (value == 0 || value == sozo::node::kCoordinatorNodeId ||
      value == sozo::node::kBroadcastNodeId) {
    value ^= 0x13579BDFU;
  }
  return value;
}

void reportNodeEvent(const sozo::c3::C3NodeEvent event) {
  switch (event) {
    case sozo::c3::C3NodeEvent::PairingOpened:
      Serial.println("[SOZO-C3] Pairing window open for 60 seconds.");
      break;
    case sozo::c3::C3NodeEvent::PairingClosed:
      Serial.println("[SOZO-C3] Pairing window closed.");
      break;
    case sozo::c3::C3NodeEvent::RestartRequested:
      Serial.println("[SOZO-C3] Pairing reset complete; restarting.");
      delay(200);
      ESP.restart();
      break;
    case sozo::c3::C3NodeEvent::None:
    default:
      break;
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(300);
  bootButton.begin();
  const bool ready = nodeApp.begin(makeNodeId(), millis());
  const sozo::c3::NodeBinding &binding = nodeApp.binding();

  Serial.printf("[SOZO-C3] Local light ready: GPIO%u, default=%u, max=%u pixels\n",
                sozo::c3::kLedPin, sozo::c3::kDefaultLedCount,
                sozo::c3::kMaxLedCount);
  Serial.printf("[SOZO-C3] Node=%08lX BLE=%s binding=%s\n",
                static_cast<unsigned long>(nodeApp.nodeId()),
                ready ? "READY" : "FAILED", binding.bound ? "BOUND" : "UNBOUND");
  if (!binding.bound) {
    Serial.println("[SOZO-C3] Hold BOOT for 1.5 seconds to pair.");
  } else {
    Serial.println("[SOZO-C3] Hold BOOT for 8 seconds to reset pairing.");
  }
}

void loop() {
  reportNodeEvent(nodeApp.tick(millis()));
  delay(1);
}
