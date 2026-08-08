# SOZO Flux Gateway S3 架构入口

本工程是 SOZO Flux Hub：协调器维护权威空间场景、接收控制命令并管理节点生命周期；适配器连接网页、串口、Wi-Fi、BLE Central、NVS、板载音频和可选的本地灯光节点。

共同架构规则、术语、模块职责和接入流程以共享手册为准：

- [SOZO 代码架构手册](../SOZO-Common/docs/SOZO代码架构手册.md)
- [SOZO 统一术语](../SOZO-Common/CONTEXT.md)

S3 直连灯带不是场景权威，而是与 C3 采用相同运行时语义的本地灯光节点。本工程只能依赖 `SOZO-Common` 的核心模块，不得依赖 C3 工程，也不得保存 C3 的 GPIO、物理灯珠数或节点私有状态。
