# 飞机大战

一个使用 C++20、EasyX 和 Win32 API 编写的纵版飞机射击游戏，支持单人/双人、三关闯关、无限生存、道具、Boss、存档和排行榜。

## 运行环境

本项目使用 EasyX，只能在 Windows 上原生编译和运行。

推荐环境：

- Windows 10 或 Windows 11（64 位）
- Visual Studio 2026 Community
- Visual Studio 工作负载：`使用 C++ 的桌面开发`
- Windows 10/11 SDK
- MSVC `v145` 工具集
- EasyX 26.1.1

项目已设置为 C++20，默认推荐使用 `x64 + Debug` 配置。

> 如果使用 Visual Studio 2022，请直接打开 `飞机大战/飞机大战.vcxproj`，并在提示时把平台工具集从 `v145` 重定向为 `v143`。同时需要为 Visual Studio 2022 安装对应的 EasyX 库。旧版 Visual Studio 可能无法直接打开 `.slnx` 文件。

## 第一次配置

### 1. 安装 Visual Studio

安装 Visual Studio 时勾选：

- `使用 C++ 的桌面开发`
- MSVC C++ x64/x86 生成工具
- Windows 10 SDK 或 Windows 11 SDK

如果已经安装 Visual Studio，可以打开“Visual Studio Installer”，选择“修改”，再补装以上组件。

### 2. 安装 EasyX

项目根目录提供了安装程序：

```text
EasyX_26.1.1.exe
```

在 Windows 中运行该安装程序，选择当前使用的 Visual Studio 版本并完成安装。建议同时安装 x86 和 x64 库，本项目运行时选择 x64。

EasyX 使用静态链接，编译成功后的程序通常不需要额外携带 EasyX DLL。

### 3. 打开项目

推荐双击：

```text
飞机大战.slnx
```

如果 Visual Studio 无法识别 `.slnx`，可以直接打开：

```text
飞机大战/飞机大战.vcxproj
```

### 4. 编译运行

在 Visual Studio 顶部选择：

```text
配置：Debug
平台：x64
```

然后按：

- `F5`：调试运行
- `Ctrl + F5`：不调试运行
- `Ctrl + Shift + B`：仅生成项目

首次打开时如果 Visual Studio 提示“重定向项目”或安装缺失的 `v145` 工具集，选择安装缺失组件即可。

## 资源路径要求

程序使用相对路径读取 `./images/`，并在当前工作目录保存存档和排行榜。因此运行时的工作目录必须是：

```text
项目根目录/飞机大战
```

在 Visual Studio 中可检查：

```text
项目属性 → 配置属性 → 调试 → 工作目录
```

推荐设置为：

```text
$(ProjectDir)
```

不要直接移动 `x64/Debug/飞机大战.exe` 后单独运行，否则可能找不到图片。发布时应让可执行文件的工作目录中同时存在完整的 `images` 文件夹。

## 命令行编译

安装 Visual Studio 和 EasyX 后，打开“Developer Command Prompt for VS”，进入仓库根目录并执行：

```bat
msbuild "飞机大战\飞机大战.vcxproj" /p:Configuration=Debug /p:Platform=x64
```

命令行运行前也要先进入资源目录：

```bat
cd /d "项目路径\飞机大战"
"x64\Debug\飞机大战.exe"
```

具体输出目录可能随 Visual Studio 的生成设置变化，以生成日志中显示的路径为准。

## 游戏模式

### 闯关模式

- 共 3 个关卡，每关末尾有 Boss。
- 每一关独立使用 3 点生命。
- 击败 Boss 后进行关卡结算，清理战场并恢复满生命，再进入下一关。
- 通过第三关后完成游戏。

### 无限模式

- 初始拥有 10 点生命。
- 没有最终关卡，以生存时间和得分为目标。
- 每 45 秒提升一个难度等级。
- 随游戏时间增加，敌人强度、同屏数量和生成速度会提高。

两种玩法都支持单人和双人。

## 操作说明

| 功能 | 单人 / P1 | P2 |
| --- | --- | --- |
| 移动 | `W/A/S/D`；单人时也可用方向键 | 方向键 |
| 射击 | `Space` | 小键盘 `1` |
| 使用炸弹 | `B`，两名玩家共享炸弹数量 | `B` |
| 暂停 | `Esc` | `Esc` |

暂停菜单支持继续游戏、保存并返回主菜单、放弃游戏和退出程序。

## 道具说明

| 道具 | 功能 |
| --- | --- |
| 火力补给 | 7 秒内同时发射 3 枚子弹 |
| 生命道具 | 恢复 1 点生命，但不会超过当前模式的生命上限 |
| 炸弹补给 | 增加 1 枚炸弹 |
| 护盾补给 | 抵消下一次受到的伤害 |

按 `B` 使用炸弹时，会消灭普通敌机并清除敌方子弹；对 Boss 造成固定伤害。

## 存档和排行榜

游戏运行后会在工作目录产生以下文件：

| 文件 | 用途 |
| --- | --- |
| `savegame.dat` | 当前游戏存档，包含玩法、关卡/难度、生命、道具等状态 |
| `leaderboard.txt` | 历史最高分排行榜 |
| `last_player.txt` | 最近使用的玩家昵称 |

新的存档格式为 `PLANE_SAVE_V2`，仍然兼容项目原有的 `PLANE_SAVE_V1` 存档。

## 项目结构

```text
飞机大战/
├── EasyX_26.1.1.exe          # EasyX 安装程序
├── 飞机大战.slnx             # Visual Studio 解决方案
├── README.md
└── 飞机大战/
    ├── FileName.cpp          # 游戏主要代码
    ├── 飞机大战.vcxproj       # Visual C++ 项目文件
    ├── images/               # 飞机、敌人、子弹、背景和道具资源
    ├── savegame.dat          # 运行后生成/更新的存档
    ├── leaderboard.txt       # 排行榜
    └── last_player.txt       # 最近玩家
```

## 常见问题

### 找不到 `graphics.h`

EasyX 没有安装到当前 Visual Studio，或者安装时选择了错误的 Visual Studio 版本。重新运行 `EasyX_26.1.1.exe`，为正在使用的 Visual Studio 安装 EasyX，然后重启 Visual Studio。

### 提示缺少 `v145` 工具集

当前工程使用 `v145`。可选择以下任一种处理方法：

1. 在 Visual Studio Installer 中安装对应的 MSVC `v145` 组件。
2. 如果使用 Visual Studio 2022，在“项目属性 → 常规 → 平台工具集”中选择 `v143`，或右键解决方案执行“重定向解决方案”。

### 出现 EasyX/图形函数链接错误

确认 EasyX 的目标架构与项目平台一致。本项目推荐 `x64`；如果只安装了 EasyX x86 库，则需要补装 x64 库或把项目切换为 Win32。

### 程序能启动但没有背景或飞机图片

这是工作目录不正确。将调试工作目录设为 `$(ProjectDir)`，并确认 `飞机大战/images` 文件夹完整存在。

### 双人模式 P2 无法射击

P2 使用的是小键盘数字 `1`，不是主键盘数字键。笔记本可能需要先开启 `Num Lock`。

### 能否在 macOS 或 Linux 上运行

不能直接运行。EasyX 和代码中的 Win32 API 依赖 Windows。需要使用 Windows 实机或 Windows 虚拟机，并在其中安装 Visual Studio 与 EasyX。

## 参考资料

- [EasyX 安装文档](https://docs.easyx.cn/zh-cn/setup)
- [Visual Studio C++ 环境安装说明](https://learn.microsoft.com/zh-cn/cpp/build/vscpp-step-0-installation)

