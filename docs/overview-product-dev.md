# tty UI绘制例子 开发概览

> 文档元数据
> - 文档版本：v1.0.0
> - 最后更新：2026-05-27
> - 更新来源：docs/dev/1-task-diskcrypt-kms-ui.md、docs/dev/4-task-embed-font-static-freetype.md、docs/dev/5-task-trim-runtime-deps.md、docs/dev/6-summary-drm-dumb-buffer.md、docs/dev/7-summary-embedded-bitmap-font.md、docs/dev/8-summary-embedded-drm-ioctl.md、docs/dev/9-task-static-libc.md
> - 关联产品文档：docs/overview-product.md

## 1. 技术栈

| 类别 | 技术/版本 | 用途 | 备注 |
|------|-----------|------|------|
| 语言 | C11 | 主程序实现 | `src/main.c` |
| 构建系统 | Make | 本地构建 | 产物为 `build/tty-ui`；默认 `STATIC=1` 静态链接 libc；`make font-atlas` 开发期目标使用 pkg-config |
| 运行平台 | Linux DRM/KMS | 无 GUI 图形输出 | 需要 `/dev/dri/card*` 与 DRM master |
| 内嵌封装 | `src/kms_drm.c`、`src/kms_drm.h` | DRM/KMS 模式设置、dumb buffer、page flip | 直接调用 Linux DRM ioctl，不链接 `libdrm` |
| 内嵌资源 | `src/font_atlas.c`、`src/font_atlas.h` | 中文/英文位图字形表 | 默认构建直接编译进 `build/tty-ui` |
| 开发工具 | `tools/generate_font_atlas.c` | 从 `fonts/wqy-microhei.ttc` 重新生成字形表 | 仅运行 `make font-atlas` 时需要 `freetype2` |
| 运行依赖 | 无外部共享库 | 主程序静态链接 libc | 仍需要 Linux 内核、DRM 设备和 DRM master 权限 |

## 2. 架构边界

- 模块划分：当前主程序按 DRM/KMS、dumb buffer、CPU 绘制、位图字形绘制、键盘输入分区；`src/kms_drm.c` 提供项目内最小 DRM ioctl 封装。
- 进程/线程/内核边界：单进程单线程；通过 Linux DRM ioctl 调用内核 DRM/KMS；不创建后台线程。
- 客户端/服务端/驱动边界：程序作为 DRM client 直接提交 CRTC mode set 和 page flip。
- 数据流：键盘输入更新内存中的 UI 状态；UI 状态由 CPU 写入 mmap 后的 XRGB8888 dumb buffer；buffer 交给 KMS 显示。
- 控制流：初始化 DRM/KMS 和 dumb buffer -> CPU 渲染首帧 -> 等待键盘事件 -> 状态变化后重绘并 page flip -> 退出时尝试恢复旧 CRTC。
- 外部依赖：DRM 设备；位图字形表已打包进二进制。

## 3. 关键接口

| 接口/协议/ABI | 调用方 | 提供方 | 兼容约束 | 说明 |
|---------------|--------|--------|----------|------|
| `-D <device>` | 用户/启动脚本 | `build/tty-ui` | 默认 `/dev/dri/card0` | 指定 DRM 设备 |
| DRM/KMS | 程序 | Linux 内核 DRM | 需 DRM master 权限 | connector/mode 选择、CRTC 设置、page flip |

## 4. 数据与配置

- 核心数据结构：`display` 管理 DRM/KMS 和 dumb buffer；`ui_state` 管理用户名、密码和焦点；`font_glyph` 描述内嵌位图字形。
- 配置文件/参数：无配置文件；命令行参数为 `-D`。
- 持久化数据：无。
- 迁移/兼容规则：无历史数据迁移。
- 敏感信息处理：密码只保存在进程内存中并以 `*` 掩码显示；程序不打印密码。

## 5. 高风险区域

| 风险区域 | 关注点 | 验证方式 | 关联文档 |
|----------|--------|----------|----------|
| DRM UAPI | ioctl 结构体布局、双阶段资源查询、page flip 事件处理 | 构建检查、人工审查、依赖检查 | docs/dev/8-summary-embedded-drm-ioctl.md |
| 内存/生命周期 | DRM FB、dumb buffer mmap、字形 bitmap 边界检查 | 构建检查、人工审查错误路径 | docs/dev/7-summary-embedded-bitmap-font.md |
| 权限/系统调用 | 打开 DRM 设备、设置 CRTC、page flip | 不在桌面会话实机运行；README 说明运行前提 | docs/dev/1-task-diskcrypt-kms-ui.md |
| 构建链接 | 静态 libc、内嵌 DRM ioctl 封装、内嵌位图字形表、可选开发期字形生成器 | `make`、`file build/tty-ui`、`readelf -d build/tty-ui` | docs/dev/9-task-static-libc.md |

