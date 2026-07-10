# NSParentalControl v1.3.2-fix 安装说明

## 本次修复：超时界面无法显示（静态堆太小）

v1.3.1 修好了"统计停止"(time 服务句柄泄漏)。但分析修改前的日志时发现第二个 bug：
当用户**达到每日限额**时，`showScreenTimeout()` 需要分配约 **1.9MB(0x1E0000)** 的
全屏帧缓冲，而 sysmodule 的静态堆只有 **512KB**，分配失败 →

```
[Screen timeout] Could not allocate 1966080 bytes of memory
[Screen] The framebuffer pointer is null. Aborting.
[Screen] PrepareScreenForDrawing failed
```

超时界面画不出来（sysmodule 自身不崩溃，但用户看不到"时间到"提示）。

v1.3.2 将静态堆从 512KB 扩容到 **4MB(0x400000)**，足以容纳帧缓冲。
（boot 崩溃的根因是 init 顺序，与堆大小无关，扩容安全。）

> v1.3.1 的修复(time 句柄泄漏 → 统计停止)在本版本一并包含。

## ⚠️ 重要：必须使用 boot2.flag

**Atmosphère 只支持 `boot2.flag`，不存在 `boot.flag`。** 本版本已修复所有导致 boot2
阶段崩溃的 bug，可安全使用 `boot2.flag`。

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

1. overlay 显示 "Parental Control is Enabled"。
2. 让机器运行 1 小时以上，统计应**持续累加**（v1.3.1 修复）。
3. 设一个很短的每日限额（如 1 分钟），达到后**应能看到"时间到"全屏界面**（v1.3.2 修复，
   此前此处只会报内存分配失败、界面不显示）。
4. 开启 DEBUG 日志级别，sysmodule.log 中应每 10 分钟出现一条心跳
   `[Monitor] Heartbeat: monitoring still active (loop #N)`。

默认 PIN：按 **A A A A**（四次 A 键）

## 修改前日志分析结论（用户提供的 4 个文件）

| 文件 | 结论 |
|---|---|
| `sysmodule.log` | sysmodule **全程存活**（结尾为正常 IPC），未崩溃；统计当时正常(usage=7/4 分)；暴露超时界面内存分配失败(512KB 堆不足) |
| `sessions.json` | 合法 JSON，无损坏；usage 求和正确(7/4 分) |
| `settings.json` | 合法 JSON，无损坏 |
| `overlay.log` | overlay 连接/查询/退出均干净，**非崩溃源** |

→ 此前那份 qlaunch 崩溃日志(0100000000000023)与 sysmodule 无关，sysmodule 当时是活的。
