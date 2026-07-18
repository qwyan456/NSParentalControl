# NSParentalControl 休息计时溢出修复（v1.3.11，基于 v1.3.10）

## 本次修复（P0）：休息剩余时间被撑大 / 休息被无限拉长

### 现象（用户实测 + 附件日志）
- 设置 `rest_duration = 5` 分钟，却出现「rest 剩余 9 分钟」的提示。
- sysmodule.log 中 `now` 从 `435,541,724,791` 突然**倒退**到 `23,745,264,055`（倒退约 412 秒），
  而两次读取只隔 15s(本次已改为 30s)轮询；`restEndTs_` 是固定值 `504,082,980,052`，
  于是 `remaining = restEndTs_ - now` 被撑大到 ~480s → 显示 9 分钟。

### 根因（坑 10，P0 级）
`nowNs()` 原实现：
```cpp
return (ticks * 1000000000ULL) / 19200000ULL;   // ❌ ticks * 1e9 会溢出 u64
```
`cntpct_el0` 频率 19.2MHz，开机约 **16 分钟**时 `ticks ≈ 1.84e10`，
`ticks * 1e9 ≈ 1.84e19` 超过 `u64` 最大值 `2^64 ≈ 1.84e19` → **乘法溢出回绕**，
`now` 从几百秒掉到十几秒。此后 `now` 要再爬回 `restEndTs_` 才解封，
**实际休息从 5 分钟被拉长到十几分钟**，且任何「开机 >16 分钟时进入休息」都会触发
（Switch 几乎总是开很久，故为必现/P0）。

> 注：`armGetSystemTick()` 读的是 `cntpct_el0` 物理计数器，本身单调递增、睡眠也前进，
> 硬件没问题——纯粹是纳秒换算的整数溢出。

### 正确做法（v1.3.11）
`nowNs()` 改为「先除后乘 + 余数补偿」，任意现实开机时长(数年)都不溢出：
```cpp
const u64 ticks = armGetSystemTick();
constexpr u64 freq = 19200000ULL;
const u64 sec  = ticks / freq;
const u64 frac = ticks % freq;
return sec * 1000000000ULL + (frac * 1000000000ULL) / freq;
```
已用独立程序验证：旧公式在 961s(16min)溢出，新公式在 60s~1 年全部正确且跨回绕点单调。

### 附带调整
- 休息守卫内轮询间隔 **15s → 30s**（用户要求）：提示仍稳定出现、到点更快解封，且更省电、弹窗不频繁。

## 安装
覆盖 SD 卡 `atmosphere/contents/4200000000003103/` 下的 `exefs.nsp`、`flags/boot2.flag`、`toolbox.json`，重启。
详见包内说明；Overlay 无需改动。

## 重要前提
sysmodule 必须真的在跑（日志在 `/config/parental_control/sysmodule.log`），否则限玩/休息逻辑不生效。
