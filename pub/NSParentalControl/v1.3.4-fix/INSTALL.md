# NSParentalControl v1.3.4-fix 安装说明

## 本次修复：超时界面仍画不出来（v1.3.3 的 TransferMemory 方案失败）

v1.3.3 用 `tmemCreate` 分配 ~1.9MB 帧缓冲，但用户实测"时间到了却没出提示"。
新日志（v1.3.3 构建）显示：

```
[Screen timeout] Could not allocate 1966080 bytes of TransferMemory (rc=345:2)
[Screen] The framebuffer pointer is null. Aborting.
[Screen] Could not create Map          ← Error 345:11（nvMapCreate 拿到 null 指针的级联）
[Screen] PrepareScreenForDrawing failed
```

**根因**：libnx 的 `tmemCreate` 的 backing 内存来自**进程自己的堆**（sysmodule 即 512KB
静态 inner heap，见 `tmem.h` 注释 "the user process allocates and owns its backing memory"）。
所以 `tmemCreate` 和 `aligned_alloc` 一样，在 512KB 堆上分配 1.9MB **必然失败**
（`rc=345:2` 是 libnx 模块错误）。我 v1.3.3 对 `tmemCreate` 的"系统 RAM"假设是**错的**。

**v1.3.4 的正确做法**：运行时用 `svcSetHeapSize` 把进程堆扩展到**系统内存**
（不是静态 BSS、不是 512KB inner heap），从扩展区域顶部切出页对齐的帧缓冲：

- `svcSetHeapSize` 的 size 必须是 **0x200000(2MB) 的倍数**——已对齐；
- 扩展只发生在**首次需要超时界面时**（此时 qlaunch 早已启动、内存充足），
  不触碰 boot2 阶段内存 → **开机仍然安全**（inner heap 维持 512KB 不变）；
- 帧缓冲指针有效后，`nvMapCreate(..., is_cpu_cacheable=true)` 拿到的是有效地址，
  不再级联 `Error 345:11`，"Your time is up!" 界面应能渲染。

> 注意：`nvMapCreate` 的最后一个参数是 `is_cpu_cacheable`（不是"由 nvmap 分配"），
> 它**使用我们提供的 `g_framebuffer_pointer` 作显存底背**——所以画进去的像素确实会被显示。

> v1.3.1（统计停止）、v1.3.2/v1.3.3（超时界面内存分配）的修复目标**一并保留**。

## ⚠️ 重要：必须使用 boot2.flag

Atmosphère 只支持 `boot2.flag`。本版本 inner heap 维持 512KB，可安全使用 `boot2.flag`。

## 安装步骤

### 1. 删除旧数据（建议）

```
SD:/config/parental_control/sessions.json
SD:/config/parental_control/settings.json
SD:/config/parental_control/sysmodule.log
```

### 2. 复制文件

| 源文件 | SD 卡目标路径 |
|---|---|
| `atmosphere/contents/4200000000003103/exefs.nsp` | `SD:/atmosphere/contents/4200000000003103/exefs.nsp` |
| `atmosphere/contents/4200000000003103/flags/boot2.flag` | `SD:/atmosphere/contents/4200000000003103/flags/boot2.flag` |
| `atmosphere/contents/4200000000003103/toolbox.json` | `SD:/atmosphere/contents/4200000000003103/toolbox.json` |
| `switch/.overlays/parental_control.ovl` | `SD:/switch/.overlays/parental_control.ovl` |

### 3. 确认 boot2.flag

```
SD:/atmosphere/contents/4200000000003103/flags/boot2.flag   ← 必须是 boot2.flag
```

### 4. 重启 Switch

## 验证

1. **开机不再崩溃**（维持 512KB 静态堆，与 v1.3.3 相同）。
2. overlay 显示 "Parental Control is Enabled"。
3. 运行 1 小时以上，统计持续累加（v1.3.1 修复）。
4. **设一个很短的每日限额（如 1 分钟），达到后应能看到"Your time is up!"全屏界面**
   （本次核心修复：帧缓冲改由 `svcSetHeapSize` 扩展的系统内存堆分配）。
   - 若成功，sysmodule.log 应出现 `[Screen timeout] Allocated 1966080-byte framebuffer at <addr> (heap end <addr>)`。
   - 若仍失败，日志会出现 `[Screen timeout] svcSetHeapSize failed (rc=...)` 或
     `[Screen timeout] svcGetInfo(HeapRegionSize) failed (rc=...)`——请把新日志发我。
5. 开启 DEBUG 日志级别，应每 10 分钟出现心跳 `[Monitor] Heartbeat: monitoring still active (loop #N)`。

默认 PIN：按 **A A A A**（四次 A 键）
