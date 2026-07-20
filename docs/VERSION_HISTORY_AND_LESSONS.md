# 版本修改记录与教训汇总（v1.3.7 → v1.3.12）

> 目的：把 v1.3.7 之后整条迭代线的改动与**踩过的坑**集中记录，避免后续开发重复同样的错误。
> 本仓库已在 `archive/attempts-v1.3.8-v1.3.12` 分支保留 v1.3.8→v1.3.12 的全部提交；
> `main` 已回退到 v1.3.7（`999cc5f`），后续开发基于新分支从 1.3.7 起步。

---

## 一、各版本改动一览

| 版本 | 关键改动 | 结果 |
|---|---|---|
| **v1.3.7**（基线 `999cc5f`） | 限玩提示更醒目（字号/优先级/重复）；休息中被关反馈。服务在 **qlaunch 就绪后**才注册（开机约 30+ 秒）。 | **可用**，但 Overlay 若提前打开会误报 not installed（单次探测、缓存 false）；**存在休息时间计算 bug（本次要修）** |
| v1.3.8 | 全屏醒目提示：vi 图层 + 帧缓冲；NPDM `system_resource_size: 0x1000000`。 | 可正常开机 |
| v1.3.9 | 试图“开机瞬间立即注册 pctrl 服务”+ 动态堆 `svcSetHeapSize` | **开机崩溃**；且 not installed **更严重**（开机瞬间注册被拒，服务永远不可用） |
| v1.3.10 | 撤回到静态 512KB inner heap（sys-clk 标准） | 开机正常；但**限玩到时不限制游戏**（监控线程崩在 terminate 之前） |
| v1.3.11 | 恢复 `system_resource_size`；`monitor` 先 `terminateCurrentApplication()` 再 `drawNotice()`；`framebufferCreate` 失败安全降级 toast | 限玩到时能关游戏 ✅ |
| v1.3.12 | Overlay `MainMenuPanel::update()` 每 2s 重探测 `connectToService()`（自愈）；Sysmodule 注册改回 qlaunch 后并加重试循环 | 本机 sysmodule 仍未注册上（无 sysmodule 日志）→ not installed 仍在；**整条线放弃，回退到 1.3.7** |

---

## 二、不可重复的坑（PITFALLS）—— 务必记住

### ❌ 坑 1：boot2 sysmodule 里用 `svcSetHeapSize` 动态堆 → 开机崩溃
- **现象**：开机过程中系统 crash。
- **根因**：boot2 进程**没有预留堆区**，`svcSetHeapSize` 被内核拒绝（`rc=1:101`）→ 堆大小保持 0 → `new` 抛 `std::bad_alloc` → `std::terminate` → 崩溃。
- **正确做法**：用静态 inner heap（sys-clk / nx-ovlloader 标准）：
  ```cpp
  constexpr size_t INNER_HEAP_SIZE = 0x80000; // 512KB（boot2 安全上限）
  char nx_inner_heap[INNER_HEAP_SIZE];
  size_t nx_inner_heap_size = INNER_HEAP_SIZE;
  void __libnx_initheap(void) { /* fake_heap_start/end = nx_inner_heap */ }
  ```
- **历史**：v1.3.5、v1.3.9 两次踩中。512KB + 线程栈(~384KB) ≈ 896KB，远低于挤掉 qlaunch 的阈值。

### ❌ 坑 2：`smRegisterService` 在开机瞬间（boot2 起始）调用 → 本机永远注册不上
- **现象**：Overlay 永久 “not installed”。
- **根因**：本机 boot2 环境下，开机起始时刻 sm 尚未就绪，`smRegisterService` 被拒；而 **qlaunch 就绪之后**注册（v1.3.7 方式）则可用。
- **正确做法**：注册放在 `waitForQLaunch()` 之后；可加重试循环（每 500ms，最多 ~60s）应对 sm 瞬时不可用。
- **配套**：Overlay 必须**周期性重探测**服务（见坑 6），否则服务晚注册时仍会误报。

### ❌ 坑 3：删除 NPDM 的 `system_resource_size` → 限玩到时不限制游戏
- **现象**：到达 limit 时间但游戏没被关。
- **根因**：`system_resource_size` 提供帧缓冲所需内存区（TransferMemory 的 backing 来自进程堆/资源区）。删除后 `framebufferCreate` 失败 → `m_currentFramebuffer` 为 NULL → `drawNotice()` 写空指针 → 监控线程崩溃 → 崩溃发生在 `terminateCurrentApplication()` **之前** → 游戏从不被终止。
- **正确做法**：保留 `system_resource_size: 0x1000000`（与 v1.3.8 一致，实测可开机）。

