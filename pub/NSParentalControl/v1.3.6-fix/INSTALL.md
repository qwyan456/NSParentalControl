# NSParentalControl v1.3.6 安装说明

## 本次新增：单次最长可玩时间 + 强制休息（循环可玩）

在 v1.3.5（超时即弹通知并终止游戏、回到 HOME Menu）的基础上，新增两层节奏控制：

1. **单次最长可玩时间（Session limit）**：连续玩游戏累计达到该时长，立即终止前台游戏并进入「强制休息」。
2. **强制休息时长（Rest duration）**：休息倒计时内，**任何游戏一启动就被立即强制关闭**（全局冷却）；
   倒计时结束且当日额度未耗尽后，恢复正常游玩，单次计时清零，**可再次循环**。

循环示意：

```
游玩(累计单次) ──达到单次上限──▶ 终止游戏 + 进入休息(rest 倒计时递减)
                                      │ 休息中若有游戏启动 → 立即终止
                                      ▼ rest 倒计时归零
                          当日额度未耗尽？──是──▶ 继续游玩（单次计时归零，循环）
                                      │否
                                      ▼ 当日额度耗尽 → 永久封锁（原有每日限制逻辑）
```

### 设置方式（Overlay）

在 Tesla overlay → Parental Control → 设置限制（Set limits）面板中，除原有的「Daily」外，
新增两行可编辑项：

| 行 | 含义 | 设为 0 时 |
|---|---|---|
| **Session** | 单次最长可玩时间（时/分） | 关闭单次限制（仅每日限制生效，行为与 v1.3.5 一致） |
| **Rest** | 强制休息时长（时/分） | 触发单次上限后不进入倒计时冷却，游戏被终止后任何重开都会再次被立即终止，直到当日额度耗尽才解封 |

用 ← → 切换「时/分」字段，↑ ↓ 调整数值；按 **B** 返回即保存（与每日限制一同保存）。

> 设置作用范围：**全局统一**（所有用户共用同一套单次/休息时长），与「每日限制」按用户分别设置不同。

### 行为边界

- 单次计时精度为 1 分钟（监控循环周期）。
- 当日额度耗尽（每日限制触发）优先于单次休息：当天游玩结束，不再进入临时休息。
- 休息期间对所有游戏生效（全局冷却），不区分具体是哪款游戏。
- 受 sysmodule 512KB 堆限制，休息结束**不**每轮弹通知，仅在「进入休息」「休息结束」各提示一次：
  - 进入休息：`"Session time up! Rest N min, then you can play again."`
  - 休息结束：`"Break over - you can play now."`

> v1.3.5 的全部修复（放弃全屏渲染、toast + 终止游戏、512KB 静态堆 + boot2.flag、剩余时间提示、
> time 句柄泄漏修复、统计持续累加）**全部保留**。

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

1. **开机不再崩溃**（维持 512KB 静态堆）。
2. overlay「设置限制」面板应能看到并保存 **Session / Rest** 两行；重启 sysmodule 后设置保留。
3. 设一个很短的单次时长（如 2 分钟）+ 休息（如 1 分钟）：
   - 连续游玩触发 toast 并终止游戏；
   - 休息期间重开游戏被立即终止；
   - 休息结束后可继续，且能再次循环。
4. 设一个很短的每日限额：达到后进入永久封锁，不再进入临时休息。
5. 开启 DEBUG 日志，应每 10 分钟出现心跳 `[Monitor] Heartbeat: monitoring still active (loop #N)`。

默认 PIN：按 **A A A A**（四次 A 键）
