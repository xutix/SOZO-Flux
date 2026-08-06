# SOZO Flux 路线图

## v0.1 — 平台收口

- [x] 统一 SOZO Flux 产品身份
- [x] 建立平台、Gateway、Node 与 Protocol 版本体系
- [x] 定义许可证和贡献边界
- [x] 建立固件构建、测试与格式 CI
- [x] 建立架构、路线图和发布流程文档
- [ ] 完成 S3/C3 目标硬件稳定性验收并记录结果

## v0.2 — Device Framework

- `Device`、`Identity`、`Capability`、`State`、`Command`、`Event` 共享模型
- 设备发现、能力声明和状态快照
- 稳定错误模型与诊断事件
- 首个非灯光能力接入验证

## v0.3 — Scene System

- 场景定义、参数、动作和目标设备
- 场景版本、校验、应用与回滚
- 多节点一致性和离线节点处理
- Focus、Gaming 等业务场景，不把灯效直接等同于场景

## v0.4 — 多节点与产品接入

- 节点注册、分组、健康状态和升级策略
- SOZO Dock 接入 Flux 能力模型
- Gateway 与 Node 的兼容矩阵

## 后续

- SOZO Flux Studio 桌面控制体验
- 用户、设备与场景同步服务
- 更多传输适配器和硬件节点

路线图表达顺序而非日期承诺。上一阶段的完成标准未满足前，不横向增加新模块。
