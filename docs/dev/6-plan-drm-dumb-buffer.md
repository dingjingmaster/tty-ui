# DRM dumb buffer CPU 绘制 Plan

> 文档元数据
> - 文件编号：6
> - 文档类型：plan
> - 文件路径：docs/dev/6-plan-drm-dumb-buffer.md
> - 文档版本：v1.0.0
> - 最后更新：2026-05-27
> - 需求级别：L3
> - 关联需求：改成 DRM dumb buffer + CPU 软件绘制。

## 1. 修改范围

- `src/main.c`：替换 GBM/EGL/GLES 渲染层为 DRM dumb buffer + CPU 绘制。
- `Makefile`：依赖从 `libdrm gbm egl glesv2` 收敛为 `libdrm`。
- `README.md`、`docs/overview-product-dev.md`：更新技术栈和依赖说明。
- `docs/dev/README.md`、本任务文档和 Summary。

## 2. 执行计划

| 步骤 | 修改内容 | 验证方式 | 状态 |
|------|----------|----------|------|
| 1 | 创建 Research/Plan 文档和索引 | 人工检查编号与边界 | 完成 |
| 2 | 重写 `src/main.c` 为 dumb buffer + CPU 绘制 | `make -B` | 完成 |
| 3 | 调整 Makefile 去掉 GBM/EGL/GLES | `ldd build/tty-ui`、`readelf -d build/tty-ui` | 完成 |
| 4 | 更新 README/开发总览 | 人工检查 | 完成 |
| 5 | 验证并提交 | `build/tty-ui -h`、`git diff --check`、`git status --short` | 完成 |

## 3. 风险门禁

| 项 | 结论 |
|----|------|
| 风险矩阵 | L3：架构和运行依赖变化，涉及 DRM 设备 I/O、C 代码和链接逻辑。 |
| 高风险开发门禁 | 是：C 逻辑、DRM/KMS、内存映射、构建链接。 |
| 破坏性操作 | 否。 |
| 用户已有修改 | 否。 |
| 命令权限 | C0/C1；提交阶段限定文件暂存。 |
| 回滚/止损方式 | 回退本次提交即可。 |

## 4. 验证标准

- `make -B` 构建通过。
- `build/tty-ui -h` 正常输出帮助，不初始化 DRM。
- `ldd build/tty-ui` 不再出现 `libgbm`、`libEGL`、`libGLESv2`、`libGLdispatch`。
- `readelf -d build/tty-ui` 直接依赖仅保留 `libdrm` 和 `libc`。
- `git diff --check` 通过。

## 5. 审视

- 安全：错误路径释放 dumb buffer、framebuffer、mmap 和旧 CRTC。
- 产品：布局和键盘交互保持一致。
- 架构：不再依赖 GPU shader pipeline。
