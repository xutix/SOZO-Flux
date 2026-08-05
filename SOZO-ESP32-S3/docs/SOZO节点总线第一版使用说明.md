# SOZO 节点总线第一版使用说明

更新时间：2026-08-01

## 1. 当前硬件

| 角色 | 硬件 | 工程 | 串口 |
| --- | --- | --- | --- |
| 主设备 / Coordinator | ESP32-S3 | `SOZO-ESP32-n8r8` | COM19 |
| 副设备 / ExtensionNode | ESP32-C3 SuperMini | `SOZO-ESP32-C3` | COM9 |

COM5 属于其他项目，本项目的编译、监视和烧录配置均不使用 COM5。

C3 第一版灯带参数暂时为：

- 数据引脚：GPIO4
- 灯珠数量：60
- 配对按钮：板载 BOOT，GPIO9

这些参数集中放在 `NodeHardwareConfig.h` 和 C3 的 `platformio.ini` 中。以后确认实际灯带后，只改硬件配置，不改节点协议和灯效同步逻辑。

## 2. 第一版已经实现的能力

- 主设备继续通过路由器提供 `http://sozo-esp32.local/` 网页。
- 主设备作为 BLE Central，C3 作为 BLE Peripheral。
- C3 在本地渲染灯效；主设备只发送场景参数，不连续发送每颗灯珠的 RGB 数据。
- 普通灯效同步完整参数、主色、副色、亮度和动画时间基准。
- 音频灯效只发送音量、快慢能量和节拍脉冲等标准化特征，不发送原始 PCM 音频。
- C3 断线后保持最后一个有效场景；重连后主设备重新发送完整场景快照。
- C3 保留自己的 GPIO、灯珠数量和物理布局，主设备场景不会覆盖这些硬件配置。
- 消息使用协议版本、节点编号、序号、场景版本、关联编号、长度和 CRC16 校验。
- Service 请求采用异步关联编号和超时，不阻塞网页、Wi-Fi、灯效或 Arduino 主循环。
- 主设备提供只读节点状态接口：`GET /api/nodes`。

## 3. 配对和重新配对

### 首次配对

1. 保持主设备和 C3 正常通电。
2. 主设备会持续搜索 SOZO 节点。
3. 在 C3 已经正常启动后，按住 BOOT 约 1.5 秒再松开。
4. C3 开启 60 秒配对窗口。
5. 主设备完成加密连接、能力读取和 SOZO 绑定。
6. 绑定关系写入 C3 的 NVS，后续断电重启会自动重连。

首次绑定不仅保存 Coordinator 的 NodeId，还保存首次连接主设备的 BLE 身份。绑定后，C3 广播连接白名单只接受这个主设备，不能仅靠伪造 NodeId 接管。

### 清除绑定

在 C3 正常运行时持续按住 BOOT 8 秒。C3 会清除 SOZO 绑定、BLE bond 和连接白名单，然后自动重启。重启后再按住 BOOT 1.5 秒即可与新的主设备配对。

上电时一直按住 BOOT 不会触发运行时配对逻辑；必须先松开，再重新长按，以免影响芯片下载模式。

## 4. 软件结构

```text
网页 / 手机 / 电脑 / 未来语音与实体控制器
                    │
                    ▼
        Coordinator（ESP32-S3）
        ├─ NodeCoordinator
        ├─ NodeRegistry（固定容量 8）
        ├─ SozoBus
        └─ NodeTransport 接口
                    │
          BleCentralAdapter
                    │
                    ▼
        ExtensionNode（ESP32-C3）
        ├─ BlePeripheralAdapter
        ├─ NodeSceneRuntime
        ├─ NodeBindingStore
        ├─ PairingWindow
        └─ C3LightingHardware
```

`NodeCoordinator` 不依赖 NimBLE 或具体 BLE 类，只依赖 `NodeTransport` 接口。以后增加 Wi-Fi、ESP-NOW 或串口节点时，可以新增 Transport Adapter，不需要重写灯效、场景、节点注册和网页业务。

公共协议位于独立工程 `SOZO-Common`：

- `SozoNodeProtocol`：固定小端二进制封装、CRC、类型化消息。
- `SozoBusCore`：Topic 发布订阅、异步 Service 请求与响应。
- `SozoBleContract`：SOZO BLE Service 和 Characteristic UUID。

## 5. 当前范围与后续扩展

第一条可验收链路是一个主设备连接一个 C3 灯带节点。节点注册表已经预留 8 个节点，但多 BLE 并发连接、网页节点管理、Independent 独立灯效、开关、旋钮和传感器输入属于后续纵向切片。

后续节点通过能力位声明自己具备哪些模块，例如：

- 灯光输出
- 按钮输入
- 旋钮输入
- 传感器输入
- 音频特征输入

设备类型由能力组合决定，不用为“灯带节点、按钮节点、旋钮节点”分别复制一套总线和配对逻辑。

## 6. 验收入口

- 主网页：`http://sozo-esp32.local/`
- 节点状态：`http://sozo-esp32.local/api/nodes`

未按 BOOT 开启 C3 配对窗口时，节点接口应显示 `searching` 且节点列表为空。完成配对后应显示 `ready`，列表中出现 C3 NodeId、灯珠数量、绑定状态、最后心跳、最后场景版本和剩余内存。

## 7. 蓝牙断线与重连保护

主设备的蓝牙连接、服务发现、加密认证、能力读取和灯效写入都在独立后台任务中执行，不占用网页、Wi-Fi 和主灯带所在的 Arduino 主循环。C3 断电、上电、重启或认证失败时，主网页和主灯带应继续工作。

后台操作设有固定超时：连接 6 秒、服务发现 5 秒、认证 6 秒、有响应写入 3 秒。超时后主设备会取消当前连接尝试，等待 2 秒后重试。已经取消的旧连接即使稍后返回成功，也不会覆盖当前连接状态。

待发送数据使用固定容量队列：控制消息按顺序保留，音频特征只保留最新一帧，避免高速音频数据挤掉灯光控制指令。所有跨任务队列都有固定上限，不会因副设备长期离线而持续占用内存。

`GET /api/nodes` 的重连诊断字段：

- `operationStage`：`idle`、`connecting`、`discovering`、`authenticating` 或 `writing`。
- `workerBusy`：蓝牙后台任务当前是否占用一次连接生命周期。
- `bleTimeouts`：本次主设备启动后累计发生的蓝牙操作超时次数。

实机验收时，在 C3 断电和重新上电期间持续请求 `/api/status` 与 `/api/nodes`；两项请求应持续成功，主灯带不停止，C3 最终重新回到 `ready` 并收到完整场景。
