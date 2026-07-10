# NSParentalControl 崩溃修复报告

## 环境
- Atmosphère 1.10.3 / FW 21.2.0
- 崩溃现象：开机 crash（已通过 boot2.flag → boot.flag 修复）；UI 调用时 crash

## 修复清单

### P0：saveDatabase() snprintf 截断 bug（UI 崩溃的直接原因）
- **文件**: `sysmodule/source/database/database.cpp`
- **问题**: `snprintf(buf, lenstr, "%s", str)` 只写入 `lenstr-1` 个字符，JSON 最后一个 `}` 被 `\0` 替换。下次 `loadDatabase()` 读取时 `json::parse()` 抛出异常，在 `-fno-exceptions` 下调用 `std::terminate()` → 进程崩溃
- **修复**: 直接使用 `std::string::c_str()` 写入，移除 `malloc/memset/snprintf/free`

### P0：-fno-exceptions 下 json::parse() 崩溃
- **文件**: `database.cpp`, `helpers.cpp`
- **问题**: 项目使用 `-fno-exceptions` 编译，但 `nlohmann::json::parse()` 在遇到无效 JSON 时会抛出异常，导致 `std::terminate()` → 崩溃
- **修复**: 在所有 `json::parse()` 调用前添加 `json::accept()` 预检查

### P0：-fno-exceptions 下 std::stoull() 崩溃
- **文件**: `sysmodule/source/helpers.cpp`
- **问题**: `std::stoull()` 在输入无效时抛出 `std::invalid_argument` 或 `std::out_of_range`
- **修复**: 改用 `strtoull()` + `errno` 检查

### P1：__appInit() 末尾错误的 smExit()
- **文件**: `sysmodule/source/main.cpp`
- **问题**: `__appInit()` 末尾调用 `smExit()` 关闭了 sm session，导致后续 `Ipc::Server` 构造函数中 `smRegisterService()` 失败
- **修复**: 删除 `smExit()`

### P1：__wrap_exit() 中的 smExit()
- **文件**: `sysmodule/source/main.cpp`
- **问题**: `__wrap_exit()` 中调用 `smExit()` 可能在 sm 已被关闭时导致双重释放
- **修复**: 移除 `smExit()`

### P1：Server 构造/析构中冗余的 smInitialize/smExit
- **文件**: `sysmodule/source/ipc/Server.cpp`
- **问题**: `Server` 构造函数重复调用 `smInitialize()`，析构函数调用 `smExit()` 可能影响其他线程
- **修复**: 移除冗余调用

### P1：rebootToPayload() 中冗余的 smInitialize/smExit
- **文件**: `sysmodule/source/helpers.cpp`
- **问题**: `rebootToPayload()` 中调用 `smInitialize()` 和 `smExit()`，关闭 sm 会影响其他线程
- **修复**: 移除冗余调用

### P1：renderer.hpp 中冗余的 smInitialize/smExit
- **文件**: `sysmodule/source/gui/renderer.hpp`
- **问题**: `Renderer::init()` 中重复调用 `smInitialize()` 和 `smExit()`
- **修复**: 移除冗余调用

### P2：loadDatabase() 缺少 "history" 键检查
- **文件**: `sysmodule/source/database/database.cpp`
- **问题**: `j_data["history"]` 在键不存在时会创建 null 值，`.get<std::vector<json>>()` 会抛出异常
- **修复**: 添加 `contains()` 和 `is_array()` 检查

## 修改的文件列表
1. `sysmodule/source/database/database.cpp` — saveDatabase 修复 + 安全 JSON 解析
2. `sysmodule/source/helpers.cpp` — strtoull 修复 + 安全 JSON 解析 + sm 调用修复
3. `sysmodule/source/main.cpp` — __appInit/__wrap_exit smExit 修复
4. `sysmodule/source/ipc/Server.cpp` — smInitialize/smExit 修复
5. `sysmodule/source/gui/renderer.hpp` — smInitialize/smExit 修复

---

## v1.3.1-fix：统计一段时间后停止（time 服务句柄泄漏）

### P0：today() 每次调用都 timeInitialize() 而不 timeExit() → 句柄泄漏 → 统计静默停止
- **文件**: `sysmodule/source/helpers.cpp`（`today()`）
- **问题**: monitor 循环每轮通过 `addToHistory()` 和 `getUserUsageTimeForToday()` 共调用 `today()` **两次**，每次都 `timeInitialize()` 打开一个 `time:` 服务 session 却从不 `timeExit()`。Switch 的 time 服务 session 数量有限，耗尽后 `timeInitialize()` 失败，`today()` 返回 `""`，`addToHistory()` 因 `date.empty()` 提前返回 → **统计停止累加**。sysmodule 进程本身不崩溃（循环仍在跑），只是不再计时。
- **表现**: 开机/UI 正常，运行约数十分钟后时长不再增长（与用户反馈"统计一段时间后就不再统计"完全吻合）。
- **修复**: `today()` 改为惰性 `static bool timeReady`，只在首次调用时 `timeInitialize()` 一次，之后复用该 session，不再泄漏。
- **附加**: `monitor.cpp` 新增每 10 分钟一条 INFO 心跳日志 `[Monitor] Heartbeat: monitoring still active (loop #N)`，便于确认 sysmodule 是否存活。

