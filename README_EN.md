# Codex Account Switch

<p align="center">
  <b>A fast, local-first multi-account manager for Codex</b><br/>
  Built with <code>C++ / Win32 / WebView2</code> for performance and reliability.
</p>

<p align="center">
  <a href="./README.md"><b>简体中文 README</b></a>
</p>

## Core Features

- One-stop account backup / switch / delete workflow
- Import / export backup bundles (ZIP)
- Quota refresh support (5H / 7D windows and reset info)
- Automatic account plan detection (Personal / Team)
- Theme modes: Auto / Light / Dark
- Multi-language UI via `webui/lang/*.json`

## UI Overview

1. Accounts: manage backups, refresh quota, quick switching
2. About: version info, update check, repository link
3. Settings: language, IDE, theme, auto-update

## Usage Examples

### 1. Accounts Home
<p align="center">
  <img src="./image/1_CN.png" alt="Accounts CN" width="46%" />
  <img src="./image/1_EN.png" alt="Accounts EN" width="46%" />
</p>

### 2. About
<p align="center">
  <img src="./image/2_CN.png" alt="About CN" width="46%" />
  <img src="./image/2_EN.png" alt="About EN" width="46%" />
</p>

### 3. Settings
<p align="center">
  <img src="./image/3_CN.png" alt="Settings CN" width="46%" />
  <img src="./image/3_EN.png" alt="Settings EN" width="46%" />
</p>

## Technical Architecture

- Native layer: `C++ / Win32 / WebView2`
- Frontend layer: `HTML + CSS + JavaScript`
- Bridge: WebView `postMessage` + host action routing
- Storage: local JSON files in user profile data path

Main folders:

- `Codex_AccountSwitch/`: core C++ source
- `webui/`: frontend assets
- `installer/`: setup build scripts
- `image/`: README screenshots

## Data Directory

Runtime data is stored in:

- `%LOCALAPPDATA%\Codex Account Switch\config.json`
- `%LOCALAPPDATA%\Codex Account Switch\backups\index.json`
- `%LOCALAPPDATA%\Codex Account Switch\backups\...`

## Installation Guide

### Requirements

- Windows 10/11 x64
- WebView2 Runtime

### Build

1. Open solution: `Codex_AccountSwitch.slnx`
2. Choose `Release | x64`
3. Build outputs:
   - `x64/Release/Codex_AccountSwitch.exe`
   - `x64/Release/WebView2Loader.dll`

### Build Installer

- `installer/build_installer.bat` (recommended)
- `installer/build_installer.ps1`

Output folder: `dist/`

## License

Licensed under the `MIT License`. See `LICENSE`.

## Security Notice

All account data is stored locally by default. Data never leaves your device unless you explicitly export/share it.
