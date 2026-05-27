# DRM dumb buffer CPU 绘制 Summary

> 文档元数据
> - 文件编号：6
> - 文档类型：summary
> - 文件路径：docs/dev/6-summary-drm-dumb-buffer.md
> - 文档版本：v1.0.0
> - 最后更新：2026-05-27
> - 需求级别：L3
> - 关联需求：改成 DRM dumb buffer + CPU 软件绘制。

## 1. 实现结果

- `src/main.c` 已从 GBM/EGL/GLES 渲染层切换为 DRM dumb buffer + CPU 软件绘制。
- 程序创建两个 XRGB8888 dumb buffer，mmap 后由 CPU 绘制背景、边框、输入框、按钮和文字。
- 文字仍使用内嵌 `fonts/wqy-microhei.ttc` 和静态 FreeType 栅格化。
- 键盘交互和布局保持原逻辑：Tab 切换，Enter 确认，Backspace 删除，Esc 退出。
- `Makefile` 的 pkg-config 依赖从 `libdrm gbm egl glesv2` 收敛为 `libdrm`。

## 2. 验证记录

| 验证项 | 命令/步骤 | 结果 | 备注 |
|--------|-----------|------|------|
| 构建验证 | `make -B` | 通过 | 强制重构，无编译/链接错误。 |
| 帮助路径 | `build/tty-ui -h` | 通过 | 不初始化 DRM。 |
| 动态依赖检查 | `ldd build/tty-ui` | 通过 | 仅显示 `libdrm.so.2`、`libc.so.6` 和基础 ELF 运行项。 |
| 直接依赖检查 | `readelf -d build/tty-ui \| rg "NEEDED"` | 通过 | 直接 NEEDED 仅 `libdrm.so.2`、`libc.so.6`。 |
| Diff 检查 | `git diff --check` | 通过 | 未发现空白或补丁格式问题。 |

## 3. 未执行验证

- 未在真实 TTY/initramfs 上运行 `build/tty-ui -D /dev/dri/card0`，避免在当前环境切换真实显示输出。

## 4. 残余风险

- 目标 DRM driver 必须支持 dumb buffer。
- CPU 绘制不具备 GLES shader/纹理管线能力，后续复杂动画或高频刷新需重新评估性能。
- 如果未来目标是只保留 `libc`，还需要把 libdrm API 替换为直接 ioctl 封装。

## 5. 审视结论

- 安全：未执行破坏性操作；错误路径包含 framebuffer、mmap、dumb buffer、旧 CRTC 和 FreeType 资源释放。
- 产品：用户流程和界面布局保持一致。
- 架构：成功移除 GBM/EGL/GLES/Mesa 运行依赖，符合减少 initramfs 依赖目标。
- 工程：`make -B`、`ldd`、`readelf` 和 `git diff --check` 覆盖了构建、链接和依赖目标；实机显示验证仍需在目标 TTY/initramfs 执行。
