# NSParentalControl v1.3.5-fix 安装说明

## 本次修复：放弃全屏超时界面，超时直接终止游戏

实测 v1.3.3 / v1.3.4 在你这台机器（Atmosphère 1.10.3 / FW 21.2.0）上**都无法渲染全屏
"时间到"界面**：

| 版本 | 帧缓冲分配方式 | 结果 |
|---|---|---|
| v1.3.1 / v1.3.2 | `aligned_alloc`（512KB 进程堆） | 失败：`Could not allocate 1966080 bytes` |
| v1.3.3 | `tmemCreate`（背衬仍来自进程堆） | 失败：`rc=345:2` |
| v1.3.4 | `svcSetHeapSize`（运行时扩展进程堆） | 失败：`rc=1:101`（内核拒绝，sysmodule 上下文下不允许） |

1.9MB 全屏帧缓冲在这套 sysmodule 内存环境里**根本分配不出来**。继续死磕渲染是死路。

**v1.3.5 换思路**：时间到不再渲染画面，而是——
1. 弹一条**轻量系统通知**（toast，不需要帧缓冲）：`"Time's up! The game will close."`；
2. 短暂等待约 2 秒让提示显示；
3. **直接终止前台游戏**（`pmshellTerminateProgram(titleId)`），用户回到 HOME Menu。

这样**彻底绕开帧缓冲 / nvmap / svcSetHeapSize 的所有内存问题**，且限玩依然生效
（重新打开游戏后，monitor 检测到当日已用尽仍会再次终止）。

> 此前的统计（v1.3.1 time 句柄泄漏修复）、开机安全（512KB 静态堆 + boot2.flag）、
> 剩余时间提示（v1.3.x `notifyRemainingTime`）**全部保留**。

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

1. **开机不再崩溃**（维持 512KB 静态堆）。
2. overlay 显示 "Parental Control is Enabled"。
3. 运行 1 小时以上，统计持续累加（v1.3.1 修复）。
4. **设一个很短的每日限额（如 1 分钟），达到后应看到系统弹出 "Time's up! The game will close."，随后游戏被关闭、回到 HOME Menu**——这就是限玩生效。
   - 若成功，sysmodule.log 应出现 `[Monitor] Timeout for the user <昵称>` 紧接
     `[Helpers] Trying to terminate the current title` 与 `[Helpers] Rebooting...` 之类日志
     （或 pmshell 终止成功的记录）。
   - 若游戏**没被关闭**，日志应出现 `[Helpers] Could not terminate the title with titleId ...`，
     请把新日志发我。
5. 开启 DEBUG 日志级别，应每 10 分钟出现心跳 `[Monitor] Heartbeat: monitoring still active (loop #N)`。

默认 PIN：按 **A A A A**（四次 A 键）
