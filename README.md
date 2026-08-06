# SOZO Flux

> 面向桌面空间、工程工作站与创客设备的模块化智能光场平台。

[![Firmware Build](https://github.com/xutix/SOZO-Flux/actions/workflows/build-firmware.yml/badge.svg)](https://github.com/xutix/SOZO-Flux/actions/workflows/build-firmware.yml)
[![Tests](https://github.com/xutix/SOZO-Flux/actions/workflows/test.yml/badge.svg)](https://github.com/xutix/SOZO-Flux/actions/workflows/test.yml)
[![Version](https://img.shields.io/badge/version-v0.1.0--alpha-f3a83b)](VERSION)

SOZO Flux 不是单一音乐灯固件，而是 SOZO 旗下的空间灯光技术平台。它以 ESP32-S3
协调器、ESP32-C3 扩展节点和平台无关的共享核心组成第一条可运行链路，为后续的场景、
设备能力、多节点管理与桌面软件提供稳定底座。

当前版本为 `v0.1.0-alpha`。它适合开发、验证与小规模硬件实验，尚不承诺生产稳定性。

## 系统结构

```text
Web / Serial / future Desktop App
                |
         Control Command
                |
      ESP32-S3 Coordinator
         /             \
  Main light        Flux node bus
                         |
                  ESP32-C3 Light Node

Shared by both devices: domain + lighting + protocol + bus core
```

控制入口只能产生 `ControlCommand`，由协调器校验并更新权威状态，再由能力处理器和硬件
适配器执行。网页、串口、BLE 或未来桌面软件不得直接写 GPIO、LED 状态或 NVS。

## 仓库组成

| 目录 | 角色 | 当前硬件 |
|---|---|---|
| `SOZO-ESP32-S3/` | Flux Gateway S3：协调器、主灯、网页、音频与节点管理 | ESP32-S3-WROOM-1-N8R8、INMP441、WS2812 |
| `SOZO-ESP32-C3/` | Flux Node C3：BLE 扩展灯光节点 | ESP32-C3 SuperMini、WS2812 |
| `SOZO-Common/` | 平台无关的领域、灯效、空间布局、节点协议与消息总线 | Native / C++17 |

现阶段保留三个已有工程目录，不进行高风险搬迁。计划中的 `firmware/`、`core/`、
`tools/`、`docs/` 与 `examples/` 边界见 [ARCHITECTURE.md](ARCHITECTURE.md)。

## 构建

需要 [PlatformIO Core](https://docs.platformio.org/en/latest/core/index.html)。从仓库根目录运行：

```powershell
platformio run --project-dir SOZO-ESP32-S3
platformio run --project-dir SOZO-ESP32-C3
```

本地 OTA 构建可通过私有 PlatformIO 配置定义 `SOZO_OTA_PASSWORD`。仓库不会硬编码密码：

```ini
build_flags =
  -DSOZO_OTA_PASSWORD=\"your-local-password\"
```

## 验证

```powershell
platformio test --project-dir SOZO-Common --environment native
platformio test --project-dir SOZO-ESP32-S3 --environment native-node
platformio test --project-dir SOZO-ESP32-C3 --environment native-node
node --test test/*.test.mjs SOZO-ESP32-S3/test/*.test.mjs
python tools/check_versions.py
python tools/check_text_format.py
```

CI 会构建两套完整固件并在 Linux 上运行 Common、Gateway 节点逻辑和 C3 节点运行时测试；
烧录和真实灯带验收仍需在目标硬件上完成。

## 版本

平台、S3 网关、C3 节点和 Flux Protocol 独立版本化，以根目录 [VERSION](VERSION) 为发布
清单。兼容性规则与发布步骤见 [docs/VERSIONING.md](docs/VERSIONING.md) 和
[docs/RELEASING.md](docs/RELEASING.md)。

## 开发约束

- 新设备先声明身份、能力、状态、命令与事件，再接入硬件。
- 新灯效只进入共享灯效核心，S3 与 C3 不各自复制算法。
- 协议扩展必须保持固定大小、版本可识别并补充编解码测试。
- 当前收口阶段不继续横向增加灯效或传输方式。

详细规则见 [CONTRIBUTING.md](CONTRIBUTING.md)，后续顺序见 [ROADMAP.md](ROADMAP.md)。

## 许可证

当前仓库为源码可见、保留全部权利，未授予复制、修改、分发或商业使用许可。详见
[LICENSE](LICENSE)。未来可将 `SOZO-Common` 中适合生态协作的协议层拆分并采用独立开源许可。
