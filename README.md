# tty UI绘制例子

这个界面可以运行在非图形 GUI 环境下，比如 initramfs。实现方式参考
裸 KMS 思路：直接使用 DRM/KMS 设置显示模式，用 DRM dumb buffer 和 CPU
软件绘制界面，不依赖 X11、Wayland、GBM、EGL、GLES 或桌面 compositor。

## 构建

默认构建需要 C 编译器、`make` 和基础 libc 头文件，不需要 `libdrm` 开发包。

默认构建使用 `src/font_atlas.c` 中的内嵌位图字形表，不依赖系统字体文件、
Fontconfig、FreeType 或 `libfreetype.a`。

DRM/KMS 功能通过项目内最小 ioctl 封装保留，运行时动态依赖目标为基础
C 运行时。

```sh
make
```

产物位于 `build/tty-ui`。

如果更换 `fonts/wqy-microhei.ttc`、调整界面文案或新增字号，需要在开发机安装
`freetype2` 后重新生成字形表：

```sh
make font-atlas
```

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
