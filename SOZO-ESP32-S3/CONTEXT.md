# SOZO Flux Hub 术语

本上下文描述 Flux Hub 的协调边界。共享的空间、节点和灯光词汇以 [SOZO Flux 统一术语](../SOZO-Common/CONTEXT.md) 为准。

## Language

**Hub 运行时（Hub Runtime）**：
承载协调器、控制入口和能力适配器的组合边界；它不以任何一条灯带作为状态权威。

**本地能力（Local Capability）**：
与 Hub 部署在同一硬件上、但通过标准能力接口参与空间的能力。

**节点群组（Node Fleet）**：
已登记到同一空间、由协调器观察和调度的远程节点集合。

**传输适配器（Transport Adapter）**：
把网页、串口、BLE 或未来控制入口的数据转换为控制命令的边界。

**空间运行快照（Space Runtime Snapshot）**：
可对外公开的空间场景、网络、音频和节点事实，不包含密码或节点私有硬件配置。
