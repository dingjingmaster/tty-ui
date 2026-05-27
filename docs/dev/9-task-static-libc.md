# 静态链接 libc 任务

> 文档元数据
> - 文件编号：9
> - 文档类型：task
> - 文件路径：docs/dev/9-task-static-libc.md
> - 文档版本：v1.0.0
> - 最后更新：2026-05-27
> - 需求级别：L2
> - 关联需求：不做 nolibc，只做到静态链接 libc，去掉 `libc.so.6` 运行依赖。

## 1. 目标

- 默认 `make` 产物静态链接 libc。
- 不改为 nolibc，不移除 libc 代码本身。
- 保留 `make STATIC=0` 作为动态链接调试回退。

## 2. 修改

- `Makefile` 新增 `STATIC ?= 1`。
- `STATIC=1` 时编译加入 `-fno-pie`，链接加入 `-static -no-pie`。
- `README.md` 和总览文档同步说明默认静态链接 libc。

## 3. 验证

| 命令 | 结果 |
|------|------|
| `make -B` | 通过，默认链接参数包含 `-static -no-pie` |
| `make STATIC=0 -B` | 通过，动态链接调试回退可用 |
| `make font-atlas` | 通过 |
| `file build/tty-ui` | 显示 `statically linked` |
| `readelf -d build/tty-ui` | 显示无 dynamic section |
| `build/tty-ui -h` | 通过 |

`ldd build/tty-ui` 在当前系统上返回 `exited with unknown exit code (159)`；结合
`file` 和 `readelf -d`，可确认产物不是动态链接 ELF。

未在本机运行 `build/tty-ui -D /dev/dri/card0`，避免接管当前显示输出；实机显示和交互仍需在目标 TTY/initramfs 环境验证。
