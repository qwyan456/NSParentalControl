#pragma once

// 探测并连接 pctrl IPC 服务：成功返回 true 并设置 AppContext.is_available/is_enabled。
// 失败（服务尚未注册）返回 false，调用方可稍后重试。
bool connectToService();
