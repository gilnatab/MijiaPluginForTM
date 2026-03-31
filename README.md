# 米家插座功率 TrafficMonitor 插件

## 简介

这是一个 TrafficMonitor 插件，可以在 Windows 任务栏实时显示米家/酷控（cuco）智能插座的功率数值，并可选开启功率历史记录功能。

**主要功能：**
- 📊 实时在任务栏显示功率（W）
- 💾 可选启用功率历史记录（按分钟采样，最多保存7天）
- 📈 鼠标悬停提示：显示10分钟/1小时/24小时最大/最小/平均功率
- ⚙️ 设置对话框：配置设备IP、Token，调整显示格式
- 🔗 支持断线自动重连

---

## 编译说明

### 环境要求

- Visual Studio 2019 或 2022（推荐 VS2022）
- Windows SDK 10.0+
- C++17 标准

### 编译步骤

1. 用 Visual Studio 打开 `MijiaPowerPlugin.sln`
2. 选择目标平台：
   - 如果你的 TrafficMonitor 是 **x64 版本** → 选 `Release | x64`
   - 如果你的 TrafficMonitor 是 **x86/32位版本** → 选 `Release | Win32`
3. 点击 **生成 → 生成解决方案**（或 `Ctrl+Shift+B`）
4. 编译成功后，DLL 文件位于：
   ```
   bin\Release\x64\MijiaPower.dll
   ```

---

## 安装说明

1. 找到 TrafficMonitor 主程序所在目录
2. 如果没有 `plugins` 子目录，手动创建：`<TrafficMonitor目录>\plugins\`
3. 将 `MijiaPower.dll` 复制到 `plugins` 目录中
4. **重启** TrafficMonitor
5. 在 TrafficMonitor 中右键 → **选项** → **常规设置** → **插件管理** → 查看是否加载成功

---

## 配置说明

首次使用需要配置设备信息：

1. 在 TrafficMonitor 任务栏右键 → **选项**，或直接右键插件显示区域 → **选项**
2. 填写以下内容：
   - **设备 IP**：米家插座的局域网 IP 地址（如 `192.168.1.100`）
   - **Token**：32位十六进制字符串（获取方法见下方）
   - **名称**：设备昵称，显示在提示信息中
3. 点击 **测试连接** 验证配置是否正确
4. 点击 **确定** 保存并生效

### 如何获取 Token

可参考原项目 `mijia_plug` 中的方法，或使用以下工具：
- [miio 工具](https://github.com/rytilahti/python-miio)：`python -m miio.extract_tokens`
- [MiToolKit](https://github.com/ultraicy/MiToolKit)
- 米家 APP 抓包

---

## 插件选项说明

| 选项 | 说明 |
|------|------|
| 启用功率历史记录 | 开启后按分钟采样，保存到配置目录的 `.json` 文件 |
| 显示"功率:"标签 | 控制任务栏上是否显示"功率:"前缀 |
| 显示 W 单位 | 控制数值后是否显示"W"单位符号 |
| 采集间隔（秒） | 每隔多少秒向设备发一次查询（建议3-5秒） |
| 小数位数 | 0=整数，1=一位小数（默认），2=两位小数 |

---

## 文件说明

| 文件 | 说明 |
|------|------|
| `PluginInterface.h` | TrafficMonitor 插件接口定义 |
| `MiioDevice.h/.cpp` | 纯C++ miIO协议实现（AES-128-CBC + UDP） |
| `PowerHistory.h/.cpp` | 功率历史采样与统计 |
| `PluginConfig.h/.cpp` | 插件配置（INI文件读写） |
| `MijiaPowerPlugin.h/.cpp` | 插件主类（ITMPlugin/IPluginItem实现） |
| `OptionsDlg.h/.cpp` | 设置对话框（纯Win32） |
| `pch.h/.cpp` | 预编译头 |

---

## 配置文件位置

插件配置文件默认保存在 TrafficMonitor 的插件配置目录：
```
%AppData%\TrafficMonitor\plugins\MijiaPower\
  MijiaPower.ini          ← 设备信息和选项
  MijiaPower_history.json ← 功率历史记录（如果启用）
```

---

## 注意事项

1. 插件通过 UDP 协议（端口 54321）直接与设备通信，**设备必须与电脑在同一局域网**
2. miIO 协议需要正确的 Token，错误的 Token 会导致连接失败
3. 采集间隔不建议设置低于 2 秒，以免对设备造成过多请求
4. 如果设备固件不支持 `siid=11,piid=2`（功率属性），请联系插件作者

---

## 兼容的设备

基于原项目 `mijia_plug` 的设备属性定义：

| 属性 | siid | piid | 说明 |
|------|------|------|------|
| 开关 | 2 | 1 | 插座开关状态 |
| 功率 | 11 | 2 | 当前功率（W） |
| 能耗 | 11 | 1 | 累计用电量 |
| 温度 | 12 | 2 | 插座温度 |

已知兼容型号：`cuco.plug.v3`（酷控智能插座V3）

---

## 开发依赖

插件完全无第三方依赖，所有加密算法（AES-128-CBC、MD5）均为纯 C++ 实现，仅依赖 Windows 系统 API（ws2_32.lib、comctl32.lib）。
