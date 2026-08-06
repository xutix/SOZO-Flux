# SOZO Flux Common

SOZO Flux Gateway 与扩展节点共用的平台无关领域、灯效、协议和总线库。

当前包含：

- `SozoDomain`：灯效状态、控制命令和平台无关的领域约束。
- `SpatialLightCore`：灯带布局及数量归一化。
- `SozoLightingCore`：共享 `LightingController`、全部灯效算法、逻辑帧、几何映射和 `LedOutput` 边界。
- `SozoNodeProtocol`：固定小端二进制封装、CRC16、节点能力、心跳、场景、音频特征和绑定消息。
- `SozoBusCore`：固定容量发布/订阅，以及不阻塞主循环的异步请求/响应关联与超时。

场景消息描述灯效呈现意图，不保存节点 GPIO 或物理灯珠数量。音频灯效传输标准化特征，不传输 PCM 或逐灯珠 RGB 帧。

S3 与 C3 固件都只依赖这里的同一套灯效核心；板卡工程各自实现 NeoPixel `LedOutput`，Common 不包含 Arduino、GPIO 或具体 LED 库。运行 `tools/verify-module-dependencies.ps1` 可检查这条依赖边界。
