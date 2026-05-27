# 内嵌 DRM ioctl Plan

> 文档元数据
> - 文件编号：8
> - 文档类型：plan
> - 文件路径：docs/dev/8-plan-embedded-drm-ioctl.md
> - 文档版本：v1.0.0
> - 最后更新：2026-05-27
> - 需求级别：L3
> - 关联需求：去掉 `libdrm` 依赖，同时必须保留 DRM/KMS 功能。

## 1. 修改范围

- `src/kms_drm.c`、`src/kms_drm.h`：新增最小 DRM/KMS ioctl 封装。
- `src/main.c`：从 `libdrm` API 切换到项目内 `kms_*` API。
- `Makefile`：默认构建移除 `pkg-config libdrm`、`-ldrm` 和 libdrm 头文件依赖。
- `README.md`、`docs/overview-product.md`、`docs/overview-product-dev.md`：更新依赖与架构说明。

## 2. 执行计划

| 步骤 | 修改内容 | 验证方式 | 状态 |
|------|----------|----------|------|
| 1 | 创建 Research/Plan 文档和索引 | 人工检查编号与边界 | 已完成 |
| 2 | 新增最小 DRM ioctl 封装 | `make -B` | 已完成 |
| 3 | 改主程序和 Makefile 去掉 libdrm | `make -B`、`build/tty-ui -h` | 已完成 |
| 4 | 更新 README/总文档/summary | 人工检查 | 已完成 |
| 5 | 验证并提交 | `ldd`、`readelf`、`git diff --check` | 已完成 |

## 3. 风险门禁

| 项 | 结论 |
|----|------|
| 风险矩阵 | L3：显示底层依赖和 DRM/KMS 封装变化。 |
| 高风险开发门禁 | 是：C 结构体 ABI、ioctl、显示恢复路径。 |
| 破坏性操作 | 否。 |
| 用户已有修改 | 否。 |
| 命令权限 | C0/C1；提交阶段限定文件暂存。 |
| 回滚/止损方式 | 回退本次提交即可恢复 `libdrm` helper。 |

## 4. 验证标准

- `make -B` 构建通过，默认构建不使用 `pkg-config libdrm` 或 `-ldrm`。
- `build/tty-ui -h` 正常输出帮助。
- `ldd build/tty-ui` 不再显示 `libdrm.so.2`。
- `readelf -d build/tty-ui` 直接 `NEEDED` 仅剩 `libc.so.6`。
- `git diff --check` 通过。

## 5. 审视

- 安全：ioctl 输出数组分配必须检查溢出和空指针。
- 产品：DRM/KMS 显示、dumb buffer、page flip 功能必须保留。
- 架构：仅内嵌当前必需的最小 UAPI，不把完整 libdrm vendor 到项目。
