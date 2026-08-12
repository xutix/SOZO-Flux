# SOZO Flux 项目指令

任何 agent 修改灯光、网页、节点控制或持久化前，还必须完整阅读
`docs/LIGHTING-ARCHITECTURE.md`，并在完成后运行
`tools/verify-module-dependencies.ps1`。不得恢复文档中列出的旧双控制链。

## 受保护的项目身份

- 本仓库根目录是 SOZO Flux 固件唯一允许的构建与烧录来源。
- S3 只能使用本仓库的 `SOZO-ESP32-S3/`；C3 只能使用本仓库的
  `SOZO-ESP32-C3/`；共享代码只能使用本仓库的 `SOZO-Common/`。
- 不得从仓库外的同名、旧版或实验目录构建或烧录设备。尤其不得使用相邻的
  `SOZO-ESP32-n8r8` 或其他独立 `SOZO-ESP32-C3` 目录替代本仓库工程。
- 未经用户明确要求更改产品身份，不得修改以下值：
  - 产品名 `SOZO Flux`
  - S3 项目名 `SOZO Flux Gateway S3`
  - S3 mDNS 与 OTA 主机名 `sozo-flux`
  - 局域网入口 `http://sozo-flux.local`
  - 配网热点 `SOZO-FLUX-SETUP`
  - C3 BLE 名称前缀 `SOZO-FLUX-C3-`
- 不得以解决构建、烧录、网络发现或兼容问题为理由悄悄重命名上述标识。

## 烧录硬性检查

每次构建或烧录前必须确认仓库根目录，并显式指定工程目录：

```powershell
git rev-parse --show-toplevel
platformio run --project-dir SOZO-ESP32-S3
platformio run --project-dir SOZO-ESP32-C3
```

根目录必须是本 `SOZO` 仓库。串口号、芯片型号相同或网页暂时可以访问，都不能替代工程
身份检查。S3 烧录后必须验证 `sozo-flux.local`、页面标题和状态接口；C3 烧录后必须验证
BLE 名称前缀和节点身份。

完整规则见 `docs/PROJECT-GUARDRAILS.md`。
