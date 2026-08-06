# SOZO Flux Gateway S3

本工程基于 PlatformIO 与 Arduino framework，适配 ESP32-S3-WROOM-1-N8R8（8MB Flash、8MB OPI PSRAM），支持 INMP441 拾音、固定 308 颗 WS2812 灯带、手机网页控制和音乐律动。

## 接线

### INMP441

| INMP441 | ESP32-S3 |
|---|---|
| VDD | 3.3V |
| GND | GND |
| SCK / BCLK | GPIO4 |
| WS / LRCLK | GPIO5 |
| SD / DOUT | GPIO6 |
| L/R | 接 GND |

INMP441 只能使用 3.3V。程序采用 16000Hz、32bit I2S，并固定读取 L/R 接 GND 对应的左时隙，不再提供声道切换。

### 外接 WS2812 灯带

| WS2812 | 连接 |
|---|---|
| DIN | GPIO18，建议串联 220Ω～470Ω 电阻 |
| 5V | 外部 5V 电源正极 |
| GND | 外部电源 GND |

ESP32-S3 GND、INMP441 GND、灯带 GND 和外部电源 GND 必须共地。GPIO48 的板载 WS2812 在程序启动时会被关闭，运行期间不再使用。

## 灯带供电安全

- 不要使用开发板 5V 引脚给大数量灯带供电。
- 使用足够容量的独立 5V 电源。
- 308 颗灯满白、最大亮度的理论电流可能接近 18.5A。
- 建议灯带电源入口并联 1000uF 或更大的电解电容。
- 长灯带建议多点注入电源，使用合适线径并增加保险保护。
- 默认亮度为 `50/255`。

## 编译与烧录

```bash
pio run
pio run --target upload
pio device monitor
```

串口波特率为 115200，默认烧录速度为 921600。如果上传失败，可以按住 `BOOT` 再点击 Upload，或将 `platformio.ini` 中的上传速度改为：

```ini
upload_speed = 460800
```

如果串口无法识别，可能需要安装 FT232 或 CH343 驱动。

## 串口状态与数字控制

打开 115200 波特率串口监视器后，程序启动时会逐项打印：

- 板载 GPIO48 WS2812 已关闭
- 外接 WS2812 灯带状态
- INMP441 I2S 初始化是否成功
- Wi-Fi 路由器连接或配网 AP、WebServer 状态和当前 IP
- Flash、PSRAM 和剩余内存

运行时每 500ms 打印麦克风状态、音频帧数、RMS、输出音量、鼓点脉冲 `Beat`、灯效模式和亮度。

可以在串口监视器中输入 `0` 到网页设置的有效灯珠数量，控制从灯带数据输入端开始亮起多少颗灯。亮灯颜色使用网页当前选择的静态颜色，其余灯珠关闭。

| 输入 | 功能 |
|---|---|
| `0` | 全部关闭 |
| `1` | 亮第 1 颗 |
| `100` | 亮前 100 颗，其余关闭 |
| `300` | 有效数量为 300 时全部点亮 |
| `s` | 打印完整设备状态 |
| `h` | 打印命令帮助 |

输入数字后按发送或回车。串口监视器即使选择“无行尾”，程序也会在约 300ms 后处理完整数字。

## 手机网页控制

### 首次配网或路由器连接失败

1. 设备会创建 Wi-Fi：`SOZO-FLUX-SETUP`
2. 密码：`12345678`
3. 手机或电脑连接该热点后，浏览器打开：<http://192.168.4.1>
4. 选择附近的 2.4GHz 路由器，输入密码后保存
5. 设备会重启并连接路由器；串口会打印局域网 IP

### 局域网控制

电脑、手机和设备连接同一个路由器后，优先在浏览器打开 <http://sozo-flux.local>。如果 Windows 或路由器没有启用 mDNS 解析，再使用串口或路由器客户端列表中的 IP，例如：<http://192.168.1.125>。

如果设备无法连接已保存的路由器，它会在约 30 秒后自动回到 `SOZO-FLUX-SETUP` 配网模式。正常控制页的 **Configure Wi-Fi** 按钮会清除当前凭据并重新进入配网模式。

网页支持：

