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

### ❌ 坑 5：休息时间用“循环每 1 分钟 -1”计数器 → 灭屏睡眠后算错（**本次要修**）
- **现象**：单次到时进入强制休息；灭屏（系统睡眠）超过设定休息时长后，亮屏打开游戏仍提示“休息时间没到”被关。
- **根因**：`restRemainingMin_--` 依赖监控循环每 1 分钟跑一次；**系统睡眠时监控线程被挂起，循环不执行，计数器冻结**。真实墙钟已超设定休息时长，但 `restRemainingMin_` 没减，于是仍判为“休息中”。
- **正确做法**：用**绝对墙钟结束时间**判断：
  ```cpp
  // 进入休息：restEndTs_ = nowNs() + (u64)restMin * 60 * 1'000'000'000;  // 纳秒
  // 守卫中： if (nowNs() >= restEndTs_) { 休息结束 }
  ```
  `nowNs()` 用 `timeGetCurrentTime(TimeType_LocalSystemClock, &ts)`（RTC 墙钟，**睡眠也前进**）。详见 fix/rest-time 分支的 `monitor.cpp`。

### ❌ 坑 6：Overlay 只在启动时探测一次服务 → 缓存 false 后永不自愈
- **正确做法**：`MainMenuPanel::update()` 周期性重探测 `connectToService()`，服务上线即翻转 `is_available` 并重绘。该模式（v1.3.12）保留，回退到 1.3.7 后若仍要消除 not installed，需把这段逻辑移植回去。

### ❌ 坑 7：sysmodule 没输出任何日志 = 它根本没在跑
- **现象**：“sysmodule 根本没有输出日志”。
- **排查**：sysmodule 日志在 `/config/parental_control/sysmodule.log`。若**完全无日志**，说明 sysmodule 未启动/未安装，而非逻辑 bug：
  - `atmosphere/contents/4200000000003103/exefs.nsp` 是否就位；
  - `atmosphere/contents/4200000000003103/flags/boot2.flag` 是否存在（空文件）；
  - title_id 是否为 `0x4200000000003103`。
  - 只要 sysmodule 不运行，任何限玩/休息逻辑都不会生效。

### ❌ 坑 8：`timeInitialize()` 每次调用不退出 → time 服务 session 耗尽
- **现象**：`today()` 返回空串，统计静默停止（进程不崩）。
- **根因**：每次调用 `today()` 都 `timeInitialize()` 而不 `timeExit()`，session 耗尽后 `timeInitialize()` 失败。
- **正确做法**：`today()` 内用 `static bool timeReady` 只初始化一次（已修）。新增时间相关代码不要重复 `timeInitialize()`。

---

## 三、当前待办（基于 1.3.7 新分支）
1. **修休息时间计算 bug**（坑 5）：用墙钟结束时间，见 `fix/rest-time` 分支。
2. not installed：1.3.7 在用户硬件上“服务可用但晚注册”，若仍要彻底消除，需把 v1.3.12 的 Overlay 重探测逻辑移植回来（坑 2 + 坑 6）。
3. 部署前确认 sysmodule 真的在跑（坑 7），否则一切限玩逻辑无效。