### 关于上传的崩溃日志 `01783012165_0100000000000023.log`
- 文件名中的 `0100000000000023` 是 **HOME Menu（qlaunch）** 的 title_id，因此这是 qlaunch 的崩溃报告（错误 2001-0132 / 0x10801，User Break），**不是** parental control sysmodule（4200000000003103）。
- 该日志仅包含 qlaunch 自身模块，无法据此判断 sysmodule 是否崩溃；与"统计停止"是两个独立问题（统计停止由上面的 time 句柄泄漏导致，sysmodule 并未崩溃）。
- 如需排查 qlaunch 崩溃，需提供：`/config/parental_control/sysmodule.log`、`/atmosphere/crash_reports/` 中标题含 `4200000000003103` 的日志（若有）、以及崩溃时是否正打开 parental_control overlay。

### v1.3.1 修改的文件
1. `sysmodule/source/helpers.cpp` — `today()` 改为只初始化一次 time 服务（修漏）
2. `sysmodule/source/monitor.cpp` — 新增心跳日志
3. `sysmodule/Makefile` — APP_VERSION 1.3 → 1.3.1

---

## v1.3.2-fix：超时界面无法显示（静态堆 512KB 太小）

### P0：showScreenTimeout() 分配 ~1.9MB 帧缓冲失败（512KB 静态堆不足）
- **文件**: `sysmodule/source/main.cpp`（`INNER_HEAP_SIZE`）
- **问题**: 修改前日志 `sysmodule.log` 暴露：用户达到每日限额时，`[Monitor] Timeout for the user ...` → `service_->showScreenTimeout()` 尝试分配 **1966080 字节(≈1.9MB, 0x1E0000)** 的全屏帧缓冲，但静态 inner heap 仅 **512KB(0x80000)**，分配失败：
  ```
  [Screen timeout] Could not allocate 1966080 bytes of memory
  [Screen] The framebuffer pointer is null. Aborting.
  [Screen] PrepareScreenForDrawing failed
  ```
  超时界面画不出来（sysmodule 自身不崩溃，错误被捕获处理，但用户看不到"时间到"提示）。
- **修复**: `INNER_HEAP_SIZE` 512KB → **4MB(0x400000)**。boot 崩溃根因是 init 顺序(sm/fs)，与堆大小无关，扩容安全。
- **附带结论（来自修改前 4 个日志）**:
  - `sysmodule.log` 全程为正常 IPC 活动，**sysmodule 当时存活、未崩溃**；统计当时正常(usage=7/4 分)。→ 印证"统计停止"是后续 time 句柄泄漏耗尽 session 所致（v1.3.1 已修），且此前那份 qlaunch 崩溃日志(0100000000000023)与 sysmodule 无关。
  - `sessions.json` / `settings.json` 均为**合法 JSON、无损坏**。
  - `overlay.log` 连接/查询/退出均干净，**overlay 非崩溃源**；`[IPC] Couldn't receive request (closing handle): 62977` / `Received unexpected CmifCommand (2)` 为 overlay 断开/IPC 版本差异的良性噪声。

### v1.3.2 修改的文件
1. `sysmodule/source/main.cpp` — `INNER_HEAP_SIZE` 512KB → 4MB（修超时界面内存分配失败）
2. `sysmodule/Makefile` — APP_VERSION 1.3.1 → 1.3.2

> ⚠️ **修正**：v1.3.2 当时假设"boot 崩溃根因是 init 顺序，与堆大小无关，扩容安全" ——
> **这个假设是错的**。4MB 静态 BSS 在 boot2 阶段抢占了 qlaunch 的常驻内存，正是 v1.3.3
> 要修的开机崩溃根因。详见下方 v1.3.3。

---

## v1.3.3-fix：v1.3.2 的 4MB 静态堆触发 boot2 开机崩溃（回归修复）

### P0：INNER_HEAP_SIZE 4MB 的静态 BSS 在 boot2 阶段饿死 qlaunch → 开机崩溃
- **文件**: `sysmodule/source/main.cpp`（`INNER_HEAP_SIZE`）
- **问题**: v1.3.2 把 `INNER_HEAP_SIZE` 从 512KB 扩到 4MB，静态 `nx_inner_heap[0x400000]`
  BSS 段随之变大。sysmodule 走 `boot2.flag` 在 **qlaunch（0100000000000023）之前** 被加载，
  这份 4MB 静态 BSS 在 boot2 阶段就常驻，挤占了 qlaunch 启动所需的常驻内存 →
  开机崩溃（错误 2001-0132 / 0x10801，User Break）。
- **诊断依据**: 用户上传的新 `sysmodule.log`（481 字节）显示 sysmodule 自身启动完全正常，
  直到第 13 行 `[Monitoring] Parental control is now enabled.`，之后才崩溃 ——
  说明崩溃发生在 sysmodule 之外（qlaunch 侧），由内存压力引发，而非 sysmodule 故障。
