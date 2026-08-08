# SOZO 代码架构手册

> 适用范围：`SOZO-Common`、ESP32-S3 Flux Hub、所有 ESP32-C3 或后续扩展节点。
>
> 本手册是 SOZO 固件的共同架构约束。新功能、修复和重构都必须遵守；不符合的旧逻辑要在相关改造中删除，而不是继续叠加。

## 1. 目标

SOZO 是一个可扩展的空间控制系统，而不是一条固定灯带。它将逐步接入灯光节点、实体按钮和旋钮、传感器、BLE、Wi-Fi、ESP-NOW、语音识别与电脑音频。

系统必须做到：

- 改变一种传输方式，不影响灯效渲染。
- 改变一种灯效，不影响 BLE、网页、配对或持久化。
- 增减实际灯珠，不改变场景协议和效果算法。
- 增加一种节点或控制来源，不绕过统一命令路径。
- 单独验证灯效、协议、节点状态和硬件输出。

这里的“解耦”不是模块之间没有通信，而是模块只通过稳定的接口交换明确的数据，不直接读写彼此内部状态。

## 2. 不可违反的规则

### 2.1 单向依赖

编译期依赖只能从外向内：

```text
组合根（main.cpp）
        ↓
适配器：BLE / Wi-Fi / Web / 串口 / GPIO / NVS
        ↓
应用模块：协调、节点运行时、命令处理、生命周期
        ↓
核心模块：领域模型、SozoBus 协议、灯光渲染与几何
```

核心和应用模块不得包含、引用或创建 `NimBLE`、`WiFi`、`WebServer`、`Preferences`、`Adafruit_NeoPixel`、`FastLED`、GPIO 或 Arduino 网络对象。适配器可以依赖内层接口；内层绝不能依赖具体适配器。

### 2.2 单一状态所有者

每份可变状态只能有一个模块拥有写权限：

| 状态 | 唯一所有者 | 其他模块的使用方式 |
|---|---|---|
| 空间场景与版本 | 协调器应用模块 | 读取快照或提交控制命令 |
| 节点本地场景与时钟偏移 | 节点运行时 | 接收已解码场景或读取状态 |
| 灯光帧 | 灯光渲染模块 | 输出模块只消费帧 |
| 物理灯带缓存 | 灯带输出适配器 | 不向外暴露可写访问 |
| BLE 连接与认证 | BLE 适配器 | 发布链路事件 |
| 节点生命周期 | 节点生命周期模块 | 发布状态快照 |
| 配对绑定和 NVS | 对应存储适配器 | 通过端口读写 |

禁止网页、BLE 回调、串口回调、心跳逻辑或硬件驱动直接修改灯效状态、场景版本或另一模块的缓存。

### 2.3 一个硬件输出只能有一个出口

一条 LED 数据线只能由一个 `LedOutput` 适配器调用底层 `show()`。效果代码、通信代码、心跳和网页代码都没有调用 `show()` 的权限。

### 2.4 使用值传递，不跨模块共享可写对象

模块之间传递 `ControlCommand`、`LightingScene`、`LightingFrame`、`LinkEvent`、`CommandReceipt` 和 `StatusSnapshot` 等不可变数据。不得让一个模块保留另一个模块内部数组、驱动对象或可变状态的引用。

### 2.5 接口要深，不要为了“解耦”制造空壳

一个模块应把复杂度收在小接口之后。例如 `LedOutput::present(frame)` 应自行完成映射、尾部清黑和硬件发送，调用者无需知道物理长度或 LED 库。只有确实会变化的地方才建立接口：灯光输出、传输、时钟、存储是长期需要多个适配器的真实接缝。

## 3. 系统模块图

```text
控制输入                                                    硬件输出
Web / Serial / BLE / Wi-Fi / ESP-NOW / Voice                  LED / NVS / BLE / Wi-Fi
          │                                                           ▲
          ▼                                                           │
   输入适配器 ── ControlCommand ──► 协调器 / 节点应用模块 ── 端口接口 ──► 输出适配器
                                           │
                                           ▼
                  LightingScene ──► LightingRenderer ──► LightingFrame
                                           ▲
                                           │
                         AudioFrame / Clock / 节点本地配置
```

数据向内进入，结果向外输出。适配器之间不得直接调用：例如网页不能直接调用 BLE，BLE 回调不能直接调用 `LedOutput`。

