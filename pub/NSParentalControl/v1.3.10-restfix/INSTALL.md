# NSParentalControl 通知文案防裁切包（v1.3.10，基于 v1.3.9）

## 本次修复：其它场景的提示字符被截断
v1.3.9 只修了「休息中」提示（`Rest: N min left`）。本版对**全部** UltraHand toast 文案做了
一次完整审计，把会超出弹窗宽度的文案统一缩短。

### 为什么会丢字（根因，已用 vendored UltraHand 源码核实）
- UltraHand 通知弹窗固定 **448px 宽**、字号被**钳到 34**（请求 44 也只生效 34）。
- 文本在弹窗内**居中绘制**，超出 448px 的部分被 **scissor 直接裁掉两端**（不是只丢末尾）。
- 因此文案一旦超过约 360–400px，左右都会缺字——用户看到的「字符不全」正是这个原因。

### 审计方法（可复现）
用仓库内 `libultrahand/libtesla/include/stb_truetype.h` 按设备同款公式
`像素宽 = Σ (字形 xAdvance × ScaleForPixelHeight(font, fontSize))` 测量每条文案。
以 **DejaVuSans（比 Switch 共享字体更宽）做保守上界**：只要 DejaVu 下 ≤ 448px，真机必不裁切。

### 修改前后对比（font=34 实测像素宽）

| 提示 | 原文案 | 原宽度 | 新文案 | 新宽度 |
|---|---|---|---|---|
| 时间到(日额度耗尽) | `Time's up! The game will close.` | 461 ❌ | `Time up! Closing game.` | 348 ✅ |
| 单次到时·有休息 | `Session time up! Rest N min.` | 439 ⚠️临界 | `Time up! Rest N min.` | ≤344 ✅ |
| 单次到时·无休息 | `Session time up! Paused until daily limit resets.` | 695 ❌ | `Time up! Daily limit.` | 296 ✅ |
| 休息结束 | `Break over - you can play now.` | 454 ❌ | `Break over! Play now.` | 320 ✅ |

以下文案经测量本就放得下，**未改动**：
`Parental Control enabled`(300)、`0h 54mn left`(155)、`Rest: N min left`(243)、
`Rest until daily reset`(302)、`Parental Control`(223)。

> 注：v1.3.9 已修的待机功耗问题不受影响——sysmodule 线程在灭屏睡眠期间被挂起，
> 15s/60s 轮询差异只在唤醒后才有意义，对睡眠功耗可忽略。

## 安装
覆盖 SD 卡上 `atmosphere/contents/4200000000003103/` 下的 `exefs.nsp`、`flags/boot2.flag`、`toolbox.json`，重启。
Overlay 无需改动（沿用 v1.3.7 的 overlay）。

## 重要前提
本修复仅在 **sysmodule 确实在运行** 时生效。若 `/config/parental_control/sysmodule.log` 完全无日志，
说明 sysmodule 未启动（检查 exefs.nsp / boot2.flag 是否就位、title_id 是否为 4200000000003103）。
