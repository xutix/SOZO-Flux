#pragma once

#include <LedGeometry.h>
#include <LightingFrame.h>

namespace sozo::lighting {

// Hardware boundary for the shared lighting core. The frame is always in
// logical effect order; the platform adapter owns pin, color order and the
// logical-to-physical mapping described by geometry.
class LedOutput {
 public:
  virtual ~LedOutput() = default;
  virtual void begin(const LedGeometry &geometry) = 0;
  virtual void present(const LedGeometry &geometry,
                       const LightingFrame &frame) = 0;
};

}  // namespace sozo::lighting
