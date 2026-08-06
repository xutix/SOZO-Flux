# SOZO Flux 版本规则

根目录 `VERSION` 是发布清单，包含四条相互独立的版本线：

- `platform`：仓库和整体能力集合；
- `gateway_s3`：S3 协调器固件；
- `node_c3`：C3 扩展节点固件；
- `protocol`：节点之间的线协议。

平台和固件使用语义化版本。`0.x` 表示平台仍在形成稳定接口；`alpha` 表示需要开发板和
实机环境验证。Protocol 使用递增整数，只有发生不能由旧接收方安全忽略的线格式或语义变化
时才提升。

发布时必须同步更新 `VERSION`、`SozoVersion.h`、`CHANGELOG.md`，并通过
`tools/check_versions.py`。兼容的 Bug 修复只提升对应固件补丁版本；共享领域或灯效能力变化
提升平台版本；协议破坏性变化同时提升 Protocol 和受影响固件版本。
