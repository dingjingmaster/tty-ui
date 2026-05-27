# 内嵌位图字形 Plan

> 文档元数据
> - 文件编号：7
> - 文档类型：plan
> - 文件路径：docs/dev/7-plan-embedded-bitmap-font.md
> - 文档版本：v1.0.0
> - 最后更新：2026-05-27
> - 需求级别：L3
> - 关联需求：不依赖 `libfreetype.a`，把相关代码整合到项目里。

## 1. 修改范围

- `src/font_atlas.c`、`src/font_atlas.h`：新增预生成位图字形表。
- `tools/generate_font_atlas.c`：新增开发用字形表生成器。
- `src/main.c`：移除 FreeType API，改用字形表绘制文字。
- `src/freetype_optional_stubs.c`：删除不再需要的 FreeType stub。
- `Makefile`：默认构建不再依赖 FreeType；新增可选 `font-atlas` 目标。
- `README.md`、`docs/overview-product-dev.md`：更新依赖说明。

## 2. 执行计划

| 步骤 | 修改内容 | 验证方式 | 状态 |
|------|----------|----------|------|
| 1 | 创建 Research/Plan 文档和索引 | 人工检查编号与边界 | 已完成 |
| 2 | 新增生成器并生成字形表 | `make font-atlas` | 已完成 |
| 3 | 改 `src/main.c` 使用字形表 | `make -B` | 已完成 |
| 4 | 调整 Makefile/README/总文档 | `ldd build/tty-ui`、人工检查 | 已完成 |
| 5 | 验证并提交 | `build/tty-ui -h`、`readelf -d build/tty-ui`、`git diff --check` | 已完成 |

## 3. 风险门禁

| 项 | 结论 |
|----|------|
| 风险矩阵 | L3：字体渲染架构和构建依赖变化。 |
| 高风险开发门禁 | 是：C 逻辑、内存/边界、构建链接。 |
| 破坏性操作 | 否。 |
| 用户已有修改 | 否。 |
| 命令权限 | C0/C1；提交阶段限定文件暂存。 |
| 回滚/止损方式 | 回退本次提交即可。 |

## 4. 验证标准

- `make font-atlas` 可重新生成字形表。
- `make -B` 构建通过，默认构建不链接 FreeType。
- `build/tty-ui -h` 正常输出帮助。
- `ldd build/tty-ui` 仍只显示 `libdrm`、`libc` 和基础 ELF 运行项。
- `readelf -d build/tty-ui` 直接 NEEDED 仅 `libdrm`、`libc`。
- `git diff --check` 通过。

## 5. 审视

- 安全：字形查找和 bitmap copy 必须检查边界。
- 产品：当前 UI 文案和 ASCII 用户名输入可显示。
- 架构：默认构建不再依赖 FreeType，减少目标和构建环境要求。
