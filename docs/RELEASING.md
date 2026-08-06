# SOZO Flux 发布流程

1. 从干净的 `main` 创建 `release/vX.Y.Z` 分支。
2. 更新 `VERSION`、`SozoVersion.h`、`CHANGELOG.md` 和兼容性说明。
3. 运行全部 CI，并完成 S3/C3 构建。
4. 在目标硬件上记录板型、供电、灯珠数、配网、节点连接和主要灯效结果。
5. 通过 Pull Request 合并到受保护的 `main`。
6. 在合并提交创建签名或 annotated tag：`vX.Y.Z`。
7. 创建 GitHub Release，附带变更、已知限制、Protocol 版本和固件构建产物。
8. 验证 Release 页面、下载产物和仓库徽章。

Alpha Release 可以标记为 prerelease，但不能跳过版本一致性、CI 或基本实机记录。
