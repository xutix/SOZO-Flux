# SOZO Flux Gateway S3 架构入口

本工程是 SOZO 空间的协调器：维护权威场景、接收控制命令、管理节点生命周期，并通过适配器连接网页、串口、Wi-Fi、BLE Central、主灯带、NVS 与板载音频。

共同架构规则、术语、模块职责和接入流程以共享手册为准：

- [SOZO 代码架构手册](../SOZO-Common/docs/SOZO代码架构手册.md)
- [SOZO 统一术语](../SOZO-Common/CONTEXT.md)

本工程只能依赖 `SOZO-Common` 的核心模块。它不得依赖 C3 工程，也不得保存 C3 的 GPIO、物理灯珠数或节点私有状态。
