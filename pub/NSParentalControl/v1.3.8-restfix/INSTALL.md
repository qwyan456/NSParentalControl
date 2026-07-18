# NSParentalControl 休息时间修复包（v1.3.8，基于 v1.3.7 回退版）

## 修复内容
- 单次到时后的强制休息倒计时，改为基于 **ARM 系统节拍计数器（内核 syscall）** 的绝对墙钟结束时间。
- 与上一版（v1.3.7-restfix，用 `timeGetCurrentTime` 取 RTC 墙钟）的区别：
  - `time` 服务在 sysmodule 睡眠/唤醒后偶发返回 0 或冻结，导致 `now < restEndTs` 永远成立、
    休息倒计时卡在 30 分钟无法结束，亮屏后仍被判“休息未到”反复关闭游戏。
  - 本版改用 `armGetSystemTick()` → 纳秒：该计数器由常驻(always-on)硬件计时器驱动，
    **系统灭屏睡眠时也持续前进**，且**不依赖任何系统服务**，彻底消除睡眠后卡死的问题。
- 新增诊断日志：休息期间每轮输出 `[Monitor] Forced rest: N min left (now=... restEnd=...)`，
  可直接从 `/config/parental_control/sysmodule.log` 看到倒计时在跑、到点自动解封。

## 安装
覆盖 SD 卡上 `atmosphere/contents/4200000000003103/` 下的 `exefs.nsp`、`flags/boot2.flag`、`toolbox.json`，重启。
Overlay 无需改动（沿用 v1.3.7 的 overlay）。

## 重要前提
本修复仅在 **sysmodule 确实在运行** 时生效。若 `/config/parental_control/sysmodule.log` 完全无日志，
说明 sysmodule 未启动（检查 exefs.nsp / boot2.flag 是否就位、title_id 是否为 4200000000003103）。
