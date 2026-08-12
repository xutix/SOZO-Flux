#pragma once

#include <BleCentralAdapter.h>
#include <NimBLEDevice.h>
#include <NodeFleetTransport.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace sozo {

class BleFleetAdapter final : public NodeFleetTransport,
                              private NimBLEScanCallbacks {
 public:
  static constexpr size_t kLinkCapacity = 4U;

  bool begin() override;
  void tick(uint32_t nowMs) override;
  size_t capacity() const override;
  NodeTransport *linkAt(size_t index) override;
  const NodeTransport *linkAt(size_t index) const override;
  bool releaseLink(size_t index) override;

  bool openPairingWindow(uint32_t nowMs, uint32_t durationMs) override;
  bool pairingWindowOpen(uint32_t nowMs) const override;
  uint32_t pairingRemainingMs(uint32_t nowMs) const override;
  bool scanning() const override;

 private:
  struct Candidate {
    uint64_t address{0};
    uint8_t addressType{0};
  };

  void onResult(const NimBLEAdvertisedDevice *device) override;
  void onScanEnd(const NimBLEScanResults &results, int reason) override;

  bool hasFreeLink() const;
  size_t assignedCount() const;
  bool addressAssigned(uint64_t address) const;
  bool assignCandidate(const Candidate &candidate, uint32_t nowMs);
  bool discoveryWindowOpen(uint32_t nowMs);
  bool hasUnassignedBond() const;
  void updateScan(uint32_t nowMs);

  static constexpr UBaseType_t kCandidateQueueCapacity = 8U;
  static constexpr uint32_t kStartupDiscoveryMs = 10000U;
  static constexpr uint32_t kBackgroundIntervalMs = 30000U;
  static constexpr uint32_t kBackgroundDiscoveryMs = 3000U;

  BleCentralAdapter links_[kLinkCapacity]{};
  NimBLEScan *scan_{nullptr};
  QueueHandle_t candidateQueue_{nullptr};
  uint32_t startupDiscoveryUntilMs_{0};
  uint32_t pairingUntilMs_{0};
  uint32_t backgroundDiscoveryUntilMs_{0};
  uint32_t nextBackgroundDiscoveryMs_{0};
  bool initialized_{false};
};

}  // namespace sozo