- 静态颜色，包括任意自定义 RGB 颜色
- 彩虹流动
- 呼吸灯
- 从中心向两侧展开的拾音灯
- 流星灯效
- 拾音颜色样式和流星颜色样式
- 苹果计时器风格的亮度滚轮
- 可触摸的颜色轮盘
- 鼓点灵敏度、冲击强度和拾音参数
- 开机展开动画的颜色与速度
- 网页重启按钮，可重新观看开机动画

不再提供音量柱模式。状态每 500ms 局部刷新。

网页设备状态区只显示当前模式和灯带亮度。选择某个灯效后，只显示该灯效相关的颜色模式和参数：

- 静态与呼吸：显示颜色轮盘。
- 彩虹：不显示颜色参数。
- 拾音：显示拾音颜色样式、鼓点灵敏度、冲击强度；选择静态颜色样式时显示颜色轮盘。
- 流星：显示流星颜色样式；选择静态颜色样式时显示颜色轮盘。

灯珠数量固定为 308，不再提供网页或 API 修改接口。静态、彩虹、呼吸、拾音、流星和开机展开效果均按 308 颗灯计算。

## 断电记忆与开机动画

以下配置会保存到 ESP32-S3 的 NVS：

- 路由器 Wi-Fi 名称和密码
- 上次使用的活动模式
- 亮度
- 静态自定义颜色
- 拾音灯颜色样式
- 流星颜色样式
- 开机动画颜色和速度
- 麦克风增益、噪声门、响应参数和鼓点参数

点击“关灯”不会覆盖上次活动模式；再次点击“开灯”会恢复关闭前的模式。设备重新上电时会先播放一次从灯带中心缓慢向两侧展开的动画，随后恢复上次活动模式。例如断电前处于拾音灯模式，下次启动动画结束后仍进入拾音灯模式。

配置修改后约 1 秒写入 NVS，避免拖动网页控件时频繁擦写 Flash。

## 拾音调试

麦克风诊断数据保留在 115200 串口和 `/api/status` 中。建议播放鼓点明显的音乐，同时观察串口：

- `RMS` 表示声音总体能量。
- `Volume` 是经过噪声门与平滑后的基础音量。
- `Beat` 是快慢能量包络差分得到的瞬态鼓点脉冲。

网页“鼓点灵敏度”越低越容易触发；环境声音复杂、误触发较多时调高。“鼓点冲击强度”控制鼓点发生时灯带额外展开的幅度。

音乐律动模式沿用 ESP32-C3 工程的表现：灯光从有效灯带的中心向两侧对称展开，音量越大展开范围越长。颜色可选择绿红渐变、静态自定义色、黄红渐变、青蓝渐变或紫粉渐变。RMS 计算会先减去采样平均值，以消除 INMP441 的直流偏置。

流星颜色可选择静态自定义色、彩虹、橙色、青蓝或紫色。拾音灯和流星选择“静态自定义颜色”时，共用网页设置的静态颜色。

如果音频帧数一直为 0，说明 I2S 没有收到数据，请检查：

- VDD 是否为 3.3V
- 是否全部共地
- SCK、WS、SD 是否分别连接 GPIO4、GPIO5、GPIO6
- INMP441 的 L/R 引脚状态
- SD 与 GPIO6 之间是否接触可靠

## API

| 路由 | 功能 |
|---|---|
| `GET /api/status` | 状态、左右 RMS、当前音频参数、Wi-Fi 模式、SSID 和 IP |
| `GET /api/wifi/scan` | 仅配网模式可用；扫描附近 Wi-Fi |
| `POST /api/wifi/save` | 仅配网模式可用；提交 `ssid` 与 `password`，保存后重启 |
| `GET /api/wifi/reset` | 仅已连接路由器时可用；清除 Wi-Fi 凭据并重启进入配网模式 |
| `GET /api/mode?value=music` | 设置 `static`、`rainbow`、`breathe`、`music`、`comet`、`off` 或 `on` |
| `GET /api/color?r=255&g=0&b=0` | 设置静态颜色 |
| `GET /api/brightness?value=50` | 设置灯带亮度 |
| `GET /api/effects?audioStyle=0&cometStyle=1` | 设置拾音灯和流星颜色样式，范围 0～4 |
| `GET /api/startup?r=50&g=120&b=220&speed=0.8` | 设置开机展开颜色和速度 |
| `GET /api/audio?gain=1&noise=10000&beatSensitivity=1.45&beatBoost=150` | 调节拾音和鼓点参数 |
| `GET /api/restart` | 重启设备并重新播放开机动画 |

