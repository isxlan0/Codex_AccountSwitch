# Codex Account Switch

`Codex Account Switch` 是一个基于 `C++ / WebView2` 的本地账号切换工具，用于管理 Codex 账号，支持账号备份、切换、删除、导入导出。

## 数据路径

应用运行时数据目录：

- `%LOCALAPPDATA%\Codex Account Switch\config.json`
- `%LOCALAPPDATA%\Codex Account Switch\backups\index.json`
- `%LOCALAPPDATA%\Codex Account Switch\backups\...`

## 开发与构建

### 环境要求

- Windows 10/11 x64
- Visual Studio 2026（MSVC）
- WebView2 Runtime
- Inno Setup 6（用于生成安装包）

### 编译程序

1. 打开解决方案并选择 `Release | x64`。
2. 编译后确认生成：
   - `x64/Release/Codex_AccountSwitch.exe`
   - `x64/Release/WebView2Loader.dll`

### 生成安装包（Setup.exe）

可使用：

- `installer/build_installer.bat`（推荐，一键生成）
- `installer/build_installer.ps1`

输出目录：`dist/`

## 安装与卸载说明

- 安装向导支持选择安装模式：
  - 当前用户（默认 `%LOCALAPPDATA%\Programs\Codex Account Switch`）
  - 所有用户（`Program Files`，需管理员权限）

## 配置与多语言

- 主要前端资源：`webui/index.html`、`webui/js/app.js`、`webui/css/styles.css`
- 语言索引：`webui/lang/index.json`
- 语言文本：`webui/lang/*.json`

## 贡献者(Contributors)

- [@isxlan0](https://github.com/isxlan0)

## 特别感谢(Special Thanks)

- Microsoft WebView2
- Inno Setup

## 版权许可

本项目采用 `MIT License`，详见 `LICENSE` 文件。

## 安全声明

本应用所有账号数据存储于本地文件夹，除非您主动分享，否则数据绝不离开您的设备。
