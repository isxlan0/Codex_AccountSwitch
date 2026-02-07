<h1 align="center"><b>Codex Account Switch</b></h1>

<p align="center">
  <b>本地化、多账号、高效率的 Codex 账号管理与切换工具</b><br/>
  基于 <code>C++ / Win32 / WebView2</code>，专注稳定与速度。
</p>

<p align="center">
  <a href="./README_EN.md"><b>English README</b></a>
</p>

## 核心功能

- 账号备份/切换/删除，一站式管理
- 支持导入/导出账号备份包（ZIP）
- 支持额度刷新（5H / 7D 额度与重置信息）
- 支持账号套餐自动识别（个人/团队）
- 支持主题模式：自动 / 浅色 / 深色
- 支持多语言 UI（`webui/lang/*.json`）

## 界面导览

1. 账号管理页：批量操作账号、刷新额度、快速切换
2. 关于页：版本信息、更新检查、项目链接
3. 设置页：语言、IDE、主题、自动更新等配置

## 使用案例 (Usage Examples)

### 1. 账号管理主页 / Accounts
<p align="center">
  <img src="./image/1_CN.png" alt="账号管理主页 CN" width="46%" />
  <img src="./image/1_EN.png" alt="Accounts EN" width="46%" />
</p>

### 2. 关于页 / About
<p align="center">
  <img src="./image/2_CN.png" alt="关于页 CN" width="46%" />
  <img src="./image/2_EN.png" alt="About EN" width="46%" />
</p>

### 3. 设置页 / Settings
<p align="center">
  <img src="./image/3_CN.png" alt="设置页 CN" width="46%" />
  <img src="./image/3_EN.png" alt="Settings EN" width="46%" />
</p>

## 技术架构

- 原生层：`C++ / Win32 / WebView2`
- 前端层：`HTML + CSS + JavaScript`
- 通信方式：WebView `postMessage` 与 Host Action 路由
- 数据存储：本地 JSON 文件（用户目录）

主要目录：

- `Codex_AccountSwitch/`：核心 C++ 源码
- `webui/`：前端界面资源
- `installer/`：安装包脚本
- `image/`：README 演示图片

## 数据目录

运行时数据默认写入：

- `%LOCALAPPDATA%\Codex Account Switch\config.json`
- `%LOCALAPPDATA%\Codex Account Switch\backups\index.json`
- `%LOCALAPPDATA%\Codex Account Switch\backups\...`

## 安装指南

### 运行环境

- Windows 10/11 x64
- WebView2 Runtime

### 编译

1. 打开解决方案：`Codex_AccountSwitch.slnx`
2. 选择 `Release | x64`
3. 编译后产物：
   - `x64/Release/Codex_AccountSwitch.exe`
   - `x64/Release/WebView2Loader.dll`

### 打包安装程序

- `installer/build_installer.bat`（推荐）
- `installer/build_installer.ps1`

输出目录：`dist/`

## 许可证

本项目采用 `MIT License`，详见 `LICENSE`。

## 安全说明

所有账号数据默认仅保存在本地。除非你主动导出或分享，否则数据不会离开设备。
