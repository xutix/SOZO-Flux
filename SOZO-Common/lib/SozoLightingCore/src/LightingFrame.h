#pragma once

#include <stdint.h>

#include <SozoDomain.h>

namespace sozo {

struct LightingFrame {
  Rgb pixels[spatial_light::kMaxLedCount]{};
  uint16_t count{0};
};

}  // namespace sozo
