# tty UI绘制例子

这个界面可以运行在非图形 GUI 环境下，比如 initramfs。实现方式参考
裸 KMS 思路：直接使用 DRM/KMS 设置显示模式，用 DRM dumb buffer 和 CPU
软件绘制界面，不依赖 X11、Wayland、GBM、EGL、GLES 或桌面 compositor。

## 构建

需要开发包：

- `libdrm`
- `freetype2`

构建时会把 `fonts/wqy-microhei.ttc` 打包进二进制，并优先静态链接
`libfreetype.a`。运行时不再需要系统字体文件、Fontconfig 或
`libfreetype.so`。当前内嵌字体不需要 FreeType 的可选 PNG/压缩字体路径，
构建中会禁用这些路径以避免运行时依赖 `libbz2`、`libpng16`、`libz` 和
`libbrotli*`。

运行时动态依赖目标为 `libdrm.so` 和基础 C 运行时。

```sh
make
```

产物位于 `build/tty-ui`。

## 运行

需要在真实 TTY/initramfs 等可获取 DRM master 的环境运行：

```sh
build/tty-ui -D /dev/dri/card0
```

可用参数：

- `-D <device>`：指定 DRM 设备，默认 `/dev/dri/card0`。

键盘交互：

- `Tab` 切换用户名、密码、继续启动、退出。
- `Enter` 在输入框内切到下一项，在按钮上确认。
- `Backspace` 删除当前输入框内容。
- `Esc` 退出。
