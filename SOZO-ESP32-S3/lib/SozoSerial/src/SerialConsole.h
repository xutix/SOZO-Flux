#pragma once

#include <Arduino.h>

#include <LightingControlApplication.h>

namespace sozo {

class SerialConsole {
 public:
  SerialConsole(Stream &stream, LightingControlApplication &lighting);

  void begin();
  void tick();

 private:
  void processBufferedNumber();
  void printStatus();
  void printHelp();

  Stream &stream_;
  LightingControlApplication &lighting_;
  char inputBuffer_[8];
  uint8_t inputLength_;
  uint32_t lastInputAt_;
};

}  // namespace sozo
