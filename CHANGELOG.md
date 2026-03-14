# Changelog
# 例子

### 新增

### 优化

### 修复

### Added

### Improve

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
