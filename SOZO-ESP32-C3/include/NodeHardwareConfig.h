#pragma once

#include <stdint.h>

#ifndef SOZO_NODE_LED_PIN
#define SOZO_NODE_LED_PIN 4
#endif

#ifndef SOZO_NODE_MAX_LED_COUNT
#define SOZO_NODE_MAX_LED_COUNT 512
#endif

#ifndef SOZO_NODE_DEFAULT_LED_COUNT
#define SOZO_NODE_DEFAULT_LED_COUNT 60
#endif

#ifndef SOZO_NODE_BOOT_PIN
#define SOZO_NODE_BOOT_PIN 9
#endif

namespace sozo::c3 {

constexpr uint8_t kLedPin = SOZO_NODE_LED_PIN;
constexpr uint16_t kMaxLedCount = SOZO_NODE_MAX_LED_COUNT;
constexpr uint16_t kDefaultLedCount = SOZO_NODE_DEFAULT_LED_COUNT;
constexpr uint8_t kBootPin = SOZO_NODE_BOOT_PIN;

static_assert(kMaxLedCount > 0,
              "SOZO_NODE_MAX_LED_COUNT must be greater than zero");
static_assert(kDefaultLedCount > 0,
              "SOZO_NODE_DEFAULT_LED_COUNT must be greater than zero");
static_assert(kDefaultLedCount <= kMaxLedCount,
              "SOZO_NODE_DEFAULT_LED_COUNT cannot exceed SOZO_NODE_MAX_LED_COUNT");

}  // namespace sozo::c3
