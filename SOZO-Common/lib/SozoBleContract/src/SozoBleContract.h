#pragma once

namespace sozo::ble {

constexpr char kServiceUuid[] = "8f1e0001-7a4b-4c2d-9e10-534f5a4f0001";
constexpr char kControlCharacteristicUuid[] =
    "8f1e0002-7a4b-4c2d-9e10-534f5a4f0001";
constexpr char kEventCharacteristicUuid[] =
    "8f1e0003-7a4b-4c2d-9e10-534f5a4f0001";
constexpr char kInfoCharacteristicUuid[] =
    "8f1e0004-7a4b-4c2d-9e10-534f5a4f0001";

constexpr unsigned short kPreferredMtu = 247;

}  // namespace sozo::ble
