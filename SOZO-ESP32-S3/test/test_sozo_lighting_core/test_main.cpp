#include <Arduino.h>
#include <AudioAnalyzer.h>
#include <LightingController.h>

static_assert(sozo::scaleRgb({200, 100, 50}, 0).red == 0,
              "zero scaling must turn the red channel off");
static_assert(sozo::scaleRgb({200, 100, 50}, 255).green == 100,
              "full scaling must retain each channel");
static_assert(sozo::resolvePhysicalIndex(0, 144, false) == 0,
              "a normal layout must start at the first physical pixel");
static_assert(sozo::resolvePhysicalIndex(0, 144, true) == 143,
              "a reversed layout must start at the last physical pixel");

static_assert(sozo::rainbowCycleCount(0) == 1,
              "soft rainbow must span one complete color cycle");
static_assert(sozo::rainbowCycleCount(1) == 2,
              "vivid rainbow must span two complete color cycles");
static_assert(sozo::rainbowCycleCount(2) == 4,
              "neon rainbow must span four complete color cycles");
static_assert(sozo::rainbowHueForPixel(0, 30, 60, 1) == 32768,
              "a 60-pixel strip midpoint must be halfway around the wheel");
static_assert(sozo::rainbowHueForPixel(0, 72, 144, 1) == 32768,
              "a 144-pixel strip midpoint must be halfway around the wheel");
static_assert(sozo::rainbowHueForPixel(0, 150, 300, 1) == 32768,
              "a 300-pixel strip midpoint must be halfway around the wheel");
static_assert(sozo::rainbowHueForPixel(0, 250, 500, 1) == 32768,
              "a 500-pixel strip midpoint must be halfway around the wheel");
static_assert(sozo::rainbowHueForPixel(50000, 30, 60, 1) == 17232,
              "rainbow hue arithmetic must wrap around the 16-bit wheel");

const sozo::AudioFrame completeAudioFrame{12.0F, 34.0F, 56.0F, 78.0F,
                                          90.0F, 123U, true};

static_assert(sozo::clampAudioLevel(-1.0F) == 0.0F,
              "audio values below silence must clamp to zero");
static_assert(sozo::clampAudioLevel(1.5F) == 1.0F,
              "audio values above the normalized range must clamp to one");

void setup() {}
void loop() {}
