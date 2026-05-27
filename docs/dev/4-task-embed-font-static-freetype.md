# 内嵌字体与静态 FreeType 轻量任务记录

> 文档元数据
> - 文件编号：4
> - 文档类型：task
> - 文件路径：docs/dev/4-task-embed-font-static-freetype.md
> - 文档版本：v1.0.0
> - 最后更新：2026-05-27
> - 需求级别：L2
> - 关联需求：字体已经放到 `fonts/` 下；去掉 `-f`；将 FreeType 改为尽可能静态编译以减少依赖。

## 1. 目标

- 要解决的问题：减少目标环境字体和 FreeType 运行时依赖。
- 成功标准：默认字体从 `fonts/wqy-microhei.ttc` 打包进二进制；程序不再支持 `-f` 参数；代码不再依赖 Fontconfig；构建优先静态链接 `libfreetype.a`；程序可构建。

## 2. 背景与边界

- 背景：当前程序通过 Fontconfig 查找系统字体，或通过 `-f` 指定外部字体文件；这会增加 initramfs/TTY 环境依赖。
- 包含：Makefile 内嵌字体对象生成、FreeType 静态链接调整、代码改为 `FT_New_Memory_Face()`、README 和开发文档更新。
- 不包含：将 DRM/GBM/EGL/GLES 改为静态链接、字体子集化、完整静态二进制。
- 关键假设：`fonts/wqy-microhei.ttc` 可作为默认内嵌中文字体；目标构建机提供 `libfreetype.a`。
- 非目标：不改变 UI 文字、布局、输入逻辑。
- 最大修改范围：`Makefile`、`src/main.c`、`README.md`、`docs/overview-product*.md`、`docs/dev/`。
- 禁止触碰范围：不修改 `/data/source/kmscube`；不触碰未跟踪 `.codex`。

## 3. 风险门禁

| 项 | 结论 |
|----|------|
| 风险矩阵 | L2：C 代码和构建链接调整，影响依赖和字体加载路径。 |
| 高风险开发门禁 | 是：C 逻辑、内存生命周期、构建链接。 |
| 破坏性操作 | 否。 |
| 用户已有修改 | 是：未跟踪 `.codex` 不触碰。 |
| 底层/系统风险 | 中：链接方式变化可能影响目标构建环境；不改 KMS 运行路径。 |
| 命令权限 | C0/C1；提交阶段限定文件暂存。 |
| 用户确认事项 | 无。 |
| 回滚/止损方式 | 回退本次提交即可。 |

## 4. 方案

- 推荐方案：Makefile 使用 `ld -r -b binary` 将 `fonts/wqy-microhei.ttc` 转为目标文件并链接进程序；C 代码通过 linker 生成的 `_binary_*_start/end` 符号调用 `FT_New_Memory_Face()`；移除 Fontconfig include、查找逻辑和 `-f` 参数。
- FreeType 链接：优先直接链接 `pkg-config --variable=libdir freetype2` 下的 `libfreetype.a`，并保留 `pkg-config --libs --static freetype2` 中除 `-lfreetype` 外的依赖库。
- 取舍理由：避免生成 5MB 以上 C 数组文件；减少运行时字体和 Fontconfig 依赖；保留 DRM/GBM/EGL/GLES 动态链接，范围可控。
- 风险与应对：本机构建环境缺少 Brotli 静态库，因此不能保证 FreeType 依赖链完全静态；通过 `ldd` 验证不再依赖 `libfreetype.so` 和 `libfontconfig.so`。

## 5. 执行计划

| 步骤 | 修改内容 | 验证方式 | 状态 |
|------|----------|----------|------|
| 1 | 创建任务文档和索引 | 人工检查编号与内容 | 完成 |
| 2 | 改代码为内嵌字体并移除 `-f`/Fontconfig | `make` | 完成 |
| 3 | 调整 Makefile 静态链接 FreeType | `make`、`ldd build/tty-ui` | 完成 |
| 4 | 更新 README/总文档 | 人工检查 | 完成 |
| 5 | 验证并提交 | `git diff --check`、`git status --short` | 完成 |

## 6. 实现记录

- 修改文件：`Makefile`、`src/main.c`、`README.md`、`docs/overview-product.md`、`docs/overview-product-dev.md`、`docs/dev/README.md`、`docs/dev/4-task-embed-font-static-freetype.md`。
- 关键决策：
  - 通过 `ld -r -b binary` 将 `fonts/wqy-microhei.ttc` 转为目标文件并链接进二进制。
  - 使用 linker 符号 `_binary_fonts_wqy_microhei_ttc_start/end` 调用 `FT_New_Memory_Face()`。
  - 去掉 `-f` 参数和 Fontconfig 依赖，默认字体固定为内嵌字体。
  - Makefile 优先直接链接 `libfreetype.a`，并保留 FreeType 的压缩库依赖。
  - 字体目标对象通过 `objcopy` 将 `.data` 改为只读 `.rodata`，并补充 `.note.GNU-stack` 避免可执行栈警告。
- 计划偏差：无。
- 安全门禁执行结果：未触碰 `/data/source/kmscube`；未运行会接管真实显示输出的 KMS 程序；未触碰未跟踪 `.codex`。

## 7. 验证记录

- 验证环境：本地工作区 `/data/code/tty-ui`。
- 系统信息（OS/内核/架构/编译器/运行时，按需）：沿用 docs/dev/1-task-diskcrypt-kms-ui.md 记录的本地环境；本机存在 `/usr/lib64/libfreetype.a`，缺少 Brotli 静态库。

| 验证项 | 命令/步骤 | 结果 | 备注 |
|--------|-----------|------|------|
| 构建验证 | `make -B` | 通过 | 强制重构，字体对象和主程序均生成成功。 |
| 帮助路径 | `build/tty-ui -h` | 通过 | 不初始化 DRM；帮助中不再出现 `-f`。 |
| 移除 `-f` | `build/tty-ui -f fonts/wqy-microhei.ttc` | 通过 | 返回无效选项并打印不含 `-f` 的用法。 |
| 动态依赖检查 | `ldd build/tty-ui` | 通过 | 无 `libfreetype.so`/`libfontconfig.so`；仍动态依赖 DRM/GBM/EGL/GLES 和 FreeType 压缩库依赖。 |
| Diff 检查 | `git diff --check` | 通过 | 未发现空白或补丁格式问题。 |

- 未执行验证项：未在真实 TTY/initramfs 上运行 `build/tty-ui -D /dev/dri/card0`，避免在当前环境切换真实显示输出。
- 残余风险：本机构建环境缺少 Brotli 静态库，因此 FreeType 依赖链未完全静态；目标构建环境若缺少 `libfreetype.a` 会回退到动态 FreeType。

## 8. 总结

- 最终结果：已内嵌 `fonts/wqy-microhei.ttc`，移除 `-f` 和 Fontconfig，构建产物不再动态依赖 `libfreetype.so`/`libfontconfig.so`。
- 遗留风险：DRM/GBM/EGL/GLES 和 FreeType 的压缩库依赖仍是动态库；完整静态二进制不在本次范围。
- 后续建议：若要进一步降低依赖，可在目标构建环境准备 Brotli 等静态库，或裁剪/重编 FreeType 禁用不需要的字体压缩特性。
