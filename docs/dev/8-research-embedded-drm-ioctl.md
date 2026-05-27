# 内嵌 DRM ioctl Research

> 文档元数据
> - 文件编号：8
> - 文档类型：research
> - 文件路径：docs/dev/8-research-embedded-drm-ioctl.md
> - 文档版本：v1.0.0
> - 最后更新：2026-05-27
> - 需求级别：L3
> - 关联需求：去掉 `libdrm` 依赖，同时必须保留 DRM/KMS 功能。

## 1. 背景

当前程序使用 `libdrm` 的 `xf86drm.h`/`xf86drmMode.h` 封装来完成 KMS
资源枚举、connector/encoder/crtc 查询、dumb buffer、framebuffer、page flip
和事件处理。运行时仍直接依赖 `libdrm.so.2`。

## 2. 可行方案

- 不删除 DRM/KMS 功能；继续使用 Linux DRM UAPI 与内核通信。
- 将当前程序实际使用的 `libdrm` helper 替换为项目内最小 ioctl 封装。
- 保留以下能力：
  - `GETRESOURCES` 枚举 CRTC、connector、encoder。
  - `GETCONNECTOR`、`GETENCODER`、`GETCRTC` 查询显示链路。
  - `CREATE_DUMB`、`MAP_DUMB`、`DESTROY_DUMB` 管理 dumb buffer。
  - `ADDFB`、`RMFB`、`SETCRTC`、`PAGE_FLIP` 完成显示和翻页。
  - 读取 `DRM_EVENT_FLIP_COMPLETE` 维持 page flip 等待逻辑。

## 3. 取舍

- 优点：主程序不再依赖 `libdrm.so.2`，默认构建也不需要 `libdrm` 开发包。
- 缺点：项目需要维护一小段 DRM UAPI 结构体和 ioctl 封装；内核 UAPI 变更时需要人工确认兼容性。
- 边界：只实现本项目当前使用的 legacy KMS/dumb buffer 路径，不引入 atomic KMS、plane/property 枚举等完整 libdrm 能力。

## 4. 风险

- DRM UAPI 结构体布局必须与内核一致，否则会导致 ioctl 失败。
- `GETCONNECTOR`/`GETRESOURCES` 是双阶段查询，需处理热插拔导致的数量变化。
- page flip 事件必须检查事件长度，避免错误事件数据造成循环异常。