## 4. 三个工程的职责

### 4.1 SOZO-Common：可复用核心

`SOZO-Common` 只能包含与具体板卡无关的模块：

- `SozoDomain`：统一术语对应的数据模型、范围校验和不变量。
- `SozoNodeProtocol`：SozoBus 信封、消息、编解码和请求关联。
- `SozoBusCore`：主题、服务和命令投递的通用规则。
- `SozoSceneCore`：权威空间场景、灯光节点运行时和本地输出端口。
- `SozoLightingCore`：颜色计算、灯珠几何、逻辑灯光帧和效果渲染。
- 可在宿主机运行的纯逻辑测试。

它不得依赖 S3 工程、C3 工程、某种 LED 库或 BLE 库。C3 也不得再通过 `../SOZO-ESP32-n8r8/lib` 获取共享代码；共享能力必须先沉到这里。

### 4.2 ESP32-S3：协调器工程

S3 工程包含：

- 协调器应用模块：场景权威、节点登记、节点生命周期、命令授权和场景发布。
- 适配器：网页、串口、Wi-Fi、BLE Central、可选本地灯光节点输出、NVS、板载音频。
- 组合根：只组装对象并逐个调用模块的 `tick()`。

S3 不得包含 C3 的 GPIO、物理长度或 C3 灯效私有状态。

### 4.3 ESP32-C3：扩展节点工程

C3 工程包含：

- 节点应用模块：配对流程、接收场景、版本去重、本地持续渲染、回执和节点本地状态。
- 适配器：BLE Peripheral、C3 LED 输出、GPIO BOOT 键、本地绑定存储。
- 组合根：只把适配器和应用模块接起来。

C3 只消费 `LightingScene`，不拥有或修改协调器的权威空间场景。断开后采用“离线保持”：维持最后成功应用的场景，不伪造新的空间场景。

## 5. 灯光渲染与物理灯珠

灯效必须只对逻辑灯珠计算，物理输出必须每一帧覆盖整条实际灯带。

```text
LightingScene + AudioFrame + Clock
                ↓
        LightingRenderer
                ↓
    LightingFrame（逻辑灯珠）
                ↓
 LedGeometry（逻辑 → 物理映射）
                ↓
 LedOutput::present(frame)
                ↓
    每一颗物理灯珠被写入颜色或黑色
                ↓
             show() 一次
```

`activeLedCount` 与 `physicalLedCount` 是两个不同参数：

- `activeLedCount`：效果计算和空间布局使用的逻辑数量。
- `physicalLedCount`：实际需要写入的物理数量。

示例：C3 物理上有 62 颗，只启用 60 颗时，渲染器只产生 60 个逻辑颜色；输出适配器仍然向 62 颗物理灯珠写入数据，61、62 永远写黑。因此亮度为 0、切换效果、重启或收到新场景时，尾部都不会遗留旧颜色。

禁止用“缩短底层 LED 驱动长度”表示禁用尾部灯珠；这会使未写入的物理灯珠保留历史状态。

## 6. 通信、回执与状态

### 6.1 链路状态

BLE、Wi-Fi 和 ESP-NOW 各自的适配器负责报告连接、认证、断开和重连事件。节点生命周期模块根据这些事件管理“连接中、就绪、离线”等状态。链路状态不依赖 LED 刷新，也不依赖定时应用层心跳才能成立。

### 6.2 命令回执

需要确认的场景或配置命令必须带关联编号。节点在验证、接受并应用后，发送 SozoBus `Ack` 回执；拒绝则发送明确的错误回执。协调器用回执更新“已应用场景版本”，不把“BLE 仍连接”误写成“场景已应用”。

### 6.3 状态快照与健康检测

网页诊断需要内存、版本或本地错误信息时，协调器发起一次 `StatusSnapshot` 请求，节点仅回复当前快照。

连续健康检测是可选策略：未来如果确实需要发现“BLE 连接仍在、但节点应用已经卡死”的少见情况，协调器可以低频发起 `HealthProbe` 并等待回执。它是生命周期模块的独立策略，不进入 `LightingRenderer`、`LedOutput`、BLE 回调或每帧逻辑。

因此，删除定时心跳不会失去状态判断：平时由链路事件和命令回执判断；诊断时按需请求快照；只有需要连续健康监测时才启用独立探测。

## 7. 节点处理流程

### 场景下发

