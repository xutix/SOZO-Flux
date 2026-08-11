# Changelog

## Unreleased

- Flux Protocol 2 adds complete LED geometry exchange and per-node scene delivery state for S3/C3 lighting orchestration.

- C3 `0.2.1-alpha` 支持通过已绑定且加密的 BLE 连接更新固件。
- Gateway 网页新增按节点选择固件、传输进度和错误状态；同一时间只更新一个 C3，其它节点保持在线。
- C3 在备用 OTA 分区写入，完整大小与 SHA-256 校验通过后才切换并重启；断链、超时或校验失败继续保留旧固件。

本项目从 `v0.1.0-alpha` 起采用语义化版本。预发布版本允许接口调整，但协议破坏性变更仍需
显式提升 Flux Protocol 版本。

## [0.1.0-alpha] - 2026-08-06

### Included

- ESP32-S3 Gateway：主灯控制、INMP441 音频分析、网页、Wi-Fi 与 BLE 节点协调。
- ESP32-C3 Node：本地灯带、BLE 绑定、跟随/独立控制与灯珠数量配置。
- Common：领域模型、空间布局、共享灯效、节点协议和消息总线。

### Platform closure

- 项目正式命名为 SOZO Flux。
- 增加独立版本清单、许可证、架构、路线图和贡献规范。
- 增加固件构建、测试、版本一致性和文本格式自动检查。

[0.1.0-alpha]: https://github.com/xutix/SOZO-Flux/releases/tag/v0.1.0-alpha
