# NSParentalControl v1.3.10 安装说明

## 本版修复（v1.3.9 开机崩溃回归）
v1.3.9 把堆改成 `svcSetHeapSize` 动态堆，**在 boot2 阶段被内核拒绝（rc=1:101）**
→ 堆大小为 0 → `new Ipc::Server(...)` 抛 `std::bad_alloc` → 进程终止 → 开机崩溃。
这正是 v1.3.5 曾踩过、之后放弃的同款坑。

**v1.3.10 修正：撤回到静态 512KB inner heap（sys-clk/nx-ovlloader 标准做法，v1.3.8 实测可在 boot2 启动）**，
同时**保留 v1.3.9 的“立即注册 pctrl 服务”**（这是修复 Overlay 误报 “not installed” 的真正关键）。

静态堆 + 384KB 线程栈 ≈ 896KB，远低于会挤掉 qlaunch 的 4MB 阈值，开机安全。

## 仍保留的有效修复
- **服务开机即注册**：`main()` 起始立即 `Ipc::Server("pctrl")`，不再等待 qlaunch/ns 的 ~32s 空窗，Overlay 不再误报 not installed。
- **NPDM 对齐可运营参考**：`main_thread_priority` 15；无 `system_resource_size`；`force_debug` false；syscall 白名单含 `svcSetHeapSize`（仅作能力声明，运行期不再调用）。

## 安装
将 `NSParentalControl-v1.3.10-fix.zip` 解压到 SD 卡根目录，重启即可。

布局：
```
atmosphere/contents/4200000000003103/
├── exefs.nsp
├── flags/boot2.flag
└── toolbox.json
switch/.overlays/parental_control.ovl
```

## 备注
`sysmodule/source/gui/screen_timeout.cpp` 中的 `svcSetHeapSize` 属**死代码**
（`ScreenTimeout` 类从未被实例化，实际全屏提示走 `Renderer::drawNotice` 的 vi 帧缓冲路径），不会执行。
