# 关机按钮与 Esc 输入处理任务

> 文档元数据
> - 文件编号：11
> - 文档类型：task
> - 文件路径：docs/dev/11-task-shutdown-button-escape-input.md
> - 文档版本：v1.0.0
> - 最后更新：2026-05-27
> - 需求级别：L2
> - 关联需求：Esc 不退出；退出按钮改为关机，用户选择关机时正常关闭计算机。

## 1. 目标

- Esc 不再退出程序。
- 方向键等终端转义序列不再触发退出，也不写入输入框。
- Enter 不再从输入框切换焦点，只有 Tab 负责焦点切换。
- 按钮文案由“退出”改为“关机”，保持右对齐布局。
- 用户聚焦“关机”并按 Enter 后，程序恢复显示/终端状态，再请求系统关机。

## 2. 背景与边界

- 背景：原实现把 Esc 当作退出动作，方向键在 raw input 下会产生以 Esc 开头的转义序列，导致用户误以为方向键让图形进程退出。
- 包含：键盘输入状态机、按钮文案和焦点枚举、关机系统调用、字形表生成输入、README 和总览文档。
- 不包含：新增鼠标/触控输入、认证逻辑、磁盘解密逻辑、确认弹窗。
- 关键假设：目标 initramfs/TTY 运行环境具备关机所需权限；无权限时内核会拒绝 `reboot(RB_POWER_OFF)`。
- 非目标：在开发机执行真实关机验证。
- 最大修改范围：`src/main.c`、字形生成器/生成字形表、README、产品/开发文档。
- 禁止触碰范围：不改 DRM/KMS 模式设置、dumb buffer、静态链接策略和安装脚本。

## 3. 风险门禁

| 项 | 结论 |
|----|------|
| 风险矩阵 | L2，单模块行为变更并涉及 C 代码和系统调用 |
| 高风险开发门禁 | 是，涉及 C 逻辑、终端输入解析和 `reboot` 系统调用 |
| 破坏性操作 | 代码新增关机路径；本地验证不执行真实关机 |
| 用户已有修改 | 编码前工作区仅本次相关文件变更，后续按精确范围提交 |
| 底层/系统风险 | 是，需确认显示/终端资源先恢复，再调用关机 |
| 命令权限 | C0/C1 验证；提交按项目自动提交规则执行 |
| 用户确认事项 | 用户已明确要求选择关机时正常关闭计算机 |
| 回滚/止损方式 | 回退本次提交即可恢复旧按钮与输入行为 |

## 4. 方案

- 推荐方案：把 Esc 从普通按键处理移到终端转义序列状态机中消费；将按钮焦点从 `FOCUS_EXIT` 改为 `FOCUS_SHUTDOWN`；关机按钮返回内部动作码，主流程恢复终端和显示后执行 `sync()` 与 `reboot(RB_POWER_OFF)`。
- 取舍理由：不引入新依赖，不改变 UI 主循环结构；用最小状态机覆盖方向键常见的 CSI/SS3 序列。
- 风险与应对：真实关机会影响宿主机，因此只做构建、静态链接、帮助命令和人工审查；实机关机需在目标 initramfs/TTY 环境确认。

## 5. 执行计划

| 步骤 | 修改内容 | 验证方式 | 状态 |
|------|----------|----------|------|
| 1 | 调整输入处理，Esc 和方向键转义序列不退出、不污染输入框 | 人工审查 `handle_input_byte` 和 `handle_key` | 完成 |
| 2 | 将 Enter 改为只确认按钮，输入框内不切换焦点 | `make -B`、人工审查 `handle_key` | 完成 |
| 3 | 将退出按钮改为关机按钮，并在 Enter 后触发关机动作码 | `make -B`、人工审查按钮焦点流转 | 完成 |
| 4 | 更新字形表生成输入并重新生成内嵌字形表 | `make font-atlas` | 完成 |
| 5 | 更新 README、总览文档和任务索引 | `git diff --check` | 完成 |
| 6 | 完成构建、帮助命令和静态链接验证 | `make -B`、`-h`、`file`、`readelf -d` | 完成 |

## 6. 实现记录

- 修改文件：`src/main.c`、`tools/generate_font_atlas.c`、`src/font_atlas.c`、`README.md`、`docs/overview-product.md`、`docs/overview-product-dev.md`、`docs/dev/README.md`、`docs/dev/11-task-shutdown-button-escape-input.md`。
- 关键决策：关机动作使用内部返回码 `2`，避免在 UI 循环内直接关机，确保 `terminal_restore()` 和 `display_destroy()` 先执行；Enter 在输入框焦点下不改变状态，焦点只由 Tab 切换。
- 计划偏差：无。
- 安全门禁执行结果：未执行真实关机；未触碰 DRM/KMS 初始化、buffer 管理和静态链接配置。
- L2 审视结果：修改范围限定在输入处理、按钮动作、字形和文档；错误路径保留现有资源恢复顺序；未引入新依赖或公共接口变化。

## 7. 验证记录

- 验证环境：本地仓库 `/data/code/tty-ui`。
- 系统信息（OS/内核/架构/编译器/运行时，按需）：构建产物为 x86-64 静态链接 ELF，使用系统默认 `cc` 构建。

| 验证项 | 命令/步骤 | 结果 | 备注 |
|--------|-----------|------|------|
| 字形表生成 | `make font-atlas` | 通过 | 验证“关机”字形已打包 |
| 构建 | `make -B` | 通过 | 生成 `build/andsec-disks-crypt-init-ui` |
| 帮助命令 | `build/andsec-disks-crypt-init-ui -h` | 通过 | 不需要 DRM master |
| 静态链接文件检查 | `file build/andsec-disks-crypt-init-ui` | 通过 | 显示 `statically linked` |
| 动态段检查 | `readelf -d build/andsec-disks-crypt-init-ui` | 通过 | 显示 `There is no dynamic section in this file.` |
| 补丁格式 | `git diff --check` | 通过 | 无输出 |

- 未执行验证项：未执行 `build/andsec-disks-crypt-init-ui -D /dev/dri/card0`；未触发真实关机，避免接管当前显示输出或关闭宿主机。
- 残余风险：方向键转义序列已按常见 CSI/SS3 消费；特殊终端私有序列需在目标环境实测确认。

## 8. 总结

- 最终结果：已完成 Esc/方向键输入处理、Enter 不切换焦点、关机按钮文案和关机动作接入，默认构建保持静态链接。
- 遗留风险：未在当前机器执行真实 DRM 显示和真实关机；需在目标 initramfs/TTY 环境验证权限与硬件行为。
- 后续建议：在目标 initramfs/TTY 环境补做一次人工交互验证，覆盖方向键、Esc、继续启动和关机路径。
