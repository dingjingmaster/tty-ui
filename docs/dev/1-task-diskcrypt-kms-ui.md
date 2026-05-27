# DiskCrypt KMS 登录界面 轻量任务记录

> 文档元数据
> - 文件编号：1
> - 文档类型：task
> - 文件路径：docs/dev/1-task-diskcrypt-kms-ui.md
> - 文档版本：v1.0.0
> - 最后更新：2026-05-27
> - 需求级别：L2
> - 关联需求：分析 `/data/source/kmscube` 并使用相同技术实现可运行在非图形界面环境下的 DiskCrypt 登录界面。

## 1. 目标

- 要解决的问题：在无 X11/Wayland 等图形桌面环境下，使用裸 DRM/KMS 图形路径绘制一个 DiskCrypt 登录界面。
- 成功标准：程序可构建；运行时可直接打开 DRM 设备完成模式设置和页面翻转；界面包含标题、提示、用户名输入框、密码输入框、继续启动/退出按钮和底部标语；Tab 可切换焦点，Enter 可确认按钮。

## 2. 背景与边界

- 背景：`/data/source/kmscube` 是裸机图形演示程序，使用 DRM/KMS、GBM、EGL 和 OpenGL ES 2.0，无需 compositor 或桌面环境。
- 包含：最小 C 工程、裸 KMS/EGL/GLES 渲染、中文/英文文字绘制、登录表单交互、构建说明。
- 不包含：真实 DiskCrypt 认证、磁盘解密流程、PAM/系统账户集成、触摸/鼠标输入、复杂动画。
- 关键假设：目标环境具备可用 `/dev/dri/card*`、DRM master 权限、GBM/EGL/GLES 运行库，以及可覆盖中文的字体文件。
- 非目标：不实现终端字符 UI，不依赖 X11/Wayland，不引入完整 GUI 框架。
- 最大修改范围：`src/`、`Makefile`、`README.md`、`.gitignore`、`docs/dev/`、`docs/overview-product*.md`。
- 禁止触碰范围：不修改 `/data/source/kmscube`；不修改系统配置、设备节点或图形服务；不触碰未跟踪 `.codex`。

## 3. 风险门禁

| 项 | 结论 |
|----|------|
| 风险矩阵 | L2：单模块新功能，但涉及 C 代码、设备 I/O 和底层图形 API。 |
| 高风险开发门禁 | 是：C 逻辑、DRM/KMS 设备 I/O、内存/资源生命周期、构建链接。 |
| 破坏性操作 | 否：不执行会切换真实显示输出的运行验证；仅构建。 |
| 用户已有修改 | 是：工作区存在未跟踪 `.codex`，本任务不触碰。 |
| 底层/系统风险 | 是：打开 DRM 设备、模式设置、page flip；通过错误路径清理和不实机运行降低风险。 |
| 命令权限 | C0/C1；提交阶段按项目规范执行限定文件 `git add` 与 `git commit`。 |
| 用户确认事项 | 无。 |
| 回滚/止损方式 | 代码回退即可；程序运行时保存旧 CRTC 并在退出路径尝试恢复。 |

## 4. 方案

- 推荐方案：参考 kmscube 的裸显示链路，使用 DRM/KMS 选择 connector/mode，GBM 创建 scanout surface，EGL 创建 OpenGL ES 2.0 context；用 GLES 绘制矩形/边框/按钮，用 Fontconfig+FreeType 将中文和英文文本栅格化成纹理。
- 取舍理由：DRM/KMS+GBM+EGL/GLES 与 kmscube 技术一致，能运行在无 GUI 环境；FreeType/Fontconfig 是文字渲染的最小补充，避免手写中文点阵。
- 风险与应对：KMS 程序运行会接管显示输出，因此本轮只做构建验证；运行说明中明确需要 TTY/DRM master 权限。C 资源生命周期通过统一清理函数和错误路径检查控制。

## 5. 执行计划

| 步骤 | 修改内容 | 验证方式 | 状态 |
|------|----------|----------|------|
| 1 | 创建任务文档和索引 | 人工检查文档内容与编号 | 完成 |
| 2 | 新增构建入口和 DRM/KMS+EGL/GLES UI 程序 | `make` | 完成 |
| 3 | 更新 README 和忽略构建产物 | `git diff --check`、人工检查 | 完成 |
| 4 | 构建验证并审视资源/错误路径 | `make`、`git diff --check` | 完成 |
| 5 | 更新总结与索引状态，提交本次变更 | `git status --short`、提交结果 | 完成 |

## 6. 实现记录

- 修改文件：`Makefile`、`src/main.c`、`.gitignore`、`README.md`、`docs/overview-product.md`、`docs/overview-product-dev.md`、`docs/dev/README.md`、`docs/dev/1-task-diskcrypt-kms-ui.md`。
- 关键决策：
  - `/data/source/kmscube` 的核心链路为 DRM/KMS + GBM + EGL + OpenGL ES 2.0；本实现沿用该裸显示链路，不依赖 X11/Wayland。
  - 图形元素使用 GLES 矩形和边框绘制；中文/英文文字使用 Fontconfig 选字、FreeType 栅格化，再作为 GLES alpha 纹理绘制。
  - 交互只实现当前需求所需的键盘路径：Tab 切换、Enter 确认、Backspace 删除、Esc 退出。
  - 程序退出时在已完成 mode set 的情况下尝试恢复旧 CRTC。
- 计划偏差：新增了 `docs/overview-product.md` 和 `docs/overview-product-dev.md`，用于沉淀本次形成的长期产品/技术事实。
- 安全门禁执行结果：未触碰 `/data/source/kmscube`；未运行会接管真实显示输出的 KMS 程序；未触碰未跟踪 `.codex`。

## 7. 验证记录

- 验证环境：本地工作区 `/data/code/tty-ui`。
- 系统信息（OS/内核/架构/编译器/运行时，按需）：Linux gentoo-pc 7.0.0-gentoo-dingjing x86_64；cc 15.2.1；`libdrm` 2.4.131、`gbm` 25.3.6、`egl` 1.5、`glesv2` 3.2、`freetype2` 26.6.20、`fontconfig` 2.17.1。

| 验证项 | 命令/步骤 | 结果 | 备注 |
|--------|-----------|------|------|
| 构建验证 | `make` | 通过 | 生成 `build/tty-ui`；无编译警告。 |
| 帮助路径 | `build/tty-ui -h` | 通过 | 仅打印帮助，不初始化 DRM。 |
| Diff 检查 | `git diff --check` | 通过 | 未发现空白或补丁格式问题。 |

- 未执行验证项：未在真实 TTY/initramfs 上运行 `build/tty-ui -D /dev/dri/card0`，避免在当前环境切换真实显示输出。
- 残余风险：不同 GPU/驱动的 GBM/EGL 支持、DRM master 权限、connector 状态和字体覆盖需要在目标设备上实测。

## 8. 总结

- 最终结果：已实现可构建的裸 KMS DiskCrypt 登录界面，提供用户名/密码输入、按钮焦点和基础键盘交互。
- 遗留风险：目标设备实机显示路径未在本轮验证；真实认证/解密流程尚未接入。
- 后续建议：在目标 initramfs/TTY 环境执行实机显示验证，并根据实际屏幕分辨率调整布局参数。
