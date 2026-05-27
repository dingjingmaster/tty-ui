# DRM dumb buffer CPU 绘制 Research

> 文档元数据
> - 文件编号：6
> - 文档类型：research
> - 文件路径：docs/dev/6-research-drm-dumb-buffer.md
> - 文档版本：v1.0.0
> - 最后更新：2026-05-27
> - 需求级别：L3
> - 关联需求：改成 DRM dumb buffer + CPU 软件绘制。

## 1. 背景

当前实现使用 DRM/KMS + GBM + EGL + GLESv2。该路线与 kmscube 一致，但会保留 `libgbm`、`libEGL`、`libGLESv2` 及其传递依赖。用户目标是进一步减少 initramfs/TTY 环境依赖。

## 2. 推荐方案

- 使用 DRM/KMS 模式设置与 page flip 保留裸显示能力。
- 使用 DRM dumb buffer 创建可 mmap 的 XRGB8888 framebuffer。
- CPU 直接写入 framebuffer，绘制矩形、边框、按钮、输入框和 FreeType 文本 alpha blending。
- 保留内嵌 `fonts/wqy-microhei.ttc` 与静态 FreeType。

## 3. 取舍

- 优点：去掉 GBM/EGL/GLES/Mesa 运行时依赖，依赖显著减少。
- 缺点：不再使用 GPU/OpenGL；复杂图形、动画和 shader 能力不再可用。
- 当前界面是纯 2D 表单，CPU 绘制足够。

## 4. 风险与边界

- 仍需要 DRM/KMS 设备和 DRM master 权限。
- dumb buffer 支持依赖 DRM driver；大多数 KMS driver 支持，但仍需目标设备实测。
- 不运行实机会接管显示输出的程序，只做构建、帮助路径和动态依赖验证。
- 保持当前键盘交互和布局，不接入认证/解密流程。

## 5. 审视

- 安全：不执行破坏性操作；不运行 KMS 输出。
- 产品：满足减少依赖目标，不改变用户可见流程。
- 架构：从 GPU 渲染改为 CPU framebuffer 渲染，是有意架构变更。