## 6. 构建与验证

- 构建命令：`make`
- 静态链接：默认 `STATIC=1`，主程序链接参数为 `-static -no-pie`；可用 `make STATIC=0` 构建动态链接调试版本。
- DRM/KMS 封装：`make` 直接编译 `src/kms_drm.c`，默认构建不依赖 `libdrm` 头文件、`pkg-config libdrm` 或 `-ldrm`。
- 字形打包：`make` 直接编译已生成的 `src/font_atlas.c`，默认构建不依赖 `freetype2` 或 `libfreetype.a`。
- 字形生成：更换字体、文案或字号时运行 `make font-atlas`，该开发期目标使用 `tools/generate_font_atlas.c` 和 `freetype2` 重新生成 `src/font_atlas.c`/`src/font_atlas.h`。
- 单元测试：暂无。
- 集成验证：需在真实 TTY/initramfs 或可获取 DRM master 的测试机运行 `build/tty-ui -D /dev/dri/card0`。
- 静态检查：当前使用 `git diff --check` 做补丁格式检查。
- 高风险验证：本地仅做构建和人工审查，不执行会切换真实显示输出的程序运行。
- 最小人工验证步骤：在目标 TTY 运行程序，确认界面显示、Tab 切换、Enter 确认、Esc 退出。

## 7. 发布与回滚

- 产物：`build/tty-ui`
- 安装/部署方式：当前未提供安装目标；集成方可复制二进制到 initramfs，位图字形表已随二进制内嵌。
- 配置变更：无。
- 升级步骤：重新构建并替换二进制。
- 回滚步骤：回退代码或替换回旧二进制。
- 止损条件：程序无法获得 DRM master 或显示初始化失败时退出。

## 8. 观测与排障

- 关键日志：初始化失败、DRM dumb buffer/KMS 错误、输入读取错误会输出到 stderr。
- 指标/告警：无。
- 常见故障：无 DRM 权限、无 connected connector、驱动不支持 dumb buffer、无法获得 DRM master。
- 排障入口：先确认 `build/tty-ui -h` 可用，再在目标 TTY 检查 `/dev/dri/card*` 权限。

## 9. 文档索引

- 需求与任务索引：docs/dev/README.md
- 产品概览：docs/overview-product.md
- 按需片段模板：.dj-agent/fragments/
- 关键任务文档：
  - docs/dev/1-task-diskcrypt-kms-ui.md：DiskCrypt KMS 登录界面实现。

## 10. 变更记录

| 日期 | 变更 | 影响 | 关联文档 |
|------|------|------|----------|
| 2026-05-27 | 新增裸 KMS UI 技术栈、构建和验证约定 | 确立项目当前实现架构 | docs/dev/1-task-diskcrypt-kms-ui.md |
| 2026-05-27 | 内嵌默认字体，移除 Fontconfig/`-f`，优先静态链接 FreeType | 降低 initramfs 运行时依赖 | docs/dev/4-task-embed-font-static-freetype.md |
| 2026-05-27 | 禁用 FreeType 可选压缩/PNG 路径 | 移除 `libbz2`、`libpng16`、`libz`、`libbrotli*` 运行时依赖 | docs/dev/5-task-trim-runtime-deps.md |
| 2026-05-27 | 从 GBM/EGL/GLES 切换到 DRM dumb buffer + CPU 绘制 | 运行依赖收敛到 `libdrm` 和 `libc` | docs/dev/6-summary-drm-dumb-buffer.md |
| 2026-05-27 | 改为内嵌位图字形表 | 默认构建和主程序不再依赖 `libfreetype.a` | docs/dev/7-summary-embedded-bitmap-font.md |
| 2026-05-27 | 改为项目内 DRM ioctl 封装 | 主程序不再依赖 `libdrm.so.2`，直接动态依赖仅 `libc` | docs/dev/8-summary-embedded-drm-ioctl.md |
| 2026-05-27 | 默认静态链接 libc | 主程序不再依赖 `libc.so.6` 或动态加载器 | docs/dev/9-task-static-libc.md |
