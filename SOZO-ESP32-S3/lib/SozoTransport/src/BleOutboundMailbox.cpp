#include <BleOutboundMailbox.h>

namespace sozo {

bool BleOutboundMailbox::push(const node::Envelope &envelope) {
  if (envelope.messageType == node::MessageType::AudioFeatures) {
    latestAudio_ = envelope;
    hasAudio_ = true;
    return true;
  }
  if (controlCount_ == kControlCapacity) return false;
  controls_[controlTail_] = envelope;
  controlTail_ = (controlTail_ + 1U) % kControlCapacity;
  ++controlCount_;
  return true;
}

bool BleOutboundMailbox::pop(node::Envelope &envelope) {
  if (controlCount_ > 0) {
    envelope = controls_[controlHead_];
    controlHead_ = (controlHead_ + 1U) % kControlCapacity;
    --controlCount_;
    return true;
  }
  if (!hasAudio_) return false;
  envelope = latestAudio_;
  hasAudio_ = false;
  return true;
}

void BleOutboundMailbox::clear() {
  controlHead_ = 0;
  controlTail_ = 0;
  controlCount_ = 0;
  hasAudio_ = false;
}

size_t BleOutboundMailbox::controlCount() const { return controlCount_; }

bool BleOutboundMailbox::hasAudio() const { return hasAudio_; }

}  // namespace sozo
