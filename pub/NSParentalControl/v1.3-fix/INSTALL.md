# NSParentalControl v1.3-fix 安装说明

## ⚠️ 重要：必须使用 boot2.flag

**Atmosphère 只支持 `boot2.flag`，不存在 `boot.flag`。** 之前的建议有误，请务必使用 `boot2.flag`。

本版本已修复所有导致 boot2 阶段崩溃的 bug，可以安全使用 `boot2.flag`。

## 安装步骤

### 1. 删除旧数据（重要！）

如果 SD 卡上存在以下文件，**必须删除**：
```
/config/parental_control/sessions.json
/config/parental_control/settings.json
/config/parental_control/sysmodule.log
```

### 2. 复制文件

将以下文件复制到 SD 卡对应位置：

| 源文件 | SD 卡目标路径 |
|---|---|
| `atmosphere/contents/4200000000003103/exefs.nsp` | `SD:/atmosphere/contents/4200000000003103/exefs.nsp` |
| `atmosphere/contents/4200000000003103/flags/boot2.flag` | `SD:/atmosphere/contents/4200000000003103/flags/boot2.flag` |
| `atmosphere/contents/4200000000003103/toolbox.json` | `SD:/atmosphere/contents/4200000000003103/toolbox.json` |
| `switch/.overlays/parental_control.ovl` | `SD:/switch/.overlays/parental_control.ovl` |

### 3. 确认 boot2.flag

确保 SD 卡上存在：
```
SD:/atmosphere/contents/4200000000003103/flags/boot2.flag
```
**不是 `boot.flag`，必须是 `boot2.flag`！**

### 4. 重启 Switch

重启后 sysmodule 会自动加载。

## 验证

打开 Tesla/Ultrahand overlay，应该看到 "Parental Control is Enabled"（而非 "not installed"）。

默认 PIN：按 **A A A A**（四次 A 键）
