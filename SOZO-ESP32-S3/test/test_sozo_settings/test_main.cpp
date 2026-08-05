#include <Arduino.h>
#include <SettingsStore.h>

static_assert(!sozo::isSettingsSaveDue(false, 0, 5000),
              "clean settings must never trigger an NVS write");
static_assert(!sozo::isSettingsSaveDue(true, 50, 1049),
              "settings must wait for the full one-second debounce window");
static_assert(sozo::isSettingsSaveDue(true, 50, 1050),
              "settings must write once the debounce window elapsed");
static_assert(sozo::isSettingsSaveDue(true, 0xFFFFFF00U, 0x000003E9U),
              "the debounce calculation must remain correct across millis rollover");

void setup() {}
void loop() {}
