# NSParentalControl v1.3.9 安装说明

## 本次修复：解决 Overlay 提示 “Parental Control is not installed”

此前版本在开机后 Overlay 长期显示 “not installed”，根因是 **sysmodule 在开机后约 32 秒才注册 `pctrl` 服务**（期间一直在等待 qlaunch / ns 就绪），
而 Overlay 在启动瞬间就探测服务是否存在，于是误报“未安装”。同时旧版 NPDM 携带了 16MB 的 `system_resource_size`（boot2 阶段内存紧张，存在启动风险），
且与可稳定运行的参考 sysmodule 实现不一致。

v1.3.9 已对齐**成熟可运营 sysmodule 的标准实现**，彻底修复：

| 项目 | v1.3.8（有问题） | v1.3.9（已修复） |
|---|---|---|
| 服务注册时机 | 等 qlaunch/ns 就绪后才注册（约 32s 空窗） | **开机瞬间立即注册** `pctrl` 服务 |
| 堆内存 | 16MB `system_resource_size`（boot2 风险） | **动态堆 `svcSetHeapSize` 2.5MB**（boot2 安全，参考实现） |
| `main_thread_priority` | 49 | **15**（与参考实现一致） |
| `force_debug` | true | **false**（零售机可正常启动） |
| 全屏超时提示 | 保留（RGBA_4444，约 1.9MB 帧缓冲） | 保留（复用 2.5MB 动态堆，空间充足） |

修复后，Overlay 在开机后任意时刻打开都能立即检测到 sysmodule，不再误报 “not installed”。

## 安装步骤

### 1. 删除旧数据（建议首次升级时执行）

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

> 安装布局与官方 sysmodule 模板一致：`atmosphere/contents/<title_id>/exefs.nsp` + `flags/boot2.flag`，
> 由 Atmosphère 在开机时自动以 boot2 方式启动。

### 3. 确认 boot2.flag 存在

```
SD:/atmosphere/contents/4200000000003103/flags/boot2.flag   ← 必须是 boot2.flag（空文件即可）
```

### 4. 重启 Switch

## 验证

1. 开机后**立即**打开 Overlay（无需等待），主菜单应显示
   **“Parental Control is Enabled / Disabled”**，而不是 “not installed”。
2. 设一个很短的单次时长（如 2 分钟）+ 休息（如 1 分钟），连续游玩触发时应看到更大的全屏提示，随后游戏被关闭。
3. 开启 DEBUG 日志，应每 10 分钟出现心跳 `[Monitor] Heartbeat: monitoring still active (loop #N)`。

默认 PIN：按 **A A A A**（四次 A 键）
