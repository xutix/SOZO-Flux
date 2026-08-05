#include <Arduino.h>
#include <NetworkManager.h>

static_assert(sozo::isProvisioningNetworkState(
                  sozo::NetworkState::Provisioning),
              "the setup access point must be distinguishable from router mode");
static_assert(!sozo::isProvisioningNetworkState(
                  sozo::NetworkState::Connected),
              "a router connection must not be reported as setup mode");

void setup() {}
void loop() {}