### ❌ 坑 4：`monitor` 先画提示再终止游戏 → 渲染失败阻断限制
- **正确做法**：**永远先 `terminateCurrentApplication()`，再 `drawNotice()`**。`framebufferCreate` 失败时在 `Renderer` 里安全降级为系统 toast（`drawNotice` 返回 false），绝不写空指针。

### ❌ 坑 5：休息时间用“循环每 1 分钟 -1”计数器 → 灭屏睡眠后算错（**已修**）
- **现象**：单次到时进入强制休息；灭屏（系统睡眠）超过设定休息时长后，亮屏打开游戏仍提示“休息时间没到”被关。
- **根因**：`restRemainingMin_--` 依赖监控循环每 1 分钟跑一次；**系统睡眠时监控线程被挂起，循环不执行，计数器冻结**。真实墙钟已超设定休息时长，但 `restRemainingMin_` 没减，于是仍判为“休息中”。
- **正确做法**：用**绝对墙钟结束时间**判断：
  ```cpp
  // 进入休息：restEndTs_ = nowNs() + (u64)restMin * 60 * 1'000'000'000;  // 纳秒
  // 守卫中： if (nowNs() >= restEndTs_) { 休息结束 }
  ```

### ❌ 坑 5（二次修复）：`timeGetCurrentTime` 取 RTC 墙钟在 sysmodule 睡眠/唤醒后偶发失败 → 仍卡 30 分钟
- **现象**：v1.3.7-restfix 已改用绝对墙钟结束时间，但用户实测休息一段时间后**仍卡在 30 分钟**、计时逻辑“没跑”。
- **根因**：`nowNs()` 用 `timeGetCurrentTime(TimeType_LocalSystemClock)` 读 `time` 服务。sysmodule 走 boot2、在系统灭屏睡眠再唤醒后，`time` 服务会话偶发返回 **0 或冻结值**（会话陈旧/缓存未刷新），于是 `now(0) < restEndTs_` 永远成立，休息倒计时永不结束 → 卡在 30 分钟，亮屏后照关游戏。
- **正确做法（v1.3.8）**：`nowNs()` 改用 **ARM 系统节拍计数器**（内核 syscall，不依赖任何系统服务）：
  ```cpp
  u64 nowNs() {
      const u64 ticks = armGetSystemTick();          // cntvct_el0, always-on 硬件计时器
      return (ticks * 1000000000ULL) / 19200000ULL;  // Tegra X1/X2 = 19.2MHz
  }
  ```
  - 该计数器由常驻(always-on)硬件计时器驱动，**灭屏睡眠也持续前进**；
  - 是内核 syscall，**不依赖 `time` 服务**，睡眠/唤醒不会失败或冻结 → 彻底消除卡死。
  - 配套诊断日志：休息期间每轮输出 `[Monitor] Forced rest: N min left (now=... restEnd=...)`，可直接从 `sysmodule.log` 确认倒计时在跑、到点解封。
  - ⚠️ **注意**：`today()`（每日统计用日期）仍依赖 `time` 服务，保持 `static bool timeReady` 只初始化一次（坑 8）；但**休息倒计时不要再依赖 `time` 服务**。

### ❌ 坑 6：Overlay 只在启动时探测一次服务 → 缓存 false 后永不自愈
- **正确做法**：`MainMenuPanel::update()` 周期性重探测 `connectToService()`，服务上线即翻转 `is_available` 并重绘。该模式（v1.3.12）保留，回退到 1.3.7 后若仍要消除 not installed，需把这段逻辑移植回去。

### ❌ 坑 7：sysmodule 没输出任何日志 = 它根本没在跑
- **现象**：“sysmodule 根本没有输出日志”。
- **排查**：sysmodule 日志在 `/config/parental_control/sysmodule.log`。若**完全无日志**，说明 sysmodule 未启动/未安装，而非逻辑 bug：
  - `atmosphere/contents/4200000000003103/exefs.nsp` 是否就位；
  - `atmosphere/contents/4200000000003103/flags/boot2.flag` 是否存在（空文件）；
  - title_id 是否为 `0x4200000000003103`。

