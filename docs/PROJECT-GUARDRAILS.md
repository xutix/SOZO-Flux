# SOZO Flux 项目身份与烧录保护

这份文档定义 SOZO Flux 当前不可被普通开发、重构或故障排查改变的项目身份和部署边界。
它用于避免把硬件相同但产品身份不同的旧工程、实验工程或独立副本烧录到正式设备。

## 唯一正式工程

正式固件只能从本仓库构建：

| 目标 | 唯一允许的工程目录 |
|---|---|
| ESP32-S3 Flux Hub | `<SOZO 仓库>/SOZO-ESP32-S3/` |
| ESP32-C3 Flux Node | `<SOZO 仓库>/SOZO-ESP32-C3/` |
| S3/C3 共享核心 | `<SOZO 仓库>/SOZO-Common/` |

仓库外的 `SOZO-ESP32-n8r8`、独立 `SOZO-ESP32-C3` 以及任何旧副本都不是正式烧录
来源。即使它们可以编译、芯片型号相同、串口在线或网页看起来相似，也不得用于正式设备。

## 受保护标识

以下值是产品兼容性的一部分，除非用户明确提出“修改产品身份”，否则不允许修改：

| 标识 | 固定值 |
|---|---|
| 产品名 | `SOZO Flux` |
| S3 项目名 | `SOZO Flux Gateway S3` |
| S3 Wi-Fi/mDNS 主机名 | `sozo-flux` |
| S3 OTA 主机名 | `sozo-flux` |
| 局域网网页入口 | `http://sozo-flux.local` |
| S3 配网热点 | `SOZO-FLUX-SETUP` |
| C3 BLE 名称前缀 | `SOZO-FLUX-C3-` |
| S3 网页标题 | `SOZO Flux 空间灯光` |

相关源代码中的重复值必须保持一致。不得为了绕过 mDNS、OTA、浏览器缓存、构建错误或
烧录错误而把它们临时改成 `sozo-esp32` 或其他名字。

## 烧录前检查

1. 执行 `git rev-parse --show-toplevel`，结果必须是本 `SOZO` 仓库。
2. 检查将要使用的 `platformio.ini` 位于上表对应的正式工程目录。
3. S3 构建前确认 mDNS 与 OTA 主机名均为 `sozo-flux`。
4. C3 构建前确认 BLE 名称以前缀 `SOZO-FLUX-C3-` 开头。
5. 使用显式 `--project-dir` 构建；不能仅凭当前工作目录、串口号或板型推断工程身份。
6. 在真正上传前再次记录“目标设备、串口或 OTA 地址、固件工程目录”三者的对应关系。

建议从仓库根目录执行：

```powershell
platformio run --project-dir SOZO-ESP32-S3
platformio run --project-dir SOZO-ESP32-C3
```

## 烧录后验收

### S3

- `sozo-flux.local` 能解析到刚烧录设备的 IP。
- `http://sozo-flux.local` 可以打开，页面标题为 `SOZO Flux 空间灯光`。
- 状态接口报告的 mDNS 名称为 `sozo-flux.local`。
- 串口启动信息中的项目名为 `SOZO Flux Gateway S3`。

### C3

- BLE 广播名称以前缀 `SOZO-FLUX-C3-` 开头。
- 串口启动信息显示 `SOZO Flux C3`，节点 ID 与目标硬件一致。
- S3 节点列表显示的节点 ID 与刚烧录的设备一致。

任一检查失败，都应停止继续烧录其他设备，先核对工程目录和固件身份。不得通过改名来
掩盖失败。

## 变更规则

普通功能开发可以修改灯效、网页、颜色映射、协议实现和硬件适配器，但不能顺带改变本
文档列出的身份与正式工程边界。如果未来确需品牌迁移或主机名迁移，必须由用户明确授权，
同时更新代码、文档、自动化测试和兼容迁移方案，并在烧录前单独确认。
