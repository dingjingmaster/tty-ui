# 裁剪运行时动态依赖 轻量任务记录

> 文档元数据
> - 文件编号：5
> - 文档类型：task
> - 文件路径：docs/dev/5-task-trim-runtime-deps.md
> - 文档版本：v1.0.0
> - 最后更新：2026-05-27
> - 需求级别：L2
> - 关联需求：分析 `ldd build/tty-ui` 依赖，尽可能多地去掉依赖并保持功能正常使用。

## 1. 目标

- 要解决的问题：当前二进制仍动态依赖 FreeType 可选压缩/PNG 相关库。
- 成功标准：在保持内嵌 `fonts/wqy-microhei.ttc` 正常渲染的前提下，`ldd build/tty-ui` 不再出现 `libbz2.so`、`libpng16.so`、`libz.so`、`libbrotlidec.so`、`libbrotlicommon.so`；程序可构建。

## 2. 背景与边界

- 背景：发行版 `libfreetype.a` 启用了 PNG、gzip、bzip2、Brotli 等可选路径，静态链接 FreeType 时仍会要求这些库。
- 包含：添加 FreeType 可选压缩/PNG 路径的 stub，调整 Makefile 链接参数，更新验证记录。
- 不包含：重编 FreeType、静态链接 DRM/GBM/EGL/GLES、切换到 DRM dumb buffer/software raster。
- 关键假设：当前内嵌字体 `fonts/wqy-microhei.ttc` 是普通 TrueType collection，不需要 PNG embedded bitmap、gzip/bzip stream 或 WOFF2/Brotli 解码。
- 非目标：不支持将内嵌字体替换为 WOFF2、gzip/bzip 压缩字体或依赖 PNG embedded bitmap 的彩色字体。
- 最大修改范围：`src/`、`Makefile`、`docs/dev/`。
- 禁止触碰范围：不修改 `/data/source/kmscube`。

## 3. 风险门禁

| 项 | 结论 |
|----|------|
| 风险矩阵 | L2：C 代码和链接依赖调整，影响字体加载边界。 |
| 高风险开发门禁 | 是：C 逻辑、构建链接、运行时依赖。 |
| 破坏性操作 | 否。 |
| 用户已有修改 | 否。 |
| 底层/系统风险 | 低：不改 DRM/KMS/GBM/EGL/GLES 路径。 |
| 命令权限 | C0/C1；提交阶段限定文件暂存。 |
| 用户确认事项 | 无。 |
| 回滚/止损方式 | 回退本次提交即可。 |

## 4. 方案

- 推荐方案：新增 `src/freetype_optional_stubs.c`，为 FreeType 可选 PNG/gzip/bzip/Brotli 符号提供失败返回 stub；Makefile 只链接 `libfreetype.a`，不再链接 `pkg-config --libs --static freetype2` 的可选库。
- 取舍理由：当前内嵌 TTC 字体不需要这些可选路径，stub 可最大限度减少 `ldd` 动态库条目；比重编 FreeType 更小，适合当前仓库。
- 风险与应对：若未来更换为 WOFF2、压缩字体或 PNG bitmap 字体，这些可选路径会失败；任务文档记录该边界。

## 5. 执行计划

| 步骤 | 修改内容 | 验证方式 | 状态 |
|------|----------|----------|------|
| 1 | 创建任务文档和索引 | 人工检查编号与内容 | 完成 |
| 2 | 添加 FreeType 可选路径 stub | `make -B` | 完成 |
| 3 | 调整 Makefile 链接参数 | `ldd build/tty-ui` | 完成 |
| 4 | 验证并提交 | `git diff --check`、`git status --short` | 完成 |

## 6. 实现记录

- 修改文件：`Makefile`、`src/freetype_optional_stubs.c`、`README.md`、`docs/overview-product-dev.md`、`docs/dev/README.md`、`docs/dev/5-task-trim-runtime-deps.md`。
- 关键决策：
  - 保留 kmscube 同类 GBM/EGL/GLES 图形路径，因此 `libdrm`、`libgbm`、`libEGL`、`libGLESv2` 仍是必要依赖。
  - 当前内嵌字体是普通 TTC/TrueType outline 字体，不需要 FreeType 的 gzip/bzip/PNG/Brotli 路径。
  - 用 `src/freetype_optional_stubs.c` 对这些可选路径提供失败返回，避免链接可选库。
  - Makefile 只链接 `libfreetype.a`，不再链接 `pkg-config --libs --static freetype2` 给出的可选库集合。
- 计划偏差：无。
- 安全门禁执行结果：未触碰 `/data/source/kmscube`；未运行会接管真实显示输出的 KMS 程序。

## 7. 验证记录

- 验证环境：本地工作区 `/data/code/tty-ui`。
- 系统信息（OS/内核/架构/编译器/运行时，按需）：沿用 docs/dev/1-task-diskcrypt-kms-ui.md 记录的本地环境。

| 验证项 | 命令/步骤 | 结果 | 备注 |
|--------|-----------|------|------|
| 构建验证 | `make -B` | 通过 | 强制重构，无链接错误。 |
| 帮助路径 | `build/tty-ui -h` | 通过 | 不初始化 DRM。 |
| 动态依赖检查 | `ldd build/tty-ui` | 通过 | 不再出现 `libbz2.so`、`libpng16.so`、`libz.so`、`libbrotlidec.so`、`libbrotlicommon.so`。 |
| 直接依赖检查 | `readelf -d build/tty-ui \| rg "NEEDED"` | 通过 | 主程序直接依赖为 `libdrm`、`libgbm`、`libEGL`、`libGLESv2`、`libc`。 |
| Diff 检查 | `git diff --check` | 通过 | 未发现空白或补丁格式问题。 |

- 未执行验证项：未在真实 TTY/initramfs 上运行 `build/tty-ui -D /dev/dri/card0`，避免在当前环境切换真实显示输出。
- 残余风险：如果未来换成 WOFF2、gzip/bzip 压缩字体或依赖 PNG embedded bitmap 的字体，可选路径会失败；当前 `fonts/wqy-microhei.ttc` 不受影响。

## 8. 总结

- 最终结果：已去掉 FreeType 可选压缩/PNG 相关动态依赖；`ldd` 中保留的图形库和其传递依赖来自 kmscube 同类 GBM/EGL/GLES 技术路径。
- 遗留风险：完整去掉 `libgbm`、`libEGL`、`libGLESv2`、`libGLdispatch`、`libexpat`、`libm` 需要改成 DRM dumb buffer/software raster 或提供这些图形库的静态构建，不在本次范围。
- 后续建议：如果目标是最小 initramfs 依赖，可评估是否接受从 GBM/EGL/GLES 切换到 DRM dumb buffer 的软件绘制方案。