### ❌ 坑 9：UltraHand 通知弹窗——字号被钳、弹窗窄、文本被裁、文件累积
- **现象**：休息时间提示“有时不显示”；提示文本“字符不全/缺少结尾”。
- **根因（读 `libultrahand` 源码确认）**：
  1. **字号被钳**：通知读取端把 `font_size` `clamp(1,34)`（tesla.hpp:12851），我们设的 44 实际只到 34。
  2. **弹窗固定 448×88px、单行、scissor 裁切**：文本只按 `\n` 换行、不自动换行；超出 448px 宽度的部分被裁掉。`"Forced rest: 30 min left. Game closed."`（~38 字符≈720px）被裁掉尾巴 → “感觉有缺少”。
  3. **弹窗仅显示 2.5s**（durationMs=2500），且 sysmodule 每 1 分钟循环才写一次 `.notify`；灭屏睡眠时线程挂起不写。亮屏瞬间若错过这 2.5s/不在循环边界，就“看不到”→ 偶发不显示。
  4. **`.notify` 文件不自动删**：UltraHand 显示后只记入 `shownFiles`、不删文件，长期运行目录堆积成百上千个文件，拖慢其 300ms 轮询 → 进一步加剧“偶发不显示”。
- **正确做法（v1.3.9）**：
  - 休息提示文案缩短为单行 ≤ ~22 字符（`"Rest: N min left"` / `"Rest until daily reset"`），完整落在 448px 内不被裁。
  - 进入休息时**立即**写一条提示（不再等下一个 1 分钟循环）；休息守卫内轮询间隔从 1 分钟降到 **15 秒**，提示更频繁、到点更快解封。
  - `writeNotification` 写新文件前**删除上一条** `.notify`，目录只留最新一个，避免堆积拖慢轮询。
  - ⚠️ **注意**：想让提示更大/更久，不能只靠 sysmodule 侧——字号被 UltraHand 钳到 34、时长固定 2.5s，要改需动 `libultrahand`（不在本仓库）。
  - 只要 sysmodule 不运行，任何限玩/休息逻辑都不会生效。

### ❌ 坑 9（续）：全量审计其余 toast 文案 → 还有 3 条会裁切（v1.3.10）
- **现象**：v1.3.9 只修了「休息中」一条；用户要求“check 其它场景字符显示不全”。
- **审计方法（可复现）**：用仓库内 `libultrahand/libtesla/include/stb_truetype.h`，按设备同款公式
  `像素宽 = Σ(字形 xAdvance × ScaleForPixelHeight(font, fontSize))` 测量每条文案。
  用 **DejaVuSans（比 Switch 共享字体更宽）做保守上界**：DejaVu 下 ≤ 448px 即真机必不裁。
- **实测结果（font=34，弹窗 448px）**：

  | 提示 | 原文案 | 宽度 | 结论 |
  |---|---|---|---|
  | 时间到(日额度耗尽) | `Time's up! The game will close.` | 461 | ❌裁切 |
  | 单次到时·有休息 | `Session time up! Rest N min.` | 439 | ⚠️临界(98%) |
  | 单次到时·无休息 | `Session time up! Paused until daily limit resets.` | 695 | ❌严重裁切 |
  | 休息结束 | `Break over - you can play now.` | 454 | ❌裁切 |
  | 监控已启用 | `Parental Control enabled` | 300 | ✅放得下 |
  | 剩余时间 | `0h 54mn left` | 155 | ✅ |
  | 休息中 | `Rest: N min left` / `Rest until daily reset` | 243/302 | ✅(v1.3.9已修) |

- **正确做法（v1.3.10）**：把 4 条超长文案统一缩短（实测 ≤ 348px，余量 ~100px）：
  - `Time's up! The game will close.` → `Time up! Closing game.`
  - `Session time up! Rest N min.` → `Time up! Rest N min.`
  - `Session time up! Paused until daily limit resets.` → `Time up! Daily limit.`
  - `Break over - you can play now.` → `Break over! Play now.`
  - 文本在弹窗内**居中绘制**，超出部分 scissor 裁掉**两端**（不单末尾），故临界文案也一并缩短留余量。

### ❌ 坑 10（P0）：休息计时 `nowNs()` 纳秒换算 u64 溢出 → 剩余时间撑大 / 休息无限拉长（v1.3.11）
- **现象**：设 `rest_duration=5` 分钟，却提示「rest 剩余 9 分钟」；且休息被拖成十几分钟才解封。
- **日志特征**：`now` 在某次读取突然从几百秒倒退到十几秒（如 `435,541,724,791` → `23,745,264,055`），而 `restEndTs_` 是固定值 → `remaining = restEndTs_ - now` 被撑大。
- **根因**：`nowNs()` 用 `return (ticks * 1000000000ULL) / 19200000ULL;`。`cntpct_el0`(19.2MHz) 开机约 **16 分钟**时 `ticks≈1.84e10`，`ticks*1e9≈1.84e19` 超过 `u64` 最大值 `2^64≈1.84e19` → **乘法溢出回绕**，`now` 变小。此后 `now` 要再爬回 `restEndTs_` 才解封，实际休息被大幅拉长。任何「开机>16 分钟时进入休息」都触发——Switch 几乎常开，故为**必现 P0**。
- **澄清**：`armGetSystemTick()` 读 `cntpct_el0` 物理计数器，本身单调递增、睡眠也前进（v1.3.8 选它是对的），**硬件无问题**，纯属软件整数溢出。
- **正确做法（v1.3.11）**：「先除后乘 + 余数补偿」避免溢出：
  ```cpp
  const u64 ticks = armGetSystemTick();
  constexpr u64 freq = 19200000ULL;
  const u64 sec  = ticks / freq;
  const u64 frac = ticks % freq;
  return sec * 1000000000ULL + (frac * 1000000000ULL) / freq;
  ```
  独立程序验证：旧公式 961s(16min) 溢出，新公式 60s~1 年全部正确且跨回绕点单调。
