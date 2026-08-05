#include <NodeBindingStore.h>

namespace sozo::c3 {

constexpr char NodeBindingStore::kNamespace[];

NodeBinding NodeBindingStore::load() {
  Preferences preferences;
  // 使用读写模式创建首次启动所需的命名空间，避免输出误导性的
  // NVS“未找到”错误；这里只读取，不写入绑定值。
  if (!preferences.begin(kNamespace, false)) return {};
  const bool bound = preferences.getBool("bound", false);
  const node::NodeId coordinator = preferences.getUInt("coord", 0);
  const uint64_t bleIdentityAddress = preferences.getULong64("peer", 0);
  const uint8_t bleIdentityAddressType = preferences.getUChar("peerType", 0);
  preferences.end();
  NodeBinding binding{};
  binding.bound = bound && coordinator != 0 && bleIdentityAddress != 0;
  binding.coordinatorNodeId = coordinator;
  binding.bleIdentityAddress = bleIdentityAddress;
  binding.bleIdentityAddressType = bleIdentityAddressType;
  return binding;
}

bool NodeBindingStore::save(const node::NodeId coordinatorNodeId,
                            const uint64_t bleIdentityAddress,
                            const uint8_t bleIdentityAddressType) {
  if (coordinatorNodeId == 0 || coordinatorNodeId == node::kBroadcastNodeId) {
    return false;
  }
  if (bleIdentityAddress == 0) return false;
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) return false;
  const bool coordinatorSaved =
      preferences.putUInt("coord", coordinatorNodeId) == sizeof(uint32_t);
  const bool addressSaved =
      preferences.putULong64("peer", bleIdentityAddress) == sizeof(uint64_t);
  const bool addressTypeSaved =
      preferences.putUChar("peerType", bleIdentityAddressType) ==
      sizeof(uint8_t);
  const bool boundSaved = preferences.putBool("bound", true) == sizeof(bool);
  preferences.end();
  return coordinatorSaved && addressSaved && addressTypeSaved && boundSaved;
}

bool NodeBindingStore::clear() {
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) return false;
  const bool cleared = preferences.clear();
  preferences.end();
  return cleared;
}

}  // namespace sozo::c3
