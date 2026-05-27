# 内嵌位图字形 Summary

> 文档元数据
> - 文件编号：7
> - 文档类型：summary
> - 文件路径：docs/dev/7-summary-embedded-bitmap-font.md
> - 文档版本：v1.0.0
> - 最后更新：2026-05-27
> - 需求级别：L3
> - 关联需求：不依赖 `libfreetype.a`，把相关代码整合到项目里。

## 1. 结果

- 默认构建改为直接编译 `src/font_atlas.c`/`src/font_atlas.h`，主程序不再链接 `libfreetype.a`。
- 新增 `tools/generate_font_atlas.c` 和 `make font-atlas`，仅在更换字体、文案或字号时用 `freetype2` 重新生成字形表。
- `src/main.c` 移除 FreeType API，改为查找内嵌位图字形并进行 CPU alpha blending。
- 删除 `src/freetype_optional_stubs.c`，因为主程序不再链接 FreeType，也不再需要可选依赖 stub。

## 2. 字符覆盖

- ASCII 可打印字符 `0x20..0x7e`：覆盖用户名输入、密码掩码和英文提示。
- 当前 UI 中文文案：覆盖“安得合众”、登录标题、操作提示、用户名/密码标签、按钮和底部标语。
- 当前 UI 字号：22、24、28、34。
- 新增文案、字号或需要显示非 ASCII 用户输入时，必须重新运行 `make font-atlas`。

## 3. 依赖结果

`ldd build/tty-ui` 仅显示：

```text
linux-vdso.so.1
libdrm.so.2
libc.so.6
/lib64/ld-linux-x86-64.so.2
```

`readelf -d build/tty-ui` 的直接 `NEEDED` 仅为：

```text
libdrm.so.2
libc.so.6
```

## 4. 验证

| 命令 | 结果 |
|------|------|
| `make font-atlas` | 通过 |
| `make -B` | 通过，默认构建未使用 FreeType 参数 |
| `build/tty-ui -h` | 通过 |
| `ldd build/tty-ui` | 通过，无 FreeType/Fontconfig/字体压缩库依赖 |
| `readelf -d build/tty-ui` | 通过，直接依赖仅 `libdrm.so.2`、`libc.so.6` |
| `git diff --check` | 通过 |

未在本机运行 `build/tty-ui -D /dev/dri/card0`，避免接管当前显示输出；实机显示和交互仍需在目标 TTY/initramfs 环境验证。
