# NSParentalControl v1.3.1-fix 安装说明

## 本次修复：统计一段时间后停止（time 服务句柄泄漏）

上一个版本（v1.3-fix）能正常开机、UI 也不再崩溃，但运行一段时间（约数十分钟）后
**统计不再累加**。根因是 `helpers.cpp` 的 `today()` 每次调用都 `timeInitialize()`
却从不 `timeExit()`，每分钟泄漏 2 个 `time:` 服务 session（monitor 循环每轮调用
`today()` 两次）。session 池耗尽后 `timeInitialize()` 失败 → `today()` 返回空串 →
`addToHistory()` 提前返回，统计静默停止（sysmodule 进程本身并未崩溃）。

v1.3.1 将 `time` 服务改为**只初始化一次**（惰性 static），彻底消除泄漏，并新增
**每 10 分钟一条 INFO 心跳日志**，便于确认 sysmodule 是否仍在运行。

> 注：本次崩溃日志 `01783012165_0100000000000023.log` 是 **HOME Menu（qlaunch,
> 0100000000000023）** 的崩溃报告，与 sysmodule（4200000000003103）无关，无法据此
> 判断 sysmodule 是否崩溃。详见下方"关于上传的崩溃日志"。

## ⚠️ 重要：必须使用 boot2.flag

**Atmosphère 只支持 `boot2.flag`，不存在 `boot.flag`。** 本版本已修复所有导致 boot2
阶段崩溃的 bug，可安全使用 `boot2.flag`。

## 安装步骤

### 1. 删除旧数据（重要！）

如果 SD 卡上存在以下文件，**建议删除**（旧 sessions.json 可能损坏，新版本重新生成）：
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

确保 SD 卡上存在：
```
SD:/atmosphere/contents/4200000000003103/flags/boot2.flag
```
**不是 `boot.flag`，必须是 `boot2.flag`！**

### 4. 重启 Switch

重启后 sysmodule 会自动加载。

## 验证

1. 打开 Tesla/Ultrahand overlay，应看到 "Parental Control is Enabled"（而非 "not installed"）。
2. 玩一个游戏约 1~2 分钟，然后通过 overlay 查看今日已用时长，应随游戏时间增长。
3. 让机器运行 1 小时以上，统计应**持续累加**（此前会在约数十分钟后停止）。
4. 若开启 DEBUG 日志级别，sysmodule.log 中应每 10 分钟出现一条
   `[Monitor] Heartbeat: monitoring still active (loop #N)`。

默认 PIN：按 **A A A A**（四次 A 键）

## 关于上传的崩溃日志

`01783012165_0100000000000023.log` 文件名中的 `0100000000000023` 是 **HOME Menu
（qlaunch）** 的 title_id，因此这是 HOME Menu 的崩溃报告，**不是** parental control
sysmodule（4200000000003103）的。该日志只含 qlaunch 自身模块，无法证明 sysmodule 是否崩溃。

如需进一步排查 qlaunch 崩溃，请提供：
- `SD:/config/parental_control/sysmodule.log`（sysmodule 自身日志，最重要——可见最后心跳与是否报 time 错误）
- `SD:/atmosphere/crash_reports/` 中文件名含 `4200000000003103` 的日志（若有，说明 sysmodule 自身崩溃）
- 崩溃发生时是否正在打开 parental_control overlay
