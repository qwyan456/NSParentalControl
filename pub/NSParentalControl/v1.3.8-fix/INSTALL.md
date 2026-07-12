# NSParentalControl v1.3.7 安装说明

## 本次优化：限玩提示更醒目、显示更久（UX 改进）

v1.3.6 的限玩提示走 UltraHand 轻量通知（toast），但存在两个问题：字号偏小、显示时间偏短。
本版本针对这两点做了改进：

| 改进点 | v1.3.6 | v1.3.7 |
|---|---|---|
| 通知字号 | 默认 28 | **44**（更大更醒目） |
| 通知优先级 | 默认 1 | **5**（更高，优先显示） |
| 显示时长 | 约 2 秒（受 UltraHand 默认限制） | **约 4 秒**：触发时**重复发送 3 次、间隔 1.5 秒**，把可见时间拉长 |
| 强制休息中被关游戏的反馈 | 仅进入休息时提示一次 | 休息期间每次强行开游戏被关，补一条 **「Forced rest: N min left. Game closed.」**，说明被关原因与剩余休息时长 |

> 说明：UltraHand 的 `.notify` 通知**不支持直接设置显示时长**，因此 v1.3.7 用「重复写入」的方式
> 让其持续重新显示，从而把可见时间从约 2 秒延长到约 4 秒。如需更长，可继续加大重复次数/间隔
> （见 `notifications_controller.cpp` 的 `notifyWithEmphasis`）。

涉及的通知：
- 每日额度耗尽：`"Time's up! The game will close."`（重复、放大、停留更久）
- 单次时长达到：`"Session time up! Rest N min."` / `"Session time up! Paused until daily limit resets."`
- 休息结束：`"Break over - you can play now."`
- 休息中被关：`"Forced rest: N min left. Game closed."`

其余（单次时长 + 强制休息的循环逻辑、全局设置、Overlay 配置入口、512KB 堆 + boot2.flag、
剩余时间提示、统计等）**与 v1.3.6 完全一致**，详见 v1.3.6 的 INSTALL 说明。

## ⚠️ 重要：必须使用 boot2.flag

Atmosphère 只支持 `boot2.flag`。本版本 inner heap 维持 512KB，可安全使用 `boot2.flag`。

## 安装步骤

### 1. 删除旧数据（建议，首次升级时）

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

1. 设一个很短的单次时长（如 2 分钟）+ 休息（如 1 分钟），连续游玩触发时应看到**更大、重复出现、
   停留更久**的提示，随后游戏被关闭。
2. 休息期间强行重开游戏，应看到「Forced rest: N min left. Game closed.」提示，且游戏被立即关闭。
3. 设很短的每日限额，达到后提示更醒目、停留更久（约 4 秒）。
4. 开启 DEBUG 日志，应每 10 分钟出现心跳 `[Monitor] Heartbeat: monitoring still active (loop #N)`。

默认 PIN：按 **A A A A**（四次 A 键）
