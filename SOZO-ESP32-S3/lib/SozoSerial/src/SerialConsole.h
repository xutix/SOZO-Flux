#pragma once

#include <Arduino.h>

#include <CommandRouter.h>

namespace sozo {

class SerialConsole {
 public:
  SerialConsole(Stream &stream, CommandRouter &router);

  void begin();
  void tick();

 private:
  void processBufferedNumber();
  void printStatus();
  void printHelp();

  Stream &stream_;
  CommandRouter &router_;
  char inputBuffer_[8];
  uint8_t inputLength_;
  uint32_t lastInputAt_;
};

}  // namespace sozo