- **修复**: 把两个需求**解耦**，不再用单一大静态堆同时满足：
  1. `INNER_HEAP_SIZE` **回退 512KB(0x80000)** —— boot2 阶段静态 BSS 回到安全大小；
  2. 超时界面的 ~1.9MB(0x1E0000) 帧缓冲改由 `sysmodule/source/gui/screen_timeout.cpp`
     通过 **TransferMemory**（`tmemCreate` + `tmemGetAddr`，运行时向系统申请的匿名 RAM）
     分配，而非从 inner heap `aligned_alloc`。TransferMemory 不占 boot2 静态 BSS，
     故开机安全，又足够大能放下全屏帧缓冲。
  3. 加"已分配则复用"守卫，避免每次超时重复申请造成句柄泄漏。
- **关键认知**: 之前误以为"512KB 堆不够 → 必须扩大静态堆"是唯一解法。其实帧缓冲可以走
  TransferMemory（系统运行时 RAM），与 boot2 静态堆是两个独立的内存池，不必互相挤占。

### v1.3.3 修改的文件
1. `sysmodule/source/main.cpp` — `INNER_HEAP_SIZE` 4MB → **回退 512KB**（修 boot2 开机崩溃）
2. `sysmodule/source/gui/screen_timeout.cpp` — 帧缓冲改走 TransferMemory（不再依赖大静态堆）
3. `sysmodule/Makefile` — APP_VERSION 1.3.2 → 1.3.3

---

## v1.3.4-fix：v1.3.3 的 TransferMemory 方案仍失败（超时界面画不出）

### P0：tmemCreate 的 backing 内存同样来自 512KB 进程堆 → 1.9MB 帧缓冲仍分配失败
- **文件**: `sysmodule/source/gui/screen_timeout.cpp`（`InitializeFrameBufferPointer`）
- **问题**: 用户实测 v1.3.3"时间到了却没出提示"。新日志（v1.3.3 构建）显示：
  ```
  [Screen timeout] Could not allocate 1966080 bytes of TransferMemory (rc=345:2)
  [Screen] The framebuffer pointer is null. Aborting.
  [Screen] Could not create Map          ← Error 345:11（nvMapCreate 拿到 null 的级联）
  [Screen] PrepareScreenForDrawing failed
  ```
  `rc=345:2` 是 libnx 模块错误。根因：libnx `tmem.h` 注释明确
  "the user process allocates and owns its backing memory"——`tmemCreate` 的内部
  backing buffer 来自**进程自己的堆**（sysmodule 即 512KB 静态 inner heap）。所以它和
  `aligned_alloc` 一样，在 512KB 堆上分配 1.9MB **必然失败**。v1.3.3 对 `tmemCreate`
  等于"系统 RAM"的假设是**错的**；真正的失败链与 v1.3.1 完全相同（只是错误文案从
  "Could not allocate ... memory" 变成 "Could not allocate ... TransferMemory"）。
- **修复**: 改用 `svcSetHeapSize` 在**运行时把进程堆扩展到系统内存**（非静态 BSS、非
  inner heap），从扩展区域顶部切出页对齐帧缓冲：
  ```cpp
  u64 cur_heap_size = 0;
  svcGetInfo(&cur_heap_size, InfoType_HeapRegionSize, CUR_PROCESS_HANDLE, 0);
  const u64 new_heap_size = util::AlignUp(cur_heap_size + fb_size, 0x200000); // 必须 2MB 倍数
  void* heap_end = nullptr;
  svcSetHeapSize(&heap_end, new_heap_size);
  g_framebuffer_pointer = static_cast<u8*>(heap_end) - fb_size;
  ```
  - `svcSetHeapSize` 的 size **必须是 0x200000(2MB) 的倍数**（已对齐），否则 SVC 失败；
  - 扩展发生在**首次需要超时界面时**（qlaunch 早已启动、内存充足），不动 boot2 阶段内存
    → **开机安全**（inner heap 维持 512KB 不变）；
  - `nvMapCreate(..., is_cpu_cacheable=true)` 使用我们提供的 `g_framebuffer_pointer` 作
    显存底背（该参数不是"由 nvmap 分配"），故有效指针即可正常渲染，不再级联 `Error 345:11`。
- **验证方式**: 因 exefs.nsp(NSO) 的只读数据段被压缩，`strings` 不可靠；改为在**未压缩的
  `pctrl.elf`** 中确认：新字符串 `svcSetHeapSize failed` / `svcGetInfo(HeapRegionSize)
  failed` / `Allocated %zu-byte framebuffer` / `framebuffer at %p (heap end` 均存在，
  旧的 `Could not allocate %zu bytes of TransferMemory` 已消失。

### v1.3.4 修改的文件
1. `sysmodule/source/gui/screen_timeout.cpp` — 帧缓冲改由 `svcSetHeapSize` 扩展的系统内存堆分配（修超时界面画不出）
2. `sysmodule/Makefile` — APP_VERSION 1.3.3 → 1.3.4
