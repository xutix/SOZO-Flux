#include <Arduino.h>
#include <LightingControlApplication.h>

static_assert(sozo::isSourceAllowedForCommand(
                  sozo::ControlSource::Web,
                  sozo::ControlCommandType::SetEffect),
              "web requests must be allowed to choose a light effect");
static_assert(!sozo::isSourceAllowedForCommand(
                  sozo::ControlSource::PcAudio,
                  sozo::ControlCommandType::DeviceAction),
              "a future PC audio source must never restart or reset the device");

void setup() {}
void loop() {}
