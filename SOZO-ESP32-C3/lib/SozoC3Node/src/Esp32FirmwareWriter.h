#pragma once

#include <NodeFirmwareReceiver.h>

#include <mbedtls/sha256.h>

namespace sozo::c3 {

class Esp32FirmwareWriter final : public FirmwareWriterPort {
 public:
  Esp32FirmwareWriter();
  ~Esp32FirmwareWriter() override;

  bool begin(uint32_t imageSize) override;
  bool write(const uint8_t *data, size_t dataLength) override;
  FirmwareCommitResult finish(const uint8_t expectedSha256[32]) override;
  void abort() override;

 private:
  mbedtls_sha256_context sha256_{};
  bool active_{false};
};

}  // namespace sozo::c3
