# SOZO Flux 灯光架构

## 唯一控制方向

所有灯光来源只能按以下方向调用：

```text
Web / Serial / Voice / Dock
            ↓
LightingControlApplication
            ↓
LightingSceneOrchestrator（唯一目标状态）
            ↓
SceneDeliveryCoordinator
            ↓
S3 本地输出 / C3 Fleet transport
```

- 不允许恢复 `CommandRouter -> SpaceSceneCoordinator` 旧链。
- 不允许在 `main.cpp` 用 revision 比较同步两套灯光状态。
- Web、串口和未来语音只能调用 `LightingControlApplication`，不得直接写灯带、节点投递或 NVS。
- 场景发布只更新场景列出的灯带，其他灯带保持最后目标状态；单灯带发布只更新该目标。

## 数据生命周期

- `LightingScene`：可发布给 S3/C3 的灯效、颜色、亮度和动画参数。
- `LocalLightConfiguration`：节点私有的灯珠数量、物理布局、方向和启动动画。
- `AudioTuning`：Flux Hub 私有的麦克风输入调参。

场景和远程 C3 的灯效请求不得使用 `PersistedLightingState` 作为草稿，否则会把 S3 私有配置带入远程目标。

## 模块边界

- `SozoControl` 是应用层，只依赖领域类型和持久化端口，不依赖 Arduino、Preferences 或具体 NVS store。
- `SozoSettings` 实现持久化端口。
- `SozoNetwork` 只依赖 `NetworkCredentialRepository`；Wi-Fi NVS 由独立的 `WiFiCredentialStore` 实现，不得重新依赖综合设置类。
- `SozoSceneCore` 是纯领域模块，不依赖 `SozoNodeProtocol`；协议枚举和报文只能在 C3/S3 节点适配层转换。
- `SozoWeb` 是 HTTP adapter。场景成员保留、场景输出更新等规则必须位于应用层。
- `NodeFirmwareWebController` 独占 C3 固件上传缓冲、镜像校验、SHA 和上传生命周期。
- 节点页面只管理名称、布局、灯珠数量、启动配置和固件；灯效控制只在灯光控制页面。
- 页面场景草稿统一由 `SceneEditorState` 管理，轮询和 DOM 渲染不得另建平行选择状态。

## 自动守门

修改架构后必须执行：

```powershell
./tools/verify-module-dependencies.ps1
```

正式构建仍必须从本仓库显式执行 `platformio run --project-dir SOZO-ESP32-S3` 和
`platformio run --project-dir SOZO-ESP32-C3`。
