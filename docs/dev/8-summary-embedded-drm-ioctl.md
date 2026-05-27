# 内嵌 DRM ioctl Summary

> 文档元数据
> - 文件编号：8
> - 文档类型：summary
> - 文件路径：docs/dev/8-summary-embedded-drm-ioctl.md
> - 文档版本：v1.0.0
> - 最后更新：2026-05-27
> - 需求级别：L3
> - 关联需求：去掉 `libdrm` 依赖，同时必须保留 DRM/KMS 功能。

## 1. 结果

- 新增 `src/kms_drm.c`/`src/kms_drm.h`，用项目内最小 ioctl 封装替代 `libdrm` helper。
- `src/main.c` 从 `drmMode*`/`drmIoctl` 切换到 `kms_*` API。
- 默认 `make` 不再使用 `pkg-config libdrm`、`xf86drm.h`、`xf86drmMode.h` 或 `-ldrm`。
- DRM/KMS 功能保留：资源枚举、connector/encoder/crtc 查询、dumb buffer、framebuffer、mode set、page flip 和 flip event 处理仍走内核 DRM UAPI。

## 2. 依赖结果

`ldd build/tty-ui` 仅显示：

```text
linux-vdso.so.1
libc.so.6
/lib64/ld-linux-x86-64.so.2
```

`readelf -d build/tty-ui` 的直接 `NEEDED` 仅为：

```text
libc.so.6
```

## 3. 边界

- 只内嵌当前程序需要的 legacy KMS/dumb buffer 路径。
- 不实现完整 `libdrm` 能力，例如 atomic KMS、plane/property 枚举、PRIME、syncobj 或厂商专用 DRM helper。
- `src/kms_drm.c` 中 DRM UAPI 结构体布局和 ioctl 编号来自 `/data/source/libdrm/include/drm/`，后续如果扩展 DRM 功能，需要继续对照内核 UAPI。

## 4. 验证

| 命令 | 结果 |
|------|------|
| `make -B` | 通过，默认构建未使用 `libdrm` 编译或链接参数 |
| `make font-atlas` | 通过 |
| `build/tty-ui -h` | 通过 |
| `ldd build/tty-ui` | 通过，无 `libdrm.so.2` |
| `readelf -d build/tty-ui` | 通过，直接依赖仅 `libc.so.6` |
| `git diff --check` | 通过 |

未在本机运行 `build/tty-ui -D /dev/dri/card0`，避免接管当前显示输出；实机显示和交互仍需在目标 TTY/initramfs 环境验证。
