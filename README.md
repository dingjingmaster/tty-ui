# tty UI绘制例子

这个界面可以运行在非图形 GUI 环境下，比如 initramfs。实现方式参考
`kmscube` 的裸机图形链路：DRM/KMS + GBM + EGL + OpenGL ES 2.0，不依赖
X11、Wayland 或桌面 compositor。

## 构建

需要开发包：

- `libdrm`
- `gbm`
- `egl`
- `glesv2`
- `freetype2`

构建时会把 `fonts/wqy-microhei.ttc` 打包进二进制，并优先静态链接
`libfreetype.a`。运行时不再需要系统字体文件、Fontconfig 或
`libfreetype.so`。

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
