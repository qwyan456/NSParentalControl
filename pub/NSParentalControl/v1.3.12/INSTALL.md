# NSParentalControl v1.3.12 安装说明

## 本版修复（开机后 Overlay 误报 “Parental Control is not installed”）
此前 v1.3.7 实测可用，但服务在 qlaunch 就绪后（开机约 30+ 秒）才注册；Overlay 只在
启动时**探测一次**并缓存 `is_available=false`，之后永不再探测 → 一旦在注册前打开 Overlay，
就会**永久**显示 “not installed”，且所有受 `is_available` 门控的 IPC 功能全部失效。

v1.3.9 试图“开机瞬间立即注册”来消除空窗，但在本机 boot2 环境下 `smRegisterService` 在开机
瞬间被内核拒绝，服务**永远注册不上**，反而变成更严重的永久 not installed。

### 根因（两端协同）
| 端 | 问题 |
|---|---|
| Overlay | `connectToService()` 仅在 `main()` 启动时调用一次，`is_available` 被缓存，无自愈 |
| Sysmodule | boot2 开机瞬间 sm 未就绪，过早注册失败（v1.3.9 的坑） |

### 修复（v1.3.12，两端同时改）
- **Overlay（核心）**：`MainMenuPanel::update()` 每 2 秒重探测 `connectToService()`。
  一旦 sysmodule 注册成功，立即翻转 `is_available=true`、清空并重绘列表、刷新副标题
  （`not installed` → `Enabled/Disabled`），所有 IPC 功能随之恢复。
- **Sysmodule（稳健性）**：回退到 v1.3.7 已验证可用的“**qlaunch 就绪后注册**”时序，
  并加重试循环（最多 ~60s）应对 sm 瞬时不可用；新增 `Server::isReady()` 供调用方判断。
- 新增 `overlay/src/main.h` 暴露 `connectToService()` 供面板复用。

### 仍保留的有效设置（与 v1.3.11 一致，未改动）
- 静态 512KB inner heap（boot2 开机安全，绝不用 `svcSetHeapSize`）。
- NPDM：`main_thread_priority` 15、`system_resource_size` 0x1000000（全屏帧缓冲所需）、
  `force_debug` false、syscall 白名单含 `svcSetHeapSize`（仅声明，运行期不调用）。
- 限玩到时先 `terminateCurrentApplication()` 再提示，`framebufferCreate` 失败安全降级 toast。

## 安装
将 `NSParentalControl-v1.3.12-fix.zip` 解压到 SD 卡根目录，重启即可。

布局：
```
atmosphere/contents/4200000000003103/
├── exefs.nsp
├── flags/boot2.flag
└── toolbox.json
switch/.overlays/parental_control.ovl
```

## 验证要点
- 开机后**立即**打开 Overlay：可能先显示 “not installed”，但约 30 秒内会自动变为
  `Enabled/Disabled` 并显示当前用户/游戏/剩余时间（自愈生效）。
- 开机 1 分钟后再打开 Overlay：应直接显示 `Enabled/Disabled`，不再误报。
- 到达单次/每日上限时，游戏被立即关闭并显示“该休息了 / 时间到”全屏提示（v1.3.11 行为不变）。
- 若升级后仍有问题，v1.3.7 仍作为可用回退版本保留在 Release 中。
