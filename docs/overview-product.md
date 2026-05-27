# tty UI绘制例子 产品概览

> 文档元数据
> - 文档版本：v1.0.0
> - 最后更新：2026-05-27
> - 更新来源：docs/dev/1-task-diskcrypt-kms-ui.md、docs/dev/4-task-embed-font-static-freetype.md、docs/dev/6-summary-drm-dumb-buffer.md、docs/dev/7-summary-embedded-bitmap-font.md

## 1. 产品定位

- 目标用户：需要在 initramfs、真实 TTY 或无桌面环境中展示登录/解锁界面的系统集成方。
- 核心问题：在没有 X11、Wayland 或 GUI compositor 的情况下绘制可交互图形界面。
- 核心价值：用裸 DRM/KMS 图形路径提供比终端字符界面更清晰的登录体验。
- 非目标：不负责真实账号认证、磁盘解密、系统启动编排或桌面 GUI。

## 2. 功能边界

- 核心功能：绘制 DiskCrypt 登录界面；支持用户名输入、密码掩码输入、继续启动和退出按钮。
- 不支持功能：真实凭据校验、PAM 集成、鼠标/触控输入、多语言切换、动画主题系统。
- 关键对象：用户名输入框、密码输入框、继续启动按钮、退出按钮。
- 关键状态：当前焦点、用户名内容、密码内容、用户确认结果。

## 3. 关键场景

| 场景 | 用户目标 | 成功标准 | 异常/边界 |
|------|----------|----------|-----------|
| TTY/initramfs 登录 | 输入用户名和密码并继续启动 | 界面可显示，Tab 可切换焦点，Enter 可确认 | 需要 DRM 设备和 DRM master 权限 |
| 取消登录 | 用户选择退出 | Enter 确认退出按钮后程序返回非 0 | Esc 也会退出 |

## 4. 核心流程

```text
1. 程序打开 DRM 设备并初始化图形输出。
2. 用户通过键盘输入用户名和密码。
3. 用户用 Tab 切换到按钮并用 Enter 确认继续启动或退出。
```

## 5. 产品规则

- 权限规则：运行环境必须允许程序打开 `/dev/dri/card*` 并获得 DRM master。
- 状态流转：用户名 -> 密码 -> 继续启动 -> 退出，Tab 循环切换。
- 异常处理：Esc 或退出按钮返回非 0；显示初始化失败时程序直接退出。
- 兼容约束：目标运行环境不依赖桌面 GUI；位图字形表已打包进二进制，显示路径只需要 DRM/KMS dumb buffer。
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