```text
网页等控制来源
  → Input Adapter
  → ControlCommand
  → Coordinator Application（校验、更新权威场景、递增版本）
  → NodeTransport Port
  → BLE / Wi-Fi / ESP-NOW Adapter
  → C3 Node Application（验证版本、应用本地场景、生成回执）
  → LightingRenderer（持续本地渲染）
  → LedOutput（完整物理帧）
```

### 连接变化

```text
BLE 回调 → BLE Adapter → LinkEvent → NodeLifecycle → 节点状态快照
```

这条路径不得穿过场景运行时和灯带输出。

### 按钮、旋钮、传感器上报

```text
GPIO / 传感器 Adapter → NodeInputEvent → Node Application
    → SozoBus Topic / ControlCommand → Coordinator Application
```

实体控制器只发布输入事件，不直接控制另一条灯带。这样同一控制器未来可以改用 BLE、Wi-Fi 或 ESP-NOW 而不改控制语义。

## 8. 现有代码的重构方向

截至 2026-08-08，灯效核心与节点运行时迁移已经落地：`LightingRenderer`、全部灯效算法、`LightingFrame`、`LedGeometry` 与 `LedOutput` 位于 `SozoLightingCore`；权威空间场景和 S3/C3 共用的灯光节点运行时位于 `SozoSceneCore`。S3 与 C3 只保留各自的 NeoPixel 输出适配器，C3 编译配置不再引用 S3 工程。

1. 将目前只在 S3 工程内、却被 C3 引用的领域和灯效核心下沉到 `SOZO-Common`；C3 移除对 S3 `lib` 的编译依赖。
2. 保持 `LightingController` 只接受完整节点状态并驱动 `LightingRenderer` 与 `LedOutput`；它不得接收 `ControlCommand` 或承担场景权威与持久化。颜色计算和效果不能包含 `Adafruit_NeoPixel`、`Arduino.h` 或默认 GPIO 驱动。
3. 建立 `LedGeometry`，明确物理 62 / 逻辑 60 的模型；C3 输出适配器每帧清黑未映射物理灯珠。
4. 删除 `BleLedRenderGuard` 以及“BLE 发送后暂停渲染”的逻辑。通讯与灯带刷新不能用时间阻断相互规避。
5. 将 C3 `main.cpp` 中的绑定、收包分发、心跳和渲染调度移入节点应用模块；`main.cpp` 只创建模块和调用入口。
6. 将 S3 的节点超时判断从“应用层心跳缺失”改为“链路事件 + 命令回执 + 可选按需状态快照”。
7. 在协议中完善场景命令关联和 `Ack` 回执语义，并增加按需 `StatusSnapshot` 服务；不要让状态信息混入音频或渲染消息。

## 9. 可检查的守门规则

每次改动必须通过以下检查：

- 核心模块的宿主机测试不需要真实 ESP32、BLE 或 LED 库。
- `SozoLightingCore` 中不得出现 `NimBLE`、`WiFi`、`Preferences`、`Adafruit_NeoPixel`、`FastLED` 或 GPIO 调用。
- `SozoNodeProtocol` 中不得出现具体传输和硬件库。
- C3 编译配置不得引用 `../SOZO-ESP32-n8r8/lib`。
- 每个 LED 输出适配器有测试证明：所有物理灯珠都会收到写入；未映射物理灯珠为黑；一次呈现只调用一次底层 `show()`。
- 通信测试证明：收到连接事件不渲染灯光；收到场景才改变场景；回执不调用 LED 输出。
- 任何新控制来源必须先产生 `ControlCommand` 或 `NodeInputEvent`，不得直接写 NVS、场景或 LED。

## 10. 新功能接入顺序

新增 BLE 旋钮、Wi-Fi 按钮、传感器、语音或电脑音频时，按以下顺序工作：

1. 在术语与领域模型中确认它是控制来源、扩展节点能力还是音频来源。
2. 定义它发布的命令或事件，不先写硬件回调。
3. 在应用模块中写状态转换和宿主机测试。
4. 新增相应传输或硬件适配器，实现既定接口。
5. 仅在组合根中注册适配器。
6. 做端到端验收：断连、重连、重复消息、过期版本、零亮度和物理尾部灯珠。

这套顺序保证未来增加能力时，改动局部集中、可验证，也不会重新把 S3 本地灯光节点、C3 远程灯光节点和通信逻辑绑在一起。