- 附带：休息守卫轮询 15s → 30s（用户要求，更省电、弹窗不频繁）。

### ❌ 坑 11：单次时长 `sessionElapsedMin_` 全局计数跨用户串台 → 下一个用户被提前锁定（v1.3.12）
- **现象**：两个用户日限额均 30、单次 15、休息 30。用户 A 日用满后，用户 B 刚开始玩就被提示需要休息/锁定。
- **根因**：`sessionElapsedMin_` 是 Monitor 全局成员（表示「整机连续游玩」已累计分钟），重置点只有「触发强制休息」「休息结束」两处。而**日限额耗尽分支未清零它**：用户 A 日用满（走日限额分支、未走休息分支）→ 计数残留 ~15 → 串到用户 B → B 没玩满 15 就撞 `sessionLimit` → 误判强制休息。日限额本身是**按 uid 各自计算**（正确），出问题的是这个全局「单次已玩」计数。
- **设计取舍（关键，避免引入绕过）**：
  - ❌ 不可「uid 变化就归零」：小孩来回切账号把计数反复清零，绕开「玩15分休息30分」。
  - ❌ 不可「关游戏就归零」：玩14分→退回HOME关掉→再开→清零，是更easy的绕过。
  - ✅ 保持「整机连续游玩」语义：A 玩14切到 B、B 再玩1，仍累计到15→强制休息；切账号/关游戏都不能白嫖休息；日限额按 uid 各自封顶，不会“一直可以玩”。
- **正确做法（v1.3.12）**：**仅在日限额耗尽分支补一行 `sessionElapsedMin_ = 0;`**，断掉跨用户串台。修复后 A 日用满→清零→B 从0玩满15分才休息。

### ❌ 坑 8：`timeInitialize()` 每次调用不退出 → time 服务 session 耗尽
- **现象**：`today()` 返回空串，统计静默停止（进程不崩）。
- **根因**：每次调用 `today()` 都 `timeInitialize()` 而不 `timeExit()`，session 耗尽后 `timeInitialize()` 失败。
- **正确做法**：`today()` 内用 `static bool timeReady` 只初始化一次（已修）。新增时间相关代码不要重复 `timeInitialize()`。

---

## 三、当前状态（基于 1.3.7 新分支 `fix/rest-time`）
1. ✅ **休息时间计算 bug（坑 5 及二次修复）已修**：先改为绝对墙钟结束时间，再因 `time` 服务在睡眠/唤醒后偶发失败，改为 ARM 系统节拍计数器（`armGetSystemTick`）。已发布 `restfix-v1.3.8`。
2. ✅ **UltraHand 通知修复（坑 9 + 续）**：v1.3.9 修“偶发不显示 + 休息提示裁切 + .notify 堆积”；v1.3.10 全量审计其余 toast，把 3 条（临界 1 条）超长文案缩短防裁切。
3. ✅ **休息计时溢出修复（坑 10, P0）**：v1.3.11 修 `nowNs()` 的 u64 溢出（开机>16 分钟进入休息会撑大剩余时间、拉长休息），并休息轮询 15s→30s。
4. ✅ **单次时长跨用户串台修复（坑 11）**：v1.3.12 在日限额耗尽分支补 `sessionElapsedMin_=0`，断掉跨用户残留（并明确不在切账号/关游戏时清零，避免引入绕过）。最新 Release = `restfix-v1.3.12`。
5. ⏸️ `main` 保持 v1.3.7（干净回退基线）；`archive/attempts-v1.3.8-v1.3.12` 保留有问题的尝试线，供复盘。
6. not installed：1.3.7 在用户硬件上“服务可用但晚注册”，若仍要彻底消除，需把 v1.3.12 的 Overlay 重探测逻辑移植回来（坑 2 + 坑 6）。
7. 部署前确认 sysmodule 真的在跑（坑 7），否则一切限玩逻辑无效。
