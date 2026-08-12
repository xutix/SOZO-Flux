#include <Esp32FirmwareWriter.h>

#include <Update.h>

namespace sozo::c3 {

Esp32FirmwareWriter::Esp32FirmwareWriter() { mbedtls_sha256_init(&sha256_); }

Esp32FirmwareWriter::~Esp32FirmwareWriter() {
  if (active_) Update.abort();
  mbedtls_sha256_free(&sha256_);
}

bool Esp32FirmwareWriter::begin(const uint32_t imageSize) {
  if (active_) return false;
  mbedtls_sha256_free(&sha256_);
  mbedtls_sha256_init(&sha256_);
  if (mbedtls_sha256_starts_ret(&sha256_, 0) != 0) return false;
  if (!Update.begin(imageSize, U_FLASH)) {
    mbedtls_sha256_free(&sha256_);
    mbedtls_sha256_init(&sha256_);
    return false;
  }
  active_ = true;
  return true;
}

bool Esp32FirmwareWriter::write(const uint8_t *data,
                                const size_t dataLength) {
  if (!active_ || data == nullptr || dataLength == 0U) return false;
  // Arduino-ESP32 2.x exposes a mutable pointer even though it does not
  // modify the supplied buffer.
  if (Update.write(const_cast<uint8_t *>(data), dataLength) != dataLength) {
    return false;
  }
  return mbedtls_sha256_update_ret(&sha256_, data, dataLength) == 0;
}

FirmwareCommitResult Esp32FirmwareWriter::finish(
    const uint8_t expectedSha256[32]) {
  if (!active_) return FirmwareCommitResult::WriteFailed;

  uint8_t actualSha256[32]{};
  if (mbedtls_sha256_finish_ret(&sha256_, actualSha256) != 0) {
    abort();
    return FirmwareCommitResult::WriteFailed;
  }
  for (size_t i = 0U; i < sizeof(actualSha256); ++i) {
    if (actualSha256[i] != expectedSha256[i]) {
      abort();
      return FirmwareCommitResult::HashMismatch;
    }
  }
  if (!Update.end(false)) {
    abort();
    return FirmwareCommitResult::WriteFailed;
  }
  active_ = false;
  return FirmwareCommitResult::Ok;
}

void Esp32FirmwareWriter::abort() {
  if (active_) Update.abort();
  active_ = false;
  mbedtls_sha256_free(&sha256_);
  mbedtls_sha256_init(&sha256_);
}

}  // namespace sozo::c3
