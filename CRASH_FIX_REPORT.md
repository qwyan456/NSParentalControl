# NSParentalControl 崩溃修复报告

## 环境
- Atmosphère 1.10.3 / FW 21.2.0
- 崩溃现象：开机 crash（已通过 boot2.flag → boot.flag 修复）；UI 调用时 crash

## 修复清单

### P0：saveDatabase() snprintf 截断 bug（UI 崩溃的直接原因）
- **文件**: `sysmodule/source/database/database.cpp`
- **问题**: `snprintf(buf, lenstr, "%s", str)` 只写入 `lenstr-1` 个字符，JSON 最后一个 `}` 被 `\0` 替换。下次 `loadDatabase()` 读取时 `json::parse()` 抛出异常，在 `-fno-exceptions` 下调用 `std::terminate()` → 进程崩溃
- **修复**: 直接使用 `std::string::c_str()` 写入，移除 `malloc/memset/snprintf/free`

### P0：-fno-exceptions 下 json::parse() 崩溃
- **文件**: `database.cpp`, `helpers.cpp`
- **问题**: 项目使用 `-fno-exceptions` 编译，但 `nlohmann::json::parse()` 在遇到无效 JSON 时会抛出异常，导致 `std::terminate()` → 崩溃
- **修复**: 在所有 `json::parse()` 调用前添加 `json::accept()` 预检查

### P0：-fno-exceptions 下 std::stoull() 崩溃
- **文件**: `sysmodule/source/helpers.cpp`
- **问题**: `std::stoull()` 在输入无效时抛出 `std::invalid_argument` 或 `std::out_of_range`
- **修复**: 改用 `strtoull()` + `errno` 检查

### P1：__appInit() 末尾错误的 smExit()
- **文件**: `sysmodule/source/main.cpp`
- **问题**: `__appInit()` 末尾调用 `smExit()` 关闭了 sm session，导致后续 `Ipc::Server` 构造函数中 `smRegisterService()` 失败
- **修复**: 删除 `smExit()`

### P1：__wrap_exit() 中的 smExit()
- **文件**: `sysmodule/source/main.cpp`
- **问题**: `__wrap_exit()` 中调用 `smExit()` 可能在 sm 已被关闭时导致双重释放
- **修复**: 移除 `smExit()`

### P1：Server 构造/析构中冗余的 smInitialize/smExit
- **文件**: `sysmodule/source/ipc/Server.cpp`
- **问题**: `Server` 构造函数重复调用 `smInitialize()`，析构函数调用 `smExit()` 可能影响其他线程
- **修复**: 移除冗余调用

### P1：rebootToPayload() 中冗余的 smInitialize/smExit
- **文件**: `sysmodule/source/helpers.cpp`
- **问题**: `rebootToPayload()` 中调用 `smInitialize()` 和 `smExit()`，关闭 sm 会影响其他线程
- **修复**: 移除冗余调用

### P1：renderer.hpp 中冗余的 smInitialize/smExit
- **文件**: `sysmodule/source/gui/renderer.hpp`
- **问题**: `Renderer::init()` 中重复调用 `smInitialize()` 和 `smExit()`
- **修复**: 移除冗余调用

### P2：loadDatabase() 缺少 "history" 键检查
- **文件**: `sysmodule/source/database/database.cpp`
- **问题**: `j_data["history"]` 在键不存在时会创建 null 值，`.get<std::vector<json>>()` 会抛出异常
- **修复**: 添加 `contains()` 和 `is_array()` 检查

## 修改的文件列表
1. `sysmodule/source/database/database.cpp` — saveDatabase 修复 + 安全 JSON 解析
2. `sysmodule/source/helpers.cpp` — strtoull 修复 + 安全 JSON 解析 + sm 调用修复
3. `sysmodule/source/main.cpp` — __appInit/__wrap_exit smExit 修复
4. `sysmodule/source/ipc/Server.cpp` — smInitialize/smExit 修复
5. `sysmodule/source/gui/renderer.hpp` — smInitialize/smExit 修复
