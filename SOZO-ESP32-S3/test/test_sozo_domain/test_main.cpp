#include <Arduino.h>
#include <SozoDomain.h>

using namespace sozo;

constexpr LightingSettings defaultSettings = makeDefaultLightingSettings();
constexpr PersistedLightingState defaultState = makeDefaultPersistedLightingState();

const ControlCommand validBrightness{
    kControlProtocolVersion,
    ControlSource::Web,
    1,
    ControlCommandType::SetParameter,
    LightingParameter::Brightness,
    180,
    {0, 0, 0},
    makeDefaultSpatialLayout(),
};

static_assert(isKnownEffect(EffectMode::Aurora),
              "supported effects must be known to every control source");
static_assert(!isKnownEffect(EffectMode::Off),
              "off is an output state, not a selectable effect");
static_assert(clampLightingValue(LightingParameter::Brightness, 999) == 255,
              "brightness must honor the shared upper bound");
static_assert(defaultSettings.flowSpeed == 45,
              "the migrated effect catalog must keep its current flow speed");
static_assert(defaultSettings.cometTail == 28,
              "the migrated effect catalog must keep its current comet tail");
static_assert(defaultSettings.audioSensitivityX100 == 100,
              "the migrated effect catalog must keep its current audio sensitivity");
static_assert(defaultState.brightness == 50,
              "the persisted state must preserve the current default brightness");
static_assert(defaultState.primaryColor.red == 255 &&
                  defaultState.primaryColor.green == 120 &&
                  defaultState.primaryColor.blue == 0,
              "the persisted state must preserve the current default color");
static_assert(defaultState.layout.activeCount == 144,
              "the persisted state must preserve the installed LED count");
static_assert(isParameterCommandWellFormed(
                  kControlProtocolVersion, ControlSource::Web,
                  LightingParameter::Brightness, 180),
              "a bounded web parameter command must be accepted by the domain");
static_assert(!isParameterCommandWellFormed(
                  kControlProtocolVersion, ControlSource::Web,
                  LightingParameter::Brightness, 999),
              "the router must reject out-of-range parameter values");

void setup() {}
void loop() {}