## 空间灯光控制（当前版本）

正常联网后，在电脑或手机的同一局域网中打开 <http://sozo-flux.local>。

- 灯效库提供：常亮、彩虹、流星、拾音灯、呼吸、桌面极光、火焰拾音、玻璃流光、四角脉冲、低频水波和专注。
- 每个灯效只显示并保存其实际使用的参数；拾音灯效固定使用板载 INMP441。
- 电脑音频频谱、电脑音频输入与电脑 FFT 参数暂未实现，也不会出现在网页中。
- “空间布局”页可在连续空间和左／中／右逻辑分段之间切换，可设置总数量、空间中心/三段数量和方向反转。
- 默认活动数量为 308 颗，软件允许配置 1–1024 颗。扩展灯带前必须检查 5V 电源容量、分段供电注入、线材和保险保护；1024 仅是软件上限，不代表推荐的供电规模。
- 布局和灯效参数在停止调整约 1 秒后写入 NVS，设备重启后会恢复。

### 新增 API

| 路由 | 功能 |
|---|---|
| `GET /api/layout` | 获取空间布局与 1024 颗软件上限 |
| `POST /api/layout` | 保存 `profile`、`activeCount`、连续中心或左/中/右数量、`reversed` |
| `POST /api/lighting` | 保存 `effect`、亮度、颜色及该灯效的参数；仅接受 11 种设备灯效 |

`GET /api/status` 还会返回 `effect`、`settings` 和 `layout`，供网页恢复当前状态。旧版 `GET` 控制接口保留兼容，但新的网页只使用上述接口。

## PSRAM 排查

工程默认使用 QIO Flash + OPI PSRAM。如果 PSRAM 显示为 0 或设备反复重启，可先注释：

```ini
board_build.arduino.memory_type = qio_opi
board_build.psram_type = opi
```

## Extensible control architecture

The firmware has one authoritative lighting state and one command path. A
transport adapter must create a `ControlCommand` and send it to
`CommandRouter`; adapters must not write LED state or NVS directly.

| Responsibility | Module |
|---|---|
| Control vocabulary, sources, validation and persisted state | `lib/SozoDomain` |
| Shared LED effects and spatial rendering | `../SOZO-Common/lib/SozoLightingCore` |
| S3 NeoPixel strip output | `lib/SozoLightingAdapter` |
| Microphone sampling and audio frame analysis | `lib/SozoAudio` |
| Command authorization and delayed persistence | `lib/SozoControl` |
| Router Wi-Fi, mDNS and provisioning AP | `lib/SozoNetwork` |
| Phone/computer HTTP interface | `lib/SozoWeb` |
| USB serial interface | `lib/SozoSerial` |
| Startup composition only | `src/main.cpp` |

The planned extension points are deliberately explicit:

- Router-connected self-made ESP32 controllers use a Wi-Fi adapter and
  `ControlSource::WiFiNode`.
- Direct self-made ESP32 controllers use an ESP-NOW adapter and
  `ControlSource::EspNowNode`.
- A self-made BLE button, knob, or sensor uses a BLE adapter and
  `ControlSource::BleNode`; a phone BLE interface uses
  `ControlSource::BlePhone`.
- Voice recognition maps an intent into a `ControlCommand` with
  `ControlSource::Voice`. It does not bypass the command router.
- Computer audio is a future input adapter with `ControlSource::PcAudio`.
  It may provide analyzed audio frames, but the device microphone remains the
  only current audio source exposed by the web interface.

When adding a new adapter, first extend the documented domain types and source
permission rules, then add a small transport module. Keep the web, serial,
BLE, ESP-NOW, voice, and future computer-audio transports independent so that
one source cannot silently overwrite another source's protocol or persistence
logic.

并临时移除 `-DBOARD_HAS_PSRAM`，先验证基础功能，再根据实际模块调整。
