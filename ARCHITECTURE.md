# SOZO Flux 架构

## 平台目标

SOZO Flux 是空间设备的控制与光场底座，不是某一块开发板或某一套灯效。平台围绕设备、
能力、状态、命令和事件组织，使 Dock、灯光节点、工具车与未来模块可以共享同一套语义和协议。

## 运行时数据流

```text
User / Device Event
        |
        v
Transport Adapter (Web / Serial / BLE / future Wi-Fi, ESP-NOW, Voice)
        |
        v
ControlCommand + source + protocol version
        |
        v
Scene / Target Command ---- validation / delayed persistence
        |
        v
Named Scene Assignments ----> Per-node Desired Lighting State
        |                                  |
        +----> Local Light Runtime --------+----> Shared Lighting Core ----> S3 Output
                                           |
                                           +----> Flux Node Bus ----> C3 Runtime
```

任何传输适配器都不得直接修改 LED、NVS 或设备私有状态。每条灯带拥有自己的最终目标状态；
场景激活只更新场景列出的节点，未列出的节点保持上一次状态，直接控制只更新一个节点。

## 当前模块边界

| 边界 | 当前目录 | 职责 |
|---|---|---|
| Hub firmware | `SOZO-ESP32-S3` | 组合根、网页/串口入口、Wi-Fi、音频、节点协调、可选本地灯光节点 |
| Node firmware | `SOZO-ESP32-C3` | BLE 外设、绑定、节点私有配置、本地灯带输出 |
| Domain core | `SOZO-Common/lib/SozoDomain` | 状态、命令、来源、参数约束 |
| Scene core | `SOZO-Common/lib/SozoSceneCore` | 命名场景、逐节点目标状态、投递状态与灯光节点运行时 |
| Lighting core | `SOZO-Common/lib/SozoLightingCore` | 灯效、帧、几何映射与输出端口 |
| Protocol | `SOZO-Common/lib/SozoNodeProtocol` | 二进制封装、消息与 CRC |
| Bus | `SOZO-Common/lib/SozoBusCore` | 发布订阅和异步请求/响应 |
| Spatial model | `SOZO-Common/lib/SpatialLightCore` | 灯带数量与空间布局 |

依赖只能从设备工程指向 Common。S3 与 C3 之间禁止源码依赖；它们只能通过 Flux Protocol 通信。

## 目标仓库边界

本阶段不移动现有代码。未来只有在构建脚本、文档链接和版本迁移可以一次性验证时，才按下表迁移：

```text
firmware/
  flux-gateway-s3/   <- SOZO-ESP32-S3
  flux-node-c3/      <- SOZO-ESP32-C3
core/
  device/
  lighting/
  protocol/          <- SOZO-Common 中对应模块
tools/               <- 仓库级验证与发布工具
docs/                <- 架构决策、版本与协议文档
examples/            <- 独立、可构建的接入示例
```

迁移目录本身不创造业务价值；在 Device Framework 和 Scene System 进入实现前，保持当前路径稳定。

## 稳定边界

- 本仓库是正式 S3/C3 固件的唯一构建来源；仓库外硬件相同的工程副本不是可替换实现。
- 产品身份、`sozo-flux` mDNS/OTA 主机名、`SOZO-FLUX-SETUP` 配网热点和
  `SOZO-FLUX-C3-` BLE 前缀是受保护的部署兼容性边界，详见
  [项目身份与烧录保护](docs/PROJECT-GUARDRAILS.md)。
- `sozo` C++ 命名空间和 `SOZO_*` 编译宏是内部技术标识，本次品牌重命名不强制破坏它们。
- S3 的 `sozo-light` NVS 命名空间保持不变，以兼容已经部署的配置。
- BLE UUID 与协议 magic 保持不变；完整节点布局从 protocol v2 起传输。
- 协议版本独立于平台和固件版本，兼容性规则见 `docs/VERSIONING.md`。

## 下一组架构能力

收口完成后按顺序建立 Device Framework、Capability Registry、Scene System 和 Multi-node
Management。没有进入共享模型的概念，不应先在网页或硬件适配器中形成新的旁路状态。
