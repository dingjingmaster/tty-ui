# 需求与任务索引

> 记录 L1+ 新增任务、问题、调研、计划、总结或评审文档。新需求/问题优先读取相关 Summary 或轻量任务记录，避免加载完整历史上下文。

## 编号规则

- 文件编号使用从 `1` 开始递增的正整数，不要求固定位数。
- 编号全局只在 `docs/dev/` 下递增，不按类型分别编号。
- 新增任务、问题修复、调研、计划、总结、评审或需求变更文档前，先检查本索引和 `docs/dev/` 现有文件名，取最大编号 + 1。
- 同一需求的多份文档使用同一编号，例如 `2-research-xxx.md`、`2-plan-xxx.md`、`2-summary-xxx.md`。
- 编号一旦分配不得复用；取消、废弃、拆分、合并也要在索引中保留记录并标注状态。
- 文件命名格式：`N-[type]-[slug].md`，其中 `type` 可取 `summary`、`task`、`fix`、`research`、`plan`、`review`。

## 索引

| ID | 日期 | 级别 | 类型 | 文档 | 状态 | 摘要 |
|----|------|------|------|------|------|------|
| 1 | 2026-05-27 | L2 | task | [1-task-diskcrypt-kms-ui.md](1-task-diskcrypt-kms-ui.md) | 已完成 | 基于 kmscube 技术栈实现无 GUI 环境 DiskCrypt 登录界面。 |
| 2 | 2026-05-27 | L2 | task | [2-task-align-exit-button.md](2-task-align-exit-button.md) | 已完成 | 调整按钮布局：继续启动保持原位，退出按钮右对齐。 |
| 3 | 2026-05-27 | L2 | task | [3-task-remove-header-accent.md](3-task-remove-header-accent.md) | 已完成 | 移除“安得合众”下方的蓝色装饰横线。 |
| 4 | 2026-05-27 | L2 | task | [4-task-embed-font-static-freetype.md](4-task-embed-font-static-freetype.md) | 已完成 | 内嵌 `fonts/wqy-microhei.ttc`，移除 `-f`/Fontconfig，并静态链接 FreeType。 |
| 5 | 2026-05-27 | L2 | task | [5-task-trim-runtime-deps.md](5-task-trim-runtime-deps.md) | 已完成 | 裁剪 FreeType 可选压缩/PNG 动态依赖。 |
| 6 | 2026-05-27 | L3 | research | [6-research-drm-dumb-buffer.md](6-research-drm-dumb-buffer.md) | 已完成 | 调研切换到 DRM dumb buffer + CPU 软件绘制。 |
| 6 | 2026-05-27 | L3 | plan | [6-plan-drm-dumb-buffer.md](6-plan-drm-dumb-buffer.md) | 已完成 | 规划替换 GBM/EGL/GLES 渲染层并收敛依赖。 |
| 6 | 2026-05-27 | L3 | summary | [6-summary-drm-dumb-buffer.md](6-summary-drm-dumb-buffer.md) | 已完成 | 总结 DRM dumb buffer + CPU 绘制架构变更。 |
| 7 | 2026-05-27 | L3 | research | [7-research-embedded-bitmap-font.md](7-research-embedded-bitmap-font.md) | 已完成 | 调研用内嵌位图字形替代 FreeType。 |
| 7 | 2026-05-27 | L3 | plan | [7-plan-embedded-bitmap-font.md](7-plan-embedded-bitmap-font.md) | 已完成 | 规划移除默认构建对 `libfreetype.a` 的依赖。 |
| 7 | 2026-05-27 | L3 | summary | [7-summary-embedded-bitmap-font.md](7-summary-embedded-bitmap-font.md) | 已完成 | 总结内嵌位图字形并移除 FreeType 构建依赖。 |
| 8 | 2026-05-27 | L3 | research | [8-research-embedded-drm-ioctl.md](8-research-embedded-drm-ioctl.md) | 已完成 | 调研用项目内 DRM ioctl 封装替代 `libdrm`。 |
| 8 | 2026-05-27 | L3 | plan | [8-plan-embedded-drm-ioctl.md](8-plan-embedded-drm-ioctl.md) | 已完成 | 规划移除 `libdrm` 运行和默认构建依赖。 |
| 8 | 2026-05-27 | L3 | summary | [8-summary-embedded-drm-ioctl.md](8-summary-embedded-drm-ioctl.md) | 已完成 | 总结内嵌 DRM ioctl 封装并移除 `libdrm` 依赖。 |
| 9 | 2026-05-27 | L2 | task | [9-task-static-libc.md](9-task-static-libc.md) | 已完成 | 默认静态链接 libc，去掉 `libc.so.6` 运行依赖。 |
