#include "NodeFirmwareWebController.h"

#include <esp_heap_caps.h>
#include <mbedtls/sha256.h>

namespace sozo {
namespace {

constexpr size_t kMaxC3FirmwareBytes = 0x140000U;
constexpr uint16_t kEsp32C3ImageChipId = 5U;

bool parseNodeId(const String &value, node::NodeId &nodeId) {
  if (value.length() == 0U) return false;
  char *end = nullptr;
  const unsigned long parsed = strtoul(value.c_str(), &end, 16);
  if (end == nullptr || *end != '\0' || parsed == 0UL ||
      parsed == node::kBroadcastNodeId || parsed == node::kCoordinatorNodeId) {
    return false;
  }
  nodeId = static_cast<node::NodeId>(parsed);
  return true;
}

bool validImage(const uint8_t *image, const size_t imageSize) {
  if (image == nullptr || imageSize < 24U ||
      imageSize > kMaxC3FirmwareBytes || image[0] != 0xE9U) {
    return false;
  }
  const uint16_t chipId = static_cast<uint16_t>(image[12]) |
                          (static_cast<uint16_t>(image[13]) << 8U);
  return chipId == kEsp32C3ImageChipId;
}

const char *stateName(const NodeFirmwareTransferState state) {
  switch (state) {
    case NodeFirmwareTransferState::Starting: return "starting";
    case NodeFirmwareTransferState::Sending: return "sending";
    case NodeFirmwareTransferState::Finalizing: return "verifying";
    case NodeFirmwareTransferState::Succeeded: return "restarting";
    case NodeFirmwareTransferState::Failed: return "failed";
    case NodeFirmwareTransferState::Idle:
    default: return "idle";
  }
}

const char *errorName(const node::FirmwareUpdateError error) {
  switch (error) {
    case node::FirmwareUpdateError::InvalidImage: return "invalid_image";
    case node::FirmwareUpdateError::ImageTooLarge: return "image_too_large";
    case node::FirmwareUpdateError::WriteFailed: return "write_failed";
    case node::FirmwareUpdateError::HashMismatch: return "hash_mismatch";
    case node::FirmwareUpdateError::UnexpectedOffset: return "unexpected_offset";
    case node::FirmwareUpdateError::Timeout: return "timeout";
    case node::FirmwareUpdateError::LinkLost: return "link_lost";
    case node::FirmwareUpdateError::Unsupported: return "unsupported";
    case node::FirmwareUpdateError::Busy: return "busy";
    case node::FirmwareUpdateError::Unauthorized: return "unauthorized";
    case node::FirmwareUpdateError::NotStarted: return "not_started";
    case node::FirmwareUpdateError::None:
    default: return "none";
  }
}

}  // namespace

NodeFirmwareWebController::NodeFirmwareWebController(
    WebServer &server, NodeFleetCoordinator &nodes)
    : server_(server), nodes_(nodes) {}

void NodeFirmwareWebController::registerRoutes() {
  server_.on("/api/node/firmware", HTTP_GET, [this]() { handleStatus(); });
  server_.on("/api/node/firmware", HTTP_POST,
             [this]() { handleUploadComplete(); },
             [this]() { handleUploadData(); });
}

void NodeFirmwareWebController::tick() {
  if (image_ != nullptr && uploadComplete_ &&
      nodes_.firmwareUpdateStatus().state == NodeFirmwareTransferState::Idle) {
    heap_caps_free(image_);
    image_ = nullptr;
    imageSize_ = received_ = 0U;
    target_ = 0U;
    uploadComplete_ = false;
  }
}

void NodeFirmwareWebController::handleUploadData() {
  HTTPUpload &upload = server_.upload();
  if (upload.status == UPLOAD_FILE_START) {
    uploadError_ = "";
    uploadComplete_ = false;
    received_ = imageSize_ = 0U;
    target_ = 0U;
    const NodeFirmwareTransferState state = nodes_.firmwareUpdateStatus().state;
    if (state == NodeFirmwareTransferState::Starting ||
        state == NodeFirmwareTransferState::Sending ||
        state == NodeFirmwareTransferState::Finalizing ||
        state == NodeFirmwareTransferState::Succeeded) {
      uploadError_ = "another C3 firmware update is active";
      return;
    }
    if (image_ != nullptr) heap_caps_free(image_);
    image_ = nullptr;
    if (!server_.hasArg("id") || !parseNodeId(server_.arg("id"), target_)) {
      uploadError_ = "valid target node id is required";
      return;
    }
    const NodeRecord *record = nodes_.registry().find(target_);
    if (record == nullptr ||
        record->connectionState != NodeConnectionState::Ready ||
        !record->capabilities.bound ||
        (record->capabilities.capabilityBits &
         node::capabilityMask(node::Capability::FirmwareUpdate)) == 0U) {
      uploadError_ = "target C3 must be online, bound, and OTA-capable";
      return;
    }
    if (!psramFound()) {
      uploadError_ = "S3 PSRAM is unavailable";
      return;
    }
    image_ = static_cast<uint8_t *>(heap_caps_malloc(
        kMaxC3FirmwareBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (image_ == nullptr) {
      uploadError_ = "not enough S3 PSRAM for firmware image";
    }
    return;
  }
  if (upload.status == UPLOAD_FILE_WRITE) {
    if (!uploadError_.isEmpty()) return;
    if (image_ == nullptr || upload.currentSize == 0U ||
        received_ + upload.currentSize > kMaxC3FirmwareBytes) {
      uploadError_ = "firmware image is too large";
      return;
    }
    memcpy(image_ + received_, upload.buf, upload.currentSize);
    received_ += upload.currentSize;
    return;
  }
  if (upload.status == UPLOAD_FILE_END) {
    if (!uploadError_.isEmpty()) return;
    imageSize_ = received_;
    if (!validImage(image_, imageSize_)) {
      uploadError_ = "file is not a valid ESP32-C3 application image";
      return;
    }
    uint8_t sha256[32]{};
    if (mbedtls_sha256_ret(image_, imageSize_, sha256, 0) != 0) {
      uploadError_ = "failed to hash firmware image";
      return;
    }
    if (!nodes_.requestNodeFirmwareUpdate(target_, image_, imageSize_, sha256,
                                          millis())) {
      uploadError_ = "target C3 is unavailable or busy";
      return;
    }
    uploadComplete_ = true;
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    uploadError_ = "firmware upload was aborted";
  }
}

void NodeFirmwareWebController::handleUploadComplete() {
  if (!uploadError_.isEmpty() || !uploadComplete_) {
    const String message = uploadError_.isEmpty()
                               ? String("firmware upload did not complete")
                               : uploadError_;
    if (image_ != nullptr && !uploadComplete_) {
      heap_caps_free(image_);
      image_ = nullptr;
    }
    sendResult(false, message.c_str());
    return;
  }
  char nodeId[9]{};
  snprintf(nodeId, sizeof(nodeId), "%08lx",
           static_cast<unsigned long>(target_));
  String json = F("{\"ok\":true,\"pending\":true,\"nodeId\":\"");
  json += nodeId;
  json += F("\",\"imageSize\":");
  json += imageSize_;
  json += '}';
  server_.send(202, "application/json; charset=utf-8", json);
}

void NodeFirmwareWebController::handleStatus() {
  const NodeFirmwareTransferStatus status = nodes_.firmwareUpdateStatus();
  char nodeId[9]{};
  snprintf(nodeId, sizeof(nodeId), "%08lx",
           static_cast<unsigned long>(status.nodeId));
  const uint32_t progress = status.imageSize == 0U ? 0U :
      static_cast<uint32_t>((static_cast<uint64_t>(status.confirmedBytes) *
                             100U) / status.imageSize);
  String json = F("{\"ok\":true,\"state\":\"");
  json += stateName(status.state);
  json += F("\",\"nodeId\":\"");
  json += nodeId;
  json += F("\",\"imageSize\":");
  json += status.imageSize;
  json += F(",\"confirmedBytes\":");
  json += status.confirmedBytes;
  json += F(",\"progress\":");
  json += progress;
  json += F(",\"error\":\"");
  json += errorName(status.error);
  json += F("\"}");
  server_.send(200, "application/json; charset=utf-8", json);
}

void NodeFirmwareWebController::sendResult(const bool ok,
                                           const char *message) {
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

}  // namespace sozo
