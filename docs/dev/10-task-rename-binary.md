# 重命名构建产物任务

> 文档元数据
> - 文件编号：10
> - 文档类型：task
> - 文件路径：docs/dev/10-task-rename-binary.md
> - 文档版本：v1.0.0
> - 最后更新：2026-05-27
> - 需求级别：L2
> - 关联需求：编译结果名字由 `tty-ui` 改为 `andsec-disks-crypt-init-ui`。

## 1. 目标

- 默认构建产物从 `build/tty-ui` 改为 `build/andsec-disks-crypt-init-ui`。
- 保持原有 DRM/KMS、静态链接和字体字形逻辑不变。
- README 和长期开发概览同步使用新产物名。

## 2. 修改

- `Makefile`：更新 `TARGET`。
- `README.md`：更新产物路径和运行示例。
- `docs/overview-product-dev.md`：更新构建产物、接口调用、验证和排障路径。

## 3. 验证

| 命令 | 结果 |
|------|------|
| `make -B` | 通过，生成 `build/andsec-disks-crypt-init-ui` |
| `build/andsec-disks-crypt-init-ui -h` | 通过 |
| `file build/andsec-disks-crypt-init-ui` | 显示 `statically linked` |
| `readelf -d build/andsec-disks-crypt-init-ui` | 显示无 dynamic section |
| `git diff --check` | 通过 |

未在本机运行 `build/andsec-disks-crypt-init-ui -D /dev/dri/card0`，避免接管当前显示输出；实机显示和交互仍需在目标 TTY/initramfs 环境验证。
