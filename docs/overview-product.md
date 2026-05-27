# tty UI绘制例子 产品概览

> 文档元数据
> - 文档版本：v1.0.0
> - 最后更新：2026-05-27
> - 更新来源：docs/dev/1-task-diskcrypt-kms-ui.md、docs/dev/4-task-embed-font-static-freetype.md、docs/dev/6-summary-drm-dumb-buffer.md、docs/dev/7-summary-embedded-bitmap-font.md、docs/dev/8-summary-embedded-drm-ioctl.md、docs/dev/9-task-static-libc.md、docs/dev/11-task-shutdown-button-escape-input.md

## 1. 产品定位

- 目标用户：需要在 initramfs、真实 TTY 或无桌面环境中展示登录/解锁界面的系统集成方。
- 核心问题：在没有 X11、Wayland 或 GUI compositor 的情况下绘制可交互图形界面。
- 核心价值：用裸 DRM/KMS 图形路径提供比终端字符界面更清晰的登录体验。
- 非目标：不负责真实账号认证、磁盘解密、系统启动编排或桌面 GUI。

## 2. 功能边界

- 核心功能：绘制 DiskCrypt 登录界面；支持用户名输入、密码掩码输入、继续启动和关机按钮。
- 不支持功能：真实凭据校验、PAM 集成、鼠标/触控输入、多语言切换、动画主题系统。
- 关键对象：用户名输入框、密码输入框、继续启动按钮、关机按钮。
- 关键状态：当前焦点、用户名内容、密码内容、用户确认结果。

## 3. 关键场景

| 场景 | 用户目标 | 成功标准 | 异常/边界 |
|------|----------|----------|-----------|
| TTY/initramfs 登录 | 输入用户名和密码并继续启动 | 界面可显示，Tab 可切换焦点，Enter 可确认按钮 | 需要 DRM 设备和 DRM master 权限 |
| 主动关机 | 用户选择关机 | Enter 确认关机按钮后程序恢复显示/终端状态并请求系统关机 | 需要关机权限；Esc 不退出 |

## 4. 核心流程

```text
1. 程序打开 DRM 设备并初始化图形输出。
2. 用户通过键盘输入用户名和密码。
3. 用户用 Tab 切换到按钮并用 Enter 确认继续启动或关机。
```

## 5. 产品规则

- 权限规则：运行环境必须允许程序打开 `/dev/dri/card*` 并获得 DRM master。
- 状态流转：用户名 -> 密码 -> 继续启动 -> 关机，Tab 循环切换。
- 按键规则：只有 Tab 切换焦点；Enter 只确认当前聚焦按钮，不在输入框内切换焦点。
- 异常处理：Esc 不退出；方向键等终端转义序列会被忽略；关机按钮确认后请求系统关机；显示初始化失败时程序直接退出。
- 兼容约束：目标运行环境不依赖桌面 GUI；位图字形表已打包进二进制，显示路径使用 Linux DRM/KMS dumb buffer，不需要 `libdrm.so` 或 `libc.so.6`。
- 用户可见行为：密码只显示掩码字符，不在界面回显明文。

## 6. 非功能要求

- 可用性：界面在无 GUI 环境可直接绘制，文字和控件需保持清晰。
- 安全：不输出密码明文，不在项目文档中记录敏感值。
- 兼容性：默认使用 `/dev/dri/card0`，允许通过参数指定 DRM 设备；中文文字来自内嵌位图字形表。

## 7. 文档索引

- 需求与任务索引：docs/dev/README.md
- 开发概览：docs/overview-product-dev.md
- 关键任务文档：
  - docs/dev/1-task-diskcrypt-kms-ui.md：DiskCrypt KMS 登录界面实现。

## 8. 变更记录

| 日期 | 变更 | 影响 | 关联文档 |
|------|------|------|----------|
| 2026-05-27 | 新增裸 KMS DiskCrypt 登录界面产品边界 | 确立当前仓库主功能 | docs/dev/1-task-diskcrypt-kms-ui.md |
| 2026-05-27 | 内嵌默认中文字体并移除外部字体参数 | 降低目标运行环境字体依赖 | docs/dev/4-task-embed-font-static-freetype.md |
| 2026-05-27 | 切换为 DRM dumb buffer + CPU 绘制 | 去掉 GBM/EGL/GLES 运行依赖 | docs/dev/6-summary-drm-dumb-buffer.md |
| 2026-05-27 | 使用内嵌位图字形表显示文字 | 默认构建不再依赖 `libfreetype.a` | docs/dev/7-summary-embedded-bitmap-font.md |
| 2026-05-27 | 使用项目内 DRM ioctl 封装 | 去掉 `libdrm.so` 运行依赖并保留 DRM/KMS 功能 | docs/dev/8-summary-embedded-drm-ioctl.md |
| 2026-05-27 | 默认静态链接 libc | 去掉 `libc.so.6` 运行依赖 | docs/dev/9-task-static-libc.md |
| 2026-05-27 | Esc 不再退出，退出按钮改为关机 | 避免方向键误触退出，并提供显式关机路径 | docs/dev/11-task-shutdown-button-escape-input.md |
