#include <BleFleetAdapter.h>

#include <Arduino.h>
#include <SozoBleContract.h>

namespace sozo {
namespace {

bool deadlinePending(const uint32_t nowMs, const uint32_t deadlineMs) {
  return deadlineMs != 0U && static_cast<int32_t>(deadlineMs - nowMs) > 0;
}

}  // namespace

bool BleFleetAdapter::begin() {
  if (initialized_) return true;

  candidateQueue_ = xQueueCreate(kCandidateQueueCapacity, sizeof(Candidate));
  if (candidateQueue_ == nullptr) return false;

  if (!NimBLEDevice::init("SOZO-Flux-Coordinator")) return false;
  NimBLEDevice::setMTU(ble::kPreferredMtu);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  NimBLEDevice::setSecurityAuth(true, false, true);

  scan_ = NimBLEDevice::getScan();
  if (scan_ == nullptr) return false;
  scan_->setScanCallbacks(this, false);
  scan_->setInterval(160);
  scan_->setWindow(40);
  scan_->setActiveScan(true);
  scan_->setMaxResults(0);

  const uint32_t nowMs = millis();
  startupDiscoveryUntilMs_ = nowMs + kStartupDiscoveryMs;
  nextBackgroundDiscoveryMs_ = nowMs + kBackgroundIntervalMs;
  initialized_ = true;
  updateScan(nowMs);
  return true;
}

void BleFleetAdapter::tick(const uint32_t nowMs) {
  if (!initialized_) return;

  Candidate candidate{};
  while (xQueueReceive(candidateQueue_, &candidate, 0) == pdTRUE) {
    assignCandidate(candidate, nowMs);
  }
  updateScan(nowMs);
}

size_t BleFleetAdapter::capacity() const { return kLinkCapacity; }

NodeTransport *BleFleetAdapter::linkAt(const size_t index) {
  return index < kLinkCapacity ? &links_[index] : nullptr;
}

const NodeTransport *BleFleetAdapter::linkAt(const size_t index) const {
  return index < kLinkCapacity ? &links_[index] : nullptr;
}

bool BleFleetAdapter::releaseLink(const size_t index) {
  return index < kLinkCapacity && links_[index].release();
}

bool BleFleetAdapter::openPairingWindow(const uint32_t nowMs,
                                        const uint32_t durationMs) {
  if (!initialized_ || durationMs == 0U || !hasFreeLink()) return false;
  pairingUntilMs_ = nowMs + durationMs;
  updateScan(nowMs);
  return true;
}

bool BleFleetAdapter::pairingWindowOpen(const uint32_t nowMs) const {
  return deadlinePending(nowMs, pairingUntilMs_);
}

uint32_t BleFleetAdapter::pairingRemainingMs(const uint32_t nowMs) const {
  return pairingWindowOpen(nowMs) ? pairingUntilMs_ - nowMs : 0U;
}

bool BleFleetAdapter::scanning() const {
  return scan_ != nullptr && scan_->isScanning();
}

void BleFleetAdapter::onResult(const NimBLEAdvertisedDevice *device) {
  if (device == nullptr || candidateQueue_ == nullptr ||
      !device->isAdvertisingService(NimBLEUUID(ble::kServiceUuid))) {
    return;
  }
  const NimBLEAddress deviceAddress = device->getAddress();
  if (!pairingWindowOpen(millis()) &&
      !NimBLEDevice::isBonded(deviceAddress)) {
    return;
  }
  Candidate candidate{};
  candidate.address = static_cast<uint64_t>(deviceAddress);
  candidate.addressType = device->getAddressType();
  if (candidate.address == 0U || addressAssigned(candidate.address)) return;
  xQueueSend(candidateQueue_, &candidate, 0);
}

void BleFleetAdapter::onScanEnd(const NimBLEScanResults &, int) {}

bool BleFleetAdapter::hasFreeLink() const {
  for (const BleCentralAdapter &link : links_) {
    if (!link.assigned()) return true;
  }
  return false;
}

size_t BleFleetAdapter::assignedCount() const {
  size_t count = 0U;
  for (const BleCentralAdapter &link : links_) {
    if (link.assigned()) ++count;
  }
  return count;
}

bool BleFleetAdapter::addressAssigned(const uint64_t address) const {
  for (const BleCentralAdapter &link : links_) {
    if (link.assigned() && link.peerAddress() == address) return true;
  }
  return false;
}

bool BleFleetAdapter::assignCandidate(const Candidate &candidate,
                                      const uint32_t nowMs) {
  if (candidate.address == 0U || addressAssigned(candidate.address)) {
    return false;
  }
  for (BleCentralAdapter &link : links_) {
    if (!link.assigned()) {
      return link.connect(candidate.address, candidate.addressType, nowMs);
    }
  }
  return false;
}

bool BleFleetAdapter::discoveryWindowOpen(const uint32_t nowMs) {
  if (pairingWindowOpen(nowMs)) return true;
  if (!hasUnassignedBond()) return false;
  if (deadlinePending(nowMs, startupDiscoveryUntilMs_) ||
      deadlinePending(nowMs, backgroundDiscoveryUntilMs_)) {
    return true;
  }
  if (static_cast<int32_t>(nowMs - nextBackgroundDiscoveryMs_) < 0) {
    return false;
  }
  backgroundDiscoveryUntilMs_ = nowMs + kBackgroundDiscoveryMs;
  nextBackgroundDiscoveryMs_ = nowMs + kBackgroundIntervalMs;
  return true;
}

bool BleFleetAdapter::hasUnassignedBond() const {
  const int bondedCount = NimBLEDevice::getNumBonds();
  return bondedCount > 0 &&
         static_cast<size_t>(bondedCount) > assignedCount();
}

void BleFleetAdapter::updateScan(const uint32_t nowMs) {
  if (scan_ == nullptr) return;
  const bool shouldScan = hasFreeLink() && discoveryWindowOpen(nowMs);
  if (shouldScan && !scan_->isScanning()) {
    scan_->start(0, false, true);
  } else if (!shouldScan && scan_->isScanning()) {
    scan_->stop();
  }
}

}  // namespace sozo
