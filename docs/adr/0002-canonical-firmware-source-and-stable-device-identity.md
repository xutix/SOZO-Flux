# 固定正式固件来源与设备身份

SOZO Flux 的正式设备只允许从单体仓库内的 `SOZO-ESP32-S3`、`SOZO-ESP32-C3` 和
`SOZO-Common` 构建，仓库外硬件相同的旧工程或实验副本不得用于烧录。S3 的
`sozo-flux` mDNS/OTA 主机名、`SOZO-FLUX-SETUP` 配网热点和 C3 的
`SOZO-FLUX-C3-` BLE 前缀属于部署兼容性边界；固定这些值可以避免整包固件替换后让既有
网页入口、设备发现和节点识别失效，任何身份迁移都必须获得用户明确授权并作为独立变更完成。
