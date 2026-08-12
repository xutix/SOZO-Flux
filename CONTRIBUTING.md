# 为 SOZO Flux 贡献

SOZO Flux 当前处于 `v0.1.x` 平台收口期。变更应优先提高边界清晰度、可靠性和可验证性，
不以增加功能数量为目标。

## 开发流程

1. 从最新 `main` 创建短生命周期分支：`feat/`、`fix/`、`docs/` 或 `chore/`。
2. 先明确变更属于 Gateway、Node、Common 还是工具/文档边界。
3. 在最小可验证接口处先补测试，再实现行为。
4. 本地完成与变更相关的测试和构建。
5. 通过 Pull Request 合并；`main` 不接受直接推送。

提交信息使用简短的命令式格式，例如 `feat: add node capability snapshot`。

## 固件身份与烧录保护

- 正式固件只允许从本仓库的 `SOZO-ESP32-S3/`、`SOZO-ESP32-C3/` 和
  `SOZO-Common/` 构建，不得从仓库外的旧工程、独立副本或实验目录烧录。
- 未经用户明确要求修改产品身份，不得改动 `SOZO Flux`、`sozo-flux`、
  `SOZO-FLUX-SETUP`、`SOZO-FLUX-C3-` 及对应网页标题。
- 每次烧录必须在操作前确认仓库根目录和显式工程目录，在操作后验证设备身份与发现入口。
- 详细检查清单以 [项目身份与烧录保护](docs/PROJECT-GUARDRAILS.md) 为准。

## 添加设备

先在文档和共享领域模型中定义：

- `Identity`：稳定设备身份和型号；
- `Capability`：设备能提供什么，不描述 GPIO 细节；
- `State`：可公开的运行状态；
- `Command`：允许改变状态的类型化意图；
- `Event`：设备向系统发布的事实。

硬件实现应位于设备工程的适配器层，不得让 Common 依赖 Arduino、GPIO、NVS 或具体 LED 库。

## 添加灯效

1. 在 `SOZO-Common/lib/SozoDomain` 增加领域枚举和参数约束。
2. 在 `SOZO-Common/lib/SozoLightingCore` 实现一次共享渲染逻辑。
3. 为边界值、不同灯珠数和空间映射补 Native 测试。
4. S3/C3 只提供硬件输出适配器，不复制灯效算法。
5. 最后才把参数暴露给网页或未来 Studio。

## 扩展协议

- 保持 `SozoNodeProtocol` 平台无关、固定容量、无动态分配。
- 新字段必须有明确字节序、范围、兼容策略和编解码往返测试。
- 破坏兼容性的变更必须提升 `protocol` 版本，不能只提升固件版本。
- 不传输 PCM 或逐灯 RGB 帧；传输标准化特征、场景意图和状态快照。

## 合并门禁

Pull Request 必须通过固件构建、Native/契约测试、硬件测试编译、版本一致性和文本格式检查。
涉及实体硬件行为的变更还应在 PR 中记录板型、接线、固件版本和实机结果。
