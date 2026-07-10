# NSParentalControl v1.3.3-fix 安装说明

## 本次修复：v1.3.2 的 4MB 静态堆触发 boot2 开机崩溃（回归）

v1.3.2 把静态堆从 512KB 扩到 **4MB**，用于容纳超时界面的 ~1.9MB 帧缓冲。
但 **这个假设错了**：4MB 的静态 BSS 在 boot2 阶段（sysmodule 被加载到 qlaunch/HOME Menu
之前）直接吃掉了 qlaunch（`0100000000000023`）所需的常驻内存，导致开机崩溃
（错误 2001-0132 / 0x10801，User Break）。

用户反馈："这个版本开机过程中，系统 crash 了"。上传的新 `sysmodule.log`（481 字节）
证明 sysmodule 自身启动完全正常，直到第 13 行

```
[Monitoring] Parental control is now enabled.
```

之后才崩溃 —— 这是 **qlaunch 侧** 的内存压力崩溃，不是 sysmodule 故障。

v1.3.3 的修复思路：**把两个需求解耦**，而不是用更大的静态堆去同时满足。

| 需求 | 旧方案（v1.3.2，崩溃） | 新方案（v1.3.3） |
|---|---|---|
| boot2 阶段常驻内存要小 | 静态堆 4MB（过大 → 抢 qlaunch 内存） | 静态堆 **回退 512KB**，boot2 安全 |
| 超时界面 ~1.9MB 帧缓冲 | 从 4MB 静态堆 `aligned_alloc` | 改用 **TransferMemory**（运行时系统 RAM，`tmemCreate`/`tmemGetAddr`） |

> TransferMemory 是运行时向系统申请的匿名内存，**不占 boot2 阶段的静态 BSS**，
> 因此开机安全；又足够大，能放下全屏帧缓冲。同时加了"已分配则复用"的守卫，
> 避免每次超时重复申请造成句柄泄漏。

> v1.3.1（time 句柄泄漏 → 统计停止）与 v1.3.2 的超时界面修复目标**一并保留**，
> 只是换了一种不破坏 boot 的方式实现。

## ⚠️ 重要：必须使用 boot2.flag

**Atmosphère 只支持 `boot2.flag`，不存在 `boot.flag`。** 本版本已把堆回退到 512KB，
可安全使用 `boot2.flag`。

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

1. **开机不再崩溃**（本次核心修复）：正常进入 HOME Menu，无 2001-0132 报错。
2. overlay 显示 "Parental Control is Enabled"。
3. 让机器运行 1 小时以上，统计应**持续累加**（v1.3.1 修复）。
4. 设一个很短的每日限额（如 1 分钟），达到后**应能看到"时间到"全屏界面**
   （帧缓冲改走 TransferMemory，512KB 静态堆不再成为瓶颈）。
5. 开启 DEBUG 日志级别，sysmodule.log 中应每 10 分钟出现一条心跳
   `[Monitor] Heartbeat: monitoring still active (loop #N)`。

默认 PIN：按 **A A A A**（四次 A 键）
