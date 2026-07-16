# NSParentalControl v1.3.11 安装说明

## 本版修复（限玩时间到却不限制游戏）
v1.3.10 虽然能开机，但**到达 limit 后游戏没被关掉**。根因是我在 v1.3.9 误删了 NPDM 的
`system_resource_size`（当时误以为它是 boot2 风险，其实 v1.3.8 实测可正常开机，真正的开机
崩溃是 `svcSetHeapSize`，已在 v1.3.10 修掉）。

### 根因链
1. 删除 `system_resource_size` → 进程没有足够内存区。
2. `Renderer::init()` 的 `framebufferCreate`（底层 TransferMemory 的 backing 内存来自进程堆）
   分配 ~1.84MB 全屏帧缓冲**失败** → `m_currentFramebuffer` 为 NULL。
3. `drawNotice()` 往空帧缓冲写 → **监控线程崩溃**。
4. 崩溃发生在 `terminateCurrentApplication()` 之前 → 游戏从未被终止 → “limit 到了却不限制”。

### 修复（v1.3.11）
- **恢复 `system_resource_size: 0x1000000`**：为全屏帧缓冲预留内存区（与 v1.3.8 一致，实测可开机）。
- **`monitor.cpp`：先 `terminateCurrentApplication()` 再 `drawNotice()`**。
  无论如何先把游戏终止，提示渲染失败也绝不阻断限制。
- **`renderer.hpp`：`framebufferCreate` 失败时安全降级为 toast**（不再写空指针崩溃），
  `drawNotice` 返回 false，调用方走系统通知分支。

### 仍保留的有效修复
- 静态 512KB inner heap（开机安全，不再用 `svcSetHeapSize`）。
- 开机即注册 `pctrl` 服务（Overlay 不再误报 not installed）。
- NPDM：`main_thread_priority` 15、`force_debug` false、syscall 白名单含 `svcSetHeapSize`（仅声明，运行期不调用）。

## 安装
将 `NSParentalControl-v1.3.11-fix.zip` 解压到 SD 卡根目录，重启即可。

布局：
```
atmosphere/contents/4200000000003103/
├── exefs.nsp
├── flags/boot2.flag
└── toolbox.json
switch/.overlays/parental_control.ovl
```

## 验证要点
- 到达单次/每日上限时，游戏应被立即关闭，并显示“该休息了 / 时间到”全屏提示。
- 强制休息倒计时内，任何重新启动的游戏都会被立即关闭。
