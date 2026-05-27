# tty UI绘制例子 开发概览

> 文档元数据
> - 文档版本：v1.0.0
> - 最后更新：2026-05-27
> - 更新来源：docs/dev/1-task-diskcrypt-kms-ui.md、docs/dev/4-task-embed-font-static-freetype.md
> - 关联产品文档：docs/overview-product.md

## 1. 技术栈

| 类别 | 技术/版本 | 用途 | 备注 |
|------|-----------|------|------|
| 语言 | C11 | 主程序实现 | `src/main.c` |
| 构建系统 | Make + pkg-config | 本地构建 | 产物为 `build/tty-ui` |
| 运行平台 | Linux DRM/KMS | 无 GUI 图形输出 | 需要 `/dev/dri/card*` 与 DRM master |
| 关键依赖 | libdrm、GBM、EGL、GLESv2 | 裸显示链路和 GPU 绘制 | 与 kmscube 技术栈一致 |
| 关键依赖 | FreeType | 中文/英文文字栅格化 | 构建优先链接 `libfreetype.a`，运行时不依赖 `libfreetype.so` |
| 内嵌资源 | `fonts/wqy-microhei.ttc` | 默认中文字体 | 通过 linker binary object 打包进 `build/tty-ui` |

## 2. 架构边界

- 模块划分：当前为单文件最小实现，内部按 DRM、GBM、EGL、GLES 绘制、字体渲染、键盘输入分区。
- 进程/线程/内核边界：单进程单线程；通过 libdrm 调用内核 DRM/KMS；不创建后台线程。
- 客户端/服务端/驱动边界：程序作为 DRM client 直接提交 CRTC mode set 和 page flip。
- 数据流：键盘输入更新内存中的 UI 状态；UI 状态被渲染为 GLES 图形；GBM buffer 交给 KMS 显示。
- 控制流：初始化 DRM/GBM/EGL -> 渲染首帧 -> 等待键盘事件 -> 状态变化后重绘并 page flip -> 退出时尝试恢复旧 CRTC。
- 外部依赖：DRM 设备、EGL/GLES 驱动；字体已打包进二进制。

## 3. 关键接口

| 接口/协议/ABI | 调用方 | 提供方 | 兼容约束 | 说明 |
|---------------|--------|--------|----------|------|
| `-D <device>` | 用户/启动脚本 | `build/tty-ui` | 默认 `/dev/dri/card0` | 指定 DRM 设备 |
| DRM/KMS | 程序 | Linux 内核 DRM | 需 DRM master 权限 | connector/mode 选择、CRTC 设置、page flip |

## 4. 数据与配置

- 核心数据结构：`display` 管理 DRM/GBM/EGL/GLES 资源；`ui_state` 管理用户名、密码和焦点。
- 配置文件/参数：无配置文件；命令行参数为 `-D`。
- 持久化数据：无。
- 迁移/兼容规则：无历史数据迁移。
- 敏感信息处理：密码只保存在进程内存中并以 `*` 掩码显示；程序不打印密码。

## 5. 高风险区域

| 风险区域 | 关注点 | 验证方式 | 关联文档 |
|----------|--------|----------|----------|
| 内存/生命周期 | DRM FB、GBM BO、EGL context、FreeType face 的释放顺序 | 构建检查、人工审查错误路径 | docs/dev/1-task-diskcrypt-kms-ui.md |
| 权限/系统调用 | 打开 DRM 设备、设置 CRTC、page flip | 不在桌面会话实机运行；README 说明运行前提 | docs/dev/1-task-diskcrypt-kms-ui.md |
| 构建链接 | pkg-config 依赖、内嵌字体对象、FreeType 静态链接参数 | `make`、`ldd build/tty-ui` | docs/dev/4-task-embed-font-static-freetype.md |

## 6. 构建与验证

- 构建命令：`make`
- 字体打包：`make` 使用 `ld -r -b binary` 将 `fonts/wqy-microhei.ttc` 转为目标文件并链接进程序。
- FreeType 链接：优先链接 `pkg-config --variable=libdir freetype2` 下的 `libfreetype.a`；如果构建机缺少静态库则回退到 `pkg-config --libs freetype2`。
- 单元测试：暂无。
- 集成验证：需在真实 TTY/initramfs 或可获取 DRM master 的测试机运行 `build/tty-ui -D /dev/dri/card0`。
- 静态检查：当前使用 `git diff --check` 做补丁格式检查。
- 高风险验证：本地仅做构建和人工审查，不执行会切换真实显示输出的程序运行。
- 最小人工验证步骤：在目标 TTY 运行程序，确认界面显示、Tab 切换、Enter 确认、Esc 退出。

## 7. 发布与回滚

- 产物：`build/tty-ui`
- 安装/部署方式：当前未提供安装目标；集成方可复制二进制到 initramfs，字体已随二进制内嵌。
- 配置变更：无。
- 升级步骤：重新构建并替换二进制。
- 回滚步骤：回退代码或替换回旧二进制。
- 止损条件：程序无法获得 DRM master 或显示初始化失败时退出。

## 8. 观测与排障

- 关键日志：初始化失败、DRM/GBM/EGL 错误、输入读取错误会输出到 stderr。
- 指标/告警：无。
- 常见故障：无 DRM 权限、无 connected connector、EGL 无法绑定 GBM。
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
