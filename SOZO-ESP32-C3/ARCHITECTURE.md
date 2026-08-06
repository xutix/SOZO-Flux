# SOZO Flux Node C3 架构入口

本工程是 SOZO 扩展节点：它通过 BLE 等传输接收场景或发布实体输入，在本地持续渲染自己的输出，并维护配对与节点私有硬件状态。

共同架构规则、术语、模块职责和接入流程以共享手册为准：

- [SOZO 代码架构手册](../SOZO-Common/docs/SOZO代码架构手册.md)
- [SOZO 统一术语](../SOZO-Common/CONTEXT.md)

本工程只能依赖 `SOZO-Common` 的核心模块，不能编译依赖 S3 工程的 `lib`。BLE、配对、LED 输出与效果渲染必须通过各自的接口协作，任何通信事件都不能直接暂停或刷新灯带。
