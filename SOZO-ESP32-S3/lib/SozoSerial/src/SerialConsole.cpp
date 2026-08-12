#include <SerialConsole.h>

#include <stdlib.h>

namespace sozo {

SerialConsole::SerialConsole(Stream &stream,
                             LightingControlApplication &lighting)
    : stream_(stream), lighting_(lighting), inputBuffer_{}, inputLength_(0),
      lastInputAt_(0) {}

void SerialConsole::begin() { printHelp(); }

void SerialConsole::tick() {
  while (stream_.available() > 0) {
    const char command = static_cast<char>(stream_.read());
    lastInputAt_ = millis();
    if (command >= '0' && command <= '9') {
      if (inputLength_ < sizeof(inputBuffer_) - 1) {
        inputBuffer_[inputLength_++] = command;
      } else {
        stream_.println(F("[SERIAL] Input is too long."));
        inputLength_ = 0;
      }
      continue;
    }
    if (command == '\r' || command == '\n') {
      processBufferedNumber();
      continue;
    }
    if (command == ' ') {
      continue;
    }
    processBufferedNumber();
    switch (command) {
      case 's':
      case 'S':
        printStatus();
        break;
      case 'h':
      case 'H':
      case '?':
        printHelp();
        break;
      default:
        stream_.printf("[SERIAL] Unknown command '%c'. Send h for help.\n", command);
        break;
    }
  }
  if (inputLength_ > 0 && millis() - lastInputAt_ >= 300) {
    processBufferedNumber();
  }
}

void SerialConsole::processBufferedNumber() {
  if (inputLength_ == 0) {
    return;
  }
  inputBuffer_[inputLength_] = '\0';
  const int requestedCount = atoi(inputBuffer_);
  if (requestedCount < 0 ||
      requestedCount > spatial_light::kMaxLedCount) {
    stream_.printf("[SERIAL] Invalid count: %d. Enter 0..%u.\n", requestedCount,
                   spatial_light::kMaxLedCount);
  } else {
    const ControlCommand command{
        kControlProtocolVersion,
        ControlSource::Serial,
        0,
        ControlCommandType::SetParameter,
        LightingParameter::ManualLitPixelCount,
        requestedCount,
        {0, 0, 0},
        makeDefaultSpatialLayout(),
    };
    if (lighting_.dispatch(command, millis()).accepted()) {
      stream_.printf(
          "[SERIAL] Space pixel intent: %d (each node clamps locally).\n",
          requestedCount);
    } else {
      stream_.println(F("[SERIAL] Pixel-count command was rejected."));
    }
  }
  inputLength_ = 0;
  inputBuffer_[0] = '\0';
}

void SerialConsole::printStatus() {
  const LightingApplicationSnapshot state = lighting_.snapshot();
  stream_.printf("[LOCAL NODE] Active: %u/%u\n",
                 state.lighting.layout.activeCount,
                 spatial_light::kMaxLedCount);
  stream_.printf("[SPACE] Mode: %u | Brightness: %u | Manual pixels: %d | "
                 "Revision: %lu\n",
                 static_cast<unsigned int>(state.lighting.mode),
                 state.lighting.brightness, state.manualLitPixelCount,
                 static_cast<unsigned long>(state.sceneRevision));
}

void SerialConsole::printHelp() {
  stream_.println();
  stream_.println(F("Serial commands (115200 baud):"));
  stream_.printf("  0..%u = Light the first N active pixels\n",
                 spatial_light::kMaxLedCount);
  stream_.println(F("  s = Print light status"));
  stream_.println(F("  h = Print this help"));
  stream_.println();
}

}  // namespace sozo
