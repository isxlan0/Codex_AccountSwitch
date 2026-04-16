# Changelog
# 例子

### 新增

### 优化

### 修复

### Added

### Improve

### Fix

--

## v1.3.16

### 新增

### 优化
- API 反代的默认模型与测试模型列表改为软件内置预设，不再请求上游 API 获取模型列表。

### 修复

### Added

### Improve
- The API proxy default model and test model list now use built-in presets instead of requesting model IDs from the upstream API.

### Fix

--

## v1.3.15

### 新增

### 优化

### 修复
- 修复添加账号时开启 OAuth 授权后“取消”按钮被禁用，无法退出拾取流程的问题。

### Added

### Improve

### Fix
- Fixed an issue where the Cancel button became unavailable after starting OAuth during Add Account; the flow can now be cancelled and exited normally.

--

## v1.3.14 PR#13

### 新增
- API 反向代理支持添加自定义模型，并在模型选择器和 `/v1/models` 中返回这些模型 PR#13

### 优化
- 新增代理默认模型设置，请求未指定模型时可自动使用默认模型 PR#13
- 无痕模式支持自定义 `config.toml` 模板内容，允许按需覆盖代理接入参数 PR#13

### 修复

### Added
- API proxy now supports custom model IDs and exposes them in model selectors and `/v1/models` PR#13

### Improve
- Added proxy default model selection so requests without a model can fall back automatically PR#13
- Stealth mode now supports a customizable `config.toml` template for proxy integration settings PR#13

### Fix
--

## v1.3.12

### 新增

### 优化
- 根据上游响应返回真流式数据，不再等待全部输出完毕后返回
- 系统没装WebView2时提示未安装

### 修复
- Token统计不统计使用Token

### Added

### Improve
- Returns true streaming data based on the upstream response, no longer waiting for all output to complete before returning.
- Prompts "WebView2 not installed" when the system does not have it installed.

### Fix
- Token usage is included in token statistics.
--

## v1.3.11

### 新增

### 优化
- 切换账号时 不再替换默认模型 #11

### 修复

### Added

### Improve
- When switching accounts, the default model will no longer be replaced. #11

### Fix

--

## v1.3.10

### 新增

### 优化
- 为窗口关闭消息添加选择行为 #9

### 修复

### Added

### Improve
- Add selection behavior to window close message #9

### Fix

--

## v1.3.9

### 新增

### 优化
- 异常标记仅在返回 401 错误时运行，以防止错误删除帐户。

### 修复

### Added

### Improve
- The exception flag only runs when a 401 error is returned, to prevent erroneous account deletion.

### Fix

--

## v1.3.8

### 新增

### 优化
- API反代-固定账号模式下，原来的规则为未选中账号则报错，修改为自动选择一个配额高的账号使用

### 修复

### Added

### Improve
- API reverse proxy - In fixed account mode, the original rule was to report an error if no account was selected. This has been changed to automatically select an account with a higher quota.

### Fix


---

## v1.3.7

### 新增
- 设置-菜单显示设置:可快速隐藏不需要的TAB
- 云账号TAB:可定时下载其他人分享的账号数据,此功能依赖 ([Token-JSON-Provider](https://github.com/isxlan0/Token-JSON-Provider)) 作为下载提供商

### 优化
- 刷新账号配额与导入账号时添加提示导入进度

### 修复

### Added
- Settings - Menu Display Settings: Quickly hide unnecessary tabs
- Cloud Account Tab: Schedules downloading account data shared by others. This feature relies on ([Token-JSON-Provider](https://github.com/isxlan0/Token-JSON-Provider)) as the download provider.

### Improve
- Add progress indicators when refreshing account quotas and importing accounts.

### Fix

---

## v1.3.6

### 新增
- 账号管理-添加:一键清理失效账号
- 设置-账号-添加:自动删除异常账号

### 优化

### 修复

### Added
- Account Management - Add: One-click cleanup of invalid accounts
- Settings - Accounts - Add: Automatically delete abnormal accounts

### Improve

### Fix

---

## v1.3.5
### 修复
- 修复正常使用OA授权拿到OAuth数据后，切换账号因为数据没有补齐导致Codex客户端认为没有登录的问题 #1

### Added

### Improve

### Fix
- Fixed an issue where, after obtaining OAuth data through normal OA authorization, switching accounts would cause the Codex client to believe the user was not logged in due to incomplete data. #1
---

## v1.3.4

### 新增
- 新增 WebDAV 同步账号数据能力。#2
- 新增异常账号标记功能，便于快速识别异常状态账号。

### 优化
- 补齐本地化翻译，完善多语言显示体验。

### 修复
- 修复无法彻底关闭反向代理端口的问题。

---

## v1.3.3

### 优化
- 优化大量账号导入后的使用体验，导入完成后不再自动刷新。
- 优化额度刷新回调逻辑。
- 优化更新判断链接，使下载链接匹配更精准。

### 修复
- 修复导入大量账号时可能出现取消提示的问题。
- 修复若干已知问题并提升整体稳定性。

---

## v1.3.2

### 新增
- 新增 Windows 多架构构建支持。
- 新增便携版打包流程。
- 新增 GitHub Actions 自动构建支持。

### 优化
- 为构建与打包结果补充平台及架构标识。
- 为批处理与构建流程补充 `x86` 与 `ARM` 支持。

### 修复
- 修复目录问题导致无法构建的问题。
- 调整工具链版本以改善构建兼容性。

---

## v1.3.1

### 新增
- 新增 API 反代功能。
- 新增 OAuth 授权功能。
- 新增流量日志与 Token 调用统计功能。

### 优化
- 优化整体 UI 体验。

---

## v1.2.8

### 新增
- 新增快速导入多个 OAuth 的能力。
- 新增导入后自动重命名功能。
- 新增账号重命名功能。
- 新增批量操作功能。

### 优化
- 优化多账号导入流程，减少手动输入步骤。

---

## v1.2.7

### 优化
- 调整部分描述信息，改善界面文案表现。

### 修复
- 修复免费版显示异常的问题。
- 修复部分场景下空指针崩溃的问题。

---

## v1.2.5

### 新增
- 新增仪表盘分区。
- 新增自动刷新额度功能。
- 新增导入现有 OAuth 文件功能。
- 新增额度过低时的账号切换提示。

### 优化
- 优化 CSS 与部分界面布局。

---

## v1.2.4

### 修复
- 修复公告窗口位置显示异常的问题。
- 修复部分语言缺少本地化翻译的问题。

---

## v1.2.3

### 修复
- 修复添加新账号后索引异常显示的问题。

### 其他
- 更新项目说明文档。

---

## v1.2.2

### 修复
- 修复点击额度查询时缺少提示反馈的问题。
- 修复部分场景下解析字符串时可能出现的崩溃问题。

---

## v1.2.1

### 新增
- 新增 OpenAI API 额度查询功能。
- 新增深色主题。

---

## v1.0.1

### 优化
- 优化版本号获取逻辑。
- 优化语言包内容。

### 修复
- 修复公告显示换行问题。

---

## v1.0.0

### 新增
- 初始版本发布。
