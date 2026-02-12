(function () {
  "use strict";

  const IDE_LIST = ["Code.exe", "Trae.exe", "Kiro.exe", "Antigravity.exe"];
  const DEFAULT_I18N = {
    "app.brand":  "Codex Account Switch",
    "tab.dashboard": "Dashboard",
    "tab.accounts":  "Accounts",
    "tab.about":  "About",
    "tab.settings":  "Settings",
    "toolbar.refresh":  "Refresh Quota",
    "toolbar.add_current": "Add Account",
    "toolbar.login_new":  "Login New",
    "toolbar.backup_current":  "Backup Current",
    "toolbar.import_auth":  "Add Account",
    "toolbar.import":  "Import Backup",
    "toolbar.export":  "Export Backup",
    "refresh.disabled": "Disabled",
    "refresh.countdown_prefix": "Next refresh in ",
    "refresh.countdown_suffix": "",
    "refresh.countdown_default": "--:--",
    "search.placeholder":  "Search accounts...",
    "group.all":  "All",
    "group.personal":  "Personal",
    "group.business":  "Business",
    "table.account":  "Account",
    "table.quota":  "Model Quota",
    "table.recent":  "Last Used",
    "table.action":  "Action",
    "about.title":  "Codex Account Switch",
    "about.subtitle":  "Professional Account Management",
    "about.author_label":  "Author",
    "about.author_name":  "Xiao Lan",
    "about.repo_label":  "Open Source",
    "about.repo_link":  "View Code",
    "about.check_update":  "Check Update",
    "about.version_prefix":  "Current Version: {version}",
    "about.version_checking":  "Current Version: {version}, checking...",
    "about.version_check_failed":  "Current Version: {version} (check failed)",
    "about.version_new":  "Current Version: {version}, Latest: {latest}",
    "about.version_latest":  "Current Version: {version} (latest)",
    "settings.title":  "Default Settings",
    "settings.subtitle":  "These settings are used on first launch and all future switches.",
    "settings.tab.general": "General",
    "settings.tab.account": "Account",
    "settings.language_label":  "Language",
    "settings.ide_label":  "IDE Executable",
    "settings.theme_label":  "Theme",
    "settings.theme_auto":  "Auto",
    "settings.theme_light":  "Light",
    "settings.theme_dark":  "Dark",
    "settings.theme_hint":  "Auto follows your Windows theme.",
    "settings.auto_update_label":  "Auto Update",
    "settings.auto_update_hint":  "Check update automatically at startup and prompt before downloading.",
    "settings.quota_refresh_section": "Quota Refresh",
    "settings.auto_refresh_all_label": "Auto Refresh All Accounts",
    "settings.auto_refresh_all_hint": "Refresh all accounts every {minutes} minutes in background.",
    "settings.auto_refresh_current_label": "Auto Refresh Current Account",
    "settings.auto_refresh_current_hint": "Refresh current active account quota every {minutes} minutes.",
    "settings.low_quota_prompt_label": "Low Quota Auto Switch Prompt",
    "settings.low_quota_prompt_hint": "When enabled, low quota will trigger switch prompt and notification window automatically.",
    "settings.refresh_minutes_label": "Interval (minutes)",
    "settings.minutes_hint": "Enter 1-240",
    "settings.countdown_prefix": "Remaining: ",
    "settings.first_run_toast":  "Please confirm default settings for first launch",
    "dashboard.title": "Quota Dashboard",
    "dashboard.subtitle": "Quick overview of account capacity and current account health.",
    "dashboard.total_accounts": "Total Accounts",
    "dashboard.avg_5h": "Average 5H Quota",
    "dashboard.avg_7d": "Average 7D Quota",
    "dashboard.low_accounts": "Low Quota Accounts",
    "dashboard.low_list_title": "Low Quota Account List",
    "dashboard.low_list_empty": "No low quota accounts",
    "dashboard.current_title": "Current Account Quota",
    "dashboard.current_name_empty": "No active account",
    "dashboard.current_5h": "5-hour Remaining",
    "dashboard.current_7d": "7-day Remaining",
    "dashboard.switch_button": "Go To Account Management",
    "dialog.add_account.title": "Add New Account",
    "dialog.add_account.import_current": "Import Current Logged-in Account",
    "dialog.add_account.import_oauth": "Quick Import Existing OAuth",
    "dialog.add_account.login_new": "Login New Account",
    "dialog.backup.title":  "Backup Current Account",
    "dialog.backup.name_label":  "Account Name",
    "dialog.backup.name_placeholder":  "Enter account name",
    "dialog.import_auth.title": "Quick Import Existing OAuth",
    "dialog.import_auth.name_label": "Imported Account Name",
    "dialog.import_auth.name_placeholder": "Enter account name",
    "dialog.common.cancel":  "Cancel",
    "dialog.common.save":  "Save",
    "dialog.common.confirm":  "Confirm",
    "dialog.confirm.title":  "Please Confirm",
    "dialog.confirm.default_message":  "Confirm this action?",
    "dialog.delete.title":  "Delete Account",
    "dialog.delete.message":  "Delete backup for {name}?",
    "dialog.login_new.title":  "Login New Account",
    "dialog.login_new.message":  "Clear current login files and restart {ide}?",
    "accounts.empty":  "No account backups",
    "tag.current":  "Current",
    "tag.group_business":  "Business",
    "tag.group_personal":  "Personal",
    "quota.gpt":  "GPT",
    "quota.placeholder":  "No quota data",
    "quota.format":  "5H {q5} | 7D {q7} | Reset {r5}h/{r7}h",
    "action.switch_title":  "Switch to this account",
    "action.switch":  "Switch",
    "action.refresh_title":  "Refresh quota",
    "action.refresh":  "Refresh",
    "action.delete_title":  "Delete account",
    "action.delete":  "Delete",
    "count.format":  "Showing 1 to {total}, total {total}",
    "count.empty":  "Showing 0 to 0, total 0",
    "status_code.invalid_name":  "Save failed: invalid account name",
    "status_code.name_too_long":  "Account name max length is 32",
    "status_code.auth_missing":  "Save failed: current account file missing",
    "status_code.duplicate_name":  "Duplicate name, please change it",
    "status_code.create_dir_failed":  "Save failed: cannot create backup directory",
    "status_code.write_failed":  "Operation failed: cannot write file",
    "status_code.not_found":  "Operation failed: target not found",
    "status_code.userprofile_missing":  "Operation failed: user directory not found",
    "status_code.restart_failed":  "Failed to restart {ide}, please restart manually",
    "status_code.config_saved":  "Settings saved",
    "status_code.backup_saved":  "Account backup saved",
    "status_code.auth_json_invalid": "This file may not be a valid auth.json (missing required fields)",
    "status_code.auth_json_imported": "OAuth imported",
    "status_code.low_quota_notification": "Low quota detected. Click tray notification to confirm switch.",
    "low_quota.prompt_title": "Low Quota Alert",
    "low_quota.prompt_message": "Current account {current} is down to {currentQuota}% in 5h window.\nSwitch to {best} ({bestQuota}%) and restart IDE?",
    "status_code.debug_action_ok": "Debug action triggered",
    "status_code.debug_action_unsupported": "This action is only available in Debug build",
    "status_code.import_cancelled":  "Import cancelled",
    "status_code.export_cancelled":  "Export cancelled",
    "status_code.open_url_failed":  "Failed to open link",
    "status_code.open_url_success":  "Link opened",
    "status_code.save_config_failed":  "Failed to save settings",
    "status_code.quota_refreshed":  "Quota refreshed",
    "status_code.account_quota_refreshed":  "Account quota refreshed",
    "status_code.unknown_action":  "Unknown action",
    "update.failed":  "Update check failed, please try again later",
    "update.latest":  "Already up to date",
    "update.new":  "New version detected: {latest}",
    "update.dialog.title":  "Update Available",
    "update.dialog.current_label":  "Current",
    "update.dialog.latest_label":  "Latest",
    "update.dialog.notes_label":  "Release Notes",
    "update.dialog.confirm_question":  "Download and install the latest release now?",
    "update.dialog.message":  "Current: {current}\nLatest: {latest}\n\nRelease Notes:\n{notes}\n\nDownload and install the latest release now?",
    "debug.title": "Debug Tools",
    "debug.notify": "Test: Low-Quota Notify",
    "ide.Code.exe":  "VSCode",
    "ide.Trae.exe":  "Trae",
    "ide.Kiro.exe":  "Kiro",
    "ide.Antigravity.exe":  "Antigravity"
}
;

  const ZH_FALLBACK_I18N = {
    "tab.dashboard": "仪表盘",
    "settings.tab.general": "通用",
    "settings.tab.account": "账号",
    "toolbar.refresh": "刷新额度",
    "toolbar.add_current": "添加账号",
    "toolbar.import_auth": "添加账号",
    "toolbar.import": "导入备份",
    "toolbar.export": "导出备份",
    "settings.refresh_minutes_label": "间隔（分钟）",
    "settings.auto_refresh_all_label": "后台自动刷新全部账号额度",
    "settings.auto_refresh_current_label": "自动刷新当前活动账号额度",
    "settings.auto_refresh_all_hint": "每 {minutes} 分钟自动刷新一次全部账号额度。",
    "settings.auto_refresh_current_hint": "每 {minutes} 分钟自动刷新一次当前账号额度。",
    "settings.low_quota_prompt_label": "低额度自动提示切换账号",
    "settings.low_quota_prompt_hint": "开启后，额度过低时会自动弹出切换账号确认与提示信息窗口。",
    "settings.auto_refresh_current_on": "开启",
    "settings.auto_refresh_current_off": "关闭",
    "settings.countdown_prefix": "剩余时间：",
    "dashboard.title": "配额仪表盘",
    "dashboard.subtitle": "快速查看账号整体配额情况与当前账号健康度。",
    "dashboard.total_accounts": "总账号数",
    "dashboard.avg_5h": "平均 5 小时配额",
    "dashboard.avg_7d": "平均 7 天配额",
    "dashboard.low_accounts": "低配额账号",
    "dashboard.low_list_title": "低配额账号列表",
    "dashboard.low_list_empty": "暂无低配额账号",
    "dashboard.current_title": "当前账号配额",
    "dashboard.current_name_empty": "暂无活动账号",
    "dashboard.current_5h": "5 小时剩余",
    "dashboard.current_7d": "7 天剩余",
    "dashboard.switch_button": "前往账号管理",
    "dialog.add_account.title": "添加新账号",
    "dialog.add_account.import_current": "导入当前登录账号",
    "dialog.add_account.import_oauth": "快速导入已有OAuth",
    "dialog.add_account.login_new": "登录新账号"
  };

  const dom = {
    brandTitle: document.getElementById("brandTitle"),
    tabBtnDashboard: document.getElementById("tabBtnDashboard"),
    tabBtnAccounts: document.getElementById("tabBtnAccounts"),
    tabBtnAbout: document.getElementById("tabBtnAbout"),
    tabBtnSettings: document.getElementById("tabBtnSettings"),
    addCurrentBtn: document.getElementById("addCurrentBtn"),
    refreshBtn: document.getElementById("refreshBtn"),
    importBtn: document.getElementById("importBtn"),
    exportBtn: document.getElementById("exportBtn"),
    searchInput: document.getElementById("searchInput"),
    groupAllBtn: document.getElementById("groupAllBtn"),
    groupPersonalBtn: document.getElementById("groupPersonalBtn"),
    groupBusinessBtn: document.getElementById("groupBusinessBtn"),
    thAccount: document.getElementById("thAccount"),
    thQuota: document.getElementById("thQuota"),
    thRecent: document.getElementById("thRecent"),
    thAction: document.getElementById("thAction"),
    aboutTitle: document.getElementById("aboutTitle"),
    aboutSubtitle: document.getElementById("aboutSubtitle"),
    aboutAuthorLabel: document.getElementById("aboutAuthorLabel"),
    aboutAuthorValue: document.getElementById("aboutAuthorValue"),
    aboutRepoLabel: document.getElementById("aboutRepoLabel"),
    aboutRepoLink: document.getElementById("aboutRepoLink"),
    checkUpdateBtn: document.getElementById("checkUpdateBtn"),
    versionText: document.getElementById("versionText"),
    settingsTitle: document.getElementById("settingsTitle"),
    settingsSub: document.getElementById("settingsSub"),
    settingsTabGeneralBtn: document.getElementById("settingsTabGeneralBtn"),
    settingsTabAccountBtn: document.getElementById("settingsTabAccountBtn"),
    settingsPaneGeneral: document.getElementById("settingsPaneGeneral"),
    settingsPaneAccount: document.getElementById("settingsPaneAccount"),
    settingsLanguageLabel: document.getElementById("settingsLanguageLabel"),
    settingsIdeLabel: document.getElementById("settingsIdeLabel"),
    settingsThemeLabel: document.getElementById("settingsThemeLabel"),
    settingsThemeHint: document.getElementById("settingsThemeHint"),
    themeAutoBtn: document.getElementById("themeAutoBtn"),
    themeLightBtn: document.getElementById("themeLightBtn"),
    themeDarkBtn: document.getElementById("themeDarkBtn"),
    settingsAutoUpdateLabel: document.getElementById("settingsAutoUpdateLabel"),
    settingsAutoUpdateHint: document.getElementById("settingsAutoUpdateHint"),
    settingsQuotaSectionTitle: document.getElementById("settingsQuotaSectionTitle"),
    settingsAutoRefreshAllLabel: document.getElementById("settingsAutoRefreshAllLabel"),
    settingsAutoRefreshAllHint: document.getElementById("settingsAutoRefreshAllHint"),
    settingsAllRefreshCountdown: document.getElementById("settingsAllRefreshCountdown"),
    settingsAllMinutesLabel: document.getElementById("settingsAllMinutesLabel"),
    autoRefreshAllMinutesInput: document.getElementById("autoRefreshAllMinutesInput"),
    settingsAutoRefreshCurrentLabel: document.getElementById("settingsAutoRefreshCurrentLabel"),
    settingsAutoRefreshCurrentHint: document.getElementById("settingsAutoRefreshCurrentHint"),
    settingsCurrentRefreshCountdown: document.getElementById("settingsCurrentRefreshCountdown"),
    settingsCurrentMinutesLabel: document.getElementById("settingsCurrentMinutesLabel"),
    autoRefreshCurrentMinutesInput: document.getElementById("autoRefreshCurrentMinutesInput"),
    settingsLowQuotaPromptLabel: document.getElementById("settingsLowQuotaPromptLabel"),
    settingsLowQuotaPromptHint: document.getElementById("settingsLowQuotaPromptHint"),
    lowQuotaAutoPromptToggle: document.getElementById("lowQuotaAutoPromptToggle"),
    languageOptions: document.getElementById("languageOptions"),
    ideOptions: document.getElementById("ideOptions"),
    autoUpdateToggle: document.getElementById("autoUpdateToggle"),
    autoRefreshCurrentToggle: document.getElementById("autoRefreshCurrentToggle"),
    dashboardTitle: document.getElementById("dashboardTitle"),
    dashboardSubtitle: document.getElementById("dashboardSubtitle"),
    dashTotalLabel: document.getElementById("dashTotalLabel"),
    dashAvg5Label: document.getElementById("dashAvg5Label"),
    dashAvg7Label: document.getElementById("dashAvg7Label"),
    dashLowLabel: document.getElementById("dashLowLabel"),
    dashLowListTitle: document.getElementById("dashLowListTitle"),
    dashTotalValue: document.getElementById("dashTotalValue"),
    dashAvg5Value: document.getElementById("dashAvg5Value"),
    dashAvg7Value: document.getElementById("dashAvg7Value"),
    dashLowValue: document.getElementById("dashLowValue"),
    dashLowList: document.getElementById("dashLowList"),
    dashCurrentTitle: document.getElementById("dashCurrentTitle"),
    dashCurrentName: document.getElementById("dashCurrentName"),
    dashCurrent5Label: document.getElementById("dashCurrent5Label"),
    dashCurrent7Label: document.getElementById("dashCurrent7Label"),
    dashCurrent5Value: document.getElementById("dashCurrent5Value"),
    dashCurrent7Value: document.getElementById("dashCurrent7Value"),
    dashCurrent5Bar: document.getElementById("dashCurrent5Bar"),
    dashCurrent7Bar: document.getElementById("dashCurrent7Bar"),
    dashboardSwitchBtn: document.getElementById("dashboardSwitchBtn"),
    countText: document.getElementById("countText"),
    accountsBody: document.getElementById("accountsBody"),
    logEl: document.getElementById("log"),
    toastWrap: document.getElementById("toastWrap"),
    backupModal: document.getElementById("backupModal"),
    backupTitle: document.getElementById("backupTitle"),
    backupNameLabel: document.getElementById("backupNameLabel"),
    backupNameInput: document.getElementById("backupNameInput"),
    backupCancelBtn: document.getElementById("backupCancelBtn"),
    backupConfirmBtn: document.getElementById("backupConfirmBtn"),
    importAuthModal: document.getElementById("importAuthModal"),
    importAuthTitle: document.getElementById("importAuthTitle"),
    importAuthNameLabel: document.getElementById("importAuthNameLabel"),
    importAuthNameInput: document.getElementById("importAuthNameInput"),
    importAuthCancelBtn: document.getElementById("importAuthCancelBtn"),
    importAuthConfirmBtn: document.getElementById("importAuthConfirmBtn"),
    addAccountModal: document.getElementById("addAccountModal"),
    addAccountTitle: document.getElementById("addAccountTitle"),
    addAccountImportCurrentBtn: document.getElementById("addAccountImportCurrentBtn"),
    addAccountImportOAuthBtn: document.getElementById("addAccountImportOAuthBtn"),
    addAccountLoginNewBtn: document.getElementById("addAccountLoginNewBtn"),
    addAccountCancelBtn: document.getElementById("addAccountCancelBtn"),
    debugPanel: document.getElementById("debugPanel"),
    debugTitle: document.getElementById("debugTitle"),
    debugNotifyBtn: document.getElementById("debugNotifyBtn"),
    confirmModal: document.getElementById("confirmModal"),
    confirmTitle: document.getElementById("confirmTitle"),
    confirmMessage: document.getElementById("confirmMessage"),
    confirmCancelBtn: document.getElementById("confirmCancelBtn"),
    confirmOkBtn: document.getElementById("confirmOkBtn")
  };

  const state = {
    appVersion: "v1.0.0",
    repoUrl: "https://github.com/isxlan0/Codex_AccountSwitch",
    debug: new URLSearchParams(location.search).get("debug") === "1",
    accounts: [],
    filteredAccounts: [],
    groupFilter: "all",
    confirmAction: null,
    confirmPersistent: false,
    currentLanguage: "zh-CN",
    currentIdeExe: "Code.exe",
    autoUpdate: true,
    autoRefreshCurrent: true,
    lowQuotaAutoPrompt: true,
    autoRefreshAllMinutes: 15,
    autoRefreshCurrentMinutes: 5,
    settingsSubTab: "general",
    themeMode: "auto",
    firstRun: false,
    languageIndex: [],
    i18n: {},
    didAutoCheckUpdate: false,
    updateCheckContext: "manual",
    refreshMode: "",
    refreshTargetKey: "",
    refreshBusyTimer: null,
    saveConfigTimer: null,
    configLoaded: false,
    hasPendingConfigWrite: false,
    pendingConfigSnapshot: null,
    pendingConfigAckTimer: null,
    lowQuotaPromptOpen: false,
    allRefreshRemainSec: -1,
    currentRefreshRemainSec: -1
  };

  const mediaDark = window.matchMedia ? window.matchMedia("(prefers-color-scheme: dark)") : null;

  function log(msg) {
    if (!state.debug) return;
    dom.logEl.textContent = `[${new Date().toLocaleTimeString()}] ${msg}\n` + dom.logEl.textContent;
  }

  function post(action, payload = {}) {
    if (window.chrome && window.chrome.webview) {
      window.chrome.webview.postMessage({ action, ...payload });
      log(`command sent: ${action}`);
    } else {
      log(`not running in WebView2: ${action}`);
    }
  }

  function showToast(message, level = "info") {
    const el = document.createElement("div");
    el.className = `toast ${level}`;
    el.textContent = message;
    dom.toastWrap.appendChild(el);
    setTimeout(() => {
      el.style.opacity = "0";
      el.style.transform = "translateY(-6px)";
    }, 2600);
    setTimeout(() => el.remove(), 3000);
  }

  function escapeHtml(text) {
    return String(text)
      .replaceAll("&", "&amp;")
      .replaceAll("<", "&lt;")
      .replaceAll(">", "&gt;")
      .replaceAll("\"", "&quot;")
      .replaceAll("'", "&#39;");
  }

  function t(key, vars = {}) {
    const langCode = String(state.currentLanguage || "").toLowerCase();
    const isZhCn = langCode === "zh-cn";
    let text = state.i18n[key];
    if (isZhCn && ZH_FALLBACK_I18N[key] !== undefined) {
      text = ZH_FALLBACK_I18N[key];
    }
    if (typeof text === "string") {
      text = text.replace(/\\?u([0-9a-fA-F]{4})/g, (_, hex) => String.fromCharCode(parseInt(hex, 16)));
    }
    if (typeof text !== "string") text = DEFAULT_I18N[key];
    if (typeof text !== "string") text = key;
    Object.keys(vars).forEach((k) => {
      text = text.replaceAll(`{${k}}`, String(vars[k]));
    });
    return text;
  }

  function normalizeMultilineText(text) {
    return String(text || "")
      .replaceAll("\\r\\n", "\n")
      .replaceAll("\\n", "\n")
      .replaceAll("\\r", "\n");
  }

  function initLanguageIndexFallback() {
    state.languageIndex = [{ code: "zh-CN", name: "简体中文", file: "zh-CN.json" }];
  }

  function findLanguageMeta(code) {
    return state.languageIndex.find((x) => String(x.code).toLowerCase() === String(code).toLowerCase());
  }

  function applyLanguageStrings(code, strings) {
    state.i18n = { ...DEFAULT_I18N, ...(strings || {}) };
    state.currentLanguage = code || state.currentLanguage || "zh-CN";
    applyI18n();
    refreshSettingsOptions();
    renderAccounts();
  }

  function requestLanguagePack(code) {
    post("get_language_pack", { code: code || state.currentLanguage || "zh-CN" });
  }

  function requestUpdateCheck(context = "manual") {
    state.updateCheckContext = context;
    dom.versionText.textContent = t("about.version_checking", { version: state.appVersion });
    post("check_update");
  }

  function promptUpdateDialog(info) {
    const notes = normalizeMultilineText(info?.notes).trim() || "-";
    const message = [
      `${t("update.dialog.current_label")}: ${state.appVersion}`,
      `${t("update.dialog.latest_label")}: ${info?.latest || ""}`,
      "",
      t("update.dialog.confirm_question"),
      "",
      `${t("update.dialog.notes_label")}:`,
      notes
    ].join("\n");
    openConfirm({
      title: t("update.dialog.title"),
      message: normalizeMultilineText(message),
      onConfirm: () => {
        const target = info?.downloadUrl || info?.url || `${state.repoUrl}/releases/latest`;
        post("open_external_url", { url: target });
      }
    });
  }

  function getIdeDisplayName(exe) {
    const map = {
      "code.exe": "VSCode",
      "trae.exe": "Trae",
      "kiro.exe": "Kiro",
      "antigravity.exe": "Antigravity"
    };
    const key = String(exe || "").toLowerCase();
    return map[key] || String(exe || "VSCode").replace(".exe", "");
  }

  function switchTab(tab) {
    document.querySelectorAll(".tab-btn").forEach((x) => x.classList.toggle("active", x.getAttribute("data-tab") === tab));
    document.querySelectorAll(".tab-panel").forEach((x) => x.classList.toggle("active", x.id === `tab-${tab}`));
  }

  function switchSettingsSubTab(tab) {
    state.settingsSubTab = tab === "account" ? "account" : "general";
    document.querySelectorAll("[data-settings-tab]").forEach((x) => {
      x.classList.toggle("active", x.getAttribute("data-settings-tab") === state.settingsSubTab);
    });
    dom.settingsPaneGeneral.classList.toggle("active", state.settingsSubTab === "general");
    dom.settingsPaneAccount.classList.toggle("active", state.settingsSubTab === "account");
  }

  function openConfirm(options) {
    dom.confirmTitle.textContent = options?.title || t("dialog.confirm.title");
    dom.confirmMessage.textContent = options?.message || t("dialog.confirm.default_message");
    state.confirmAction = typeof options?.onConfirm === "function" ? options.onConfirm : null;
    state.confirmPersistent = options?.persistent === true;
    dom.confirmModal.classList.add("show");
  }

  function closeConfirm(force = false) {
    if (!force && state.confirmPersistent) {
      return;
    }
    if (state.lowQuotaPromptOpen) {
      post("cancel_low_quota_switch");
      state.lowQuotaPromptOpen = false;
    }
    dom.confirmModal.classList.remove("show");
    state.confirmAction = null;
    state.confirmPersistent = false;
  }

  function normalizeThemeMode(mode) {
    const v = String(mode || "").toLowerCase();
    if (v === "light" || v === "dark" || v === "auto") return v;
    return "auto";
  }

  function resolveEffectiveTheme() {
    if (state.themeMode === "dark") return "dark";
    if (state.themeMode === "light") return "light";
    return mediaDark && mediaDark.matches ? "dark" : "light";
  }

  function applyTheme() {
    document.documentElement.setAttribute("data-theme", resolveEffectiveTheme());
  }

  function syncLayoutDensity() {
    document.body.classList.toggle("compact-ui", window.innerWidth < 1120);
  }

  function renderLanguageOptions() {
    dom.languageOptions.innerHTML = "";
    for (const lang of state.languageIndex) {
      const btn = document.createElement("button");
      btn.className = "option-btn";
      btn.setAttribute("data-lang-option", lang.code);
      btn.textContent = lang.name;
      btn.addEventListener("click", () => {
        state.currentLanguage = lang.code;
        refreshSettingsOptions();
        requestLanguagePack(lang.code);
        queueSaveConfig();
      });
      dom.languageOptions.appendChild(btn);
    }
  }

  function renderIdeOptions() {
    dom.ideOptions.innerHTML = "";
    for (const exe of IDE_LIST) {
      const btn = document.createElement("button");
      btn.className = "option-btn";
      btn.setAttribute("data-ide-option", exe);
      btn.textContent = t(`ide.${exe}`) || getIdeDisplayName(exe);
      btn.addEventListener("click", () => {
        state.currentIdeExe = exe;
        refreshSettingsOptions();
        queueSaveConfig();
      });
      dom.ideOptions.appendChild(btn);
    }
  }

  function refreshSettingsOptions() {
    document.querySelectorAll("[data-lang-option]").forEach((x) => {
      x.classList.toggle("active", x.getAttribute("data-lang-option") === state.currentLanguage);
    });
    document.querySelectorAll("[data-ide-option]").forEach((x) => {
      x.classList.toggle("active", x.getAttribute("data-ide-option") === state.currentIdeExe);
    });
    dom.autoUpdateToggle.checked = state.autoUpdate;
    document.querySelectorAll("[data-theme-option]").forEach((x) => {
      x.classList.toggle("active", x.getAttribute("data-theme-option") === state.themeMode);
    });
    dom.autoRefreshCurrentToggle.checked = state.autoRefreshCurrent;
    dom.lowQuotaAutoPromptToggle.checked = state.lowQuotaAutoPrompt;
    dom.settingsAutoRefreshAllHint.textContent = t("settings.auto_refresh_all_hint", { minutes: state.autoRefreshAllMinutes });
    dom.settingsAutoRefreshCurrentHint.textContent = t("settings.auto_refresh_current_hint", { minutes: state.autoRefreshCurrentMinutes });
    dom.autoRefreshAllMinutesInput.value = String(state.autoRefreshAllMinutes);
    dom.autoRefreshCurrentMinutesInput.value = String(state.autoRefreshCurrentMinutes);
    switchSettingsSubTab(state.settingsSubTab);
  }

  function applyI18n() {
    dom.tabBtnDashboard.textContent = t("tab.dashboard");
    dom.tabBtnAccounts.textContent = t("tab.accounts");
    dom.tabBtnAbout.textContent = t("tab.about");
    dom.tabBtnSettings.textContent = t("tab.settings");
    dom.addCurrentBtn.textContent = t("toolbar.add_current");
    dom.refreshBtn.textContent = t("toolbar.refresh");
    dom.importBtn.textContent = t("toolbar.import");
    dom.exportBtn.textContent = t("toolbar.export");
    dom.searchInput.placeholder = t("search.placeholder");
    dom.groupAllBtn.textContent = t("group.all");
    dom.groupPersonalBtn.textContent = t("group.personal");
    dom.groupBusinessBtn.textContent = t("group.business");
    dom.thAccount.textContent = t("table.account");
    dom.thQuota.textContent = t("table.quota");
    dom.thRecent.textContent = t("table.recent");
    dom.thAction.textContent = t("table.action");
    dom.aboutTitle.textContent = t("about.title");
    dom.aboutSubtitle.textContent = t("about.subtitle");
    dom.aboutAuthorLabel.textContent = t("about.author_label");
    dom.aboutAuthorValue.textContent = t("about.author_name");
    dom.aboutRepoLabel.textContent = t("about.repo_label");
    dom.aboutRepoLink.textContent = t("about.repo_link");
    dom.checkUpdateBtn.textContent = t("about.check_update");
    dom.settingsTitle.textContent = t("settings.title");
    dom.settingsSub.textContent = t("settings.subtitle");
    dom.settingsTabGeneralBtn.textContent = t("settings.tab.general");
    dom.settingsTabAccountBtn.textContent = t("settings.tab.account");
    dom.settingsLanguageLabel.textContent = t("settings.language_label");
    dom.settingsIdeLabel.textContent = t("settings.ide_label");
    dom.settingsThemeLabel.textContent = t("settings.theme_label");
    dom.settingsThemeHint.textContent = t("settings.theme_hint");
    dom.themeAutoBtn.textContent = t("settings.theme_auto");
    dom.themeLightBtn.textContent = t("settings.theme_light");
    dom.themeDarkBtn.textContent = t("settings.theme_dark");
    dom.settingsAutoUpdateLabel.textContent = t("settings.auto_update_label");
    dom.settingsAutoUpdateHint.textContent = t("settings.auto_update_hint");
    dom.settingsQuotaSectionTitle.textContent = t("settings.quota_refresh_section");
    dom.settingsAutoRefreshAllLabel.textContent = t("settings.auto_refresh_all_label");
    dom.settingsAutoRefreshAllHint.textContent = t("settings.auto_refresh_all_hint", { minutes: state.autoRefreshAllMinutes });
    dom.settingsAllMinutesLabel.textContent = t("settings.refresh_minutes_label");
    dom.settingsAutoRefreshCurrentLabel.textContent = t("settings.auto_refresh_current_label");
    dom.settingsAutoRefreshCurrentHint.textContent = t("settings.auto_refresh_current_hint", { minutes: state.autoRefreshCurrentMinutes });
    dom.settingsLowQuotaPromptLabel.textContent = t("settings.low_quota_prompt_label");
    dom.settingsLowQuotaPromptHint.textContent = t("settings.low_quota_prompt_hint");
    dom.settingsCurrentMinutesLabel.textContent = t("settings.refresh_minutes_label");
    dom.autoRefreshAllMinutesInput.placeholder = t("settings.minutes_hint");
    dom.autoRefreshCurrentMinutesInput.placeholder = t("settings.minutes_hint");
    dom.backupTitle.textContent = t("dialog.backup.title");
    dom.backupNameLabel.textContent = t("dialog.backup.name_label");
    dom.backupNameInput.placeholder = t("dialog.backup.name_placeholder");
    dom.importAuthTitle.textContent = t("dialog.import_auth.title");
    dom.importAuthNameLabel.textContent = t("dialog.import_auth.name_label");
    dom.importAuthNameInput.placeholder = t("dialog.import_auth.name_placeholder");
    dom.importAuthCancelBtn.textContent = t("dialog.common.cancel");
    dom.importAuthConfirmBtn.textContent = t("dialog.common.confirm");
    dom.backupCancelBtn.textContent = t("dialog.common.cancel");
    dom.backupConfirmBtn.textContent = t("dialog.common.save");
    dom.confirmCancelBtn.textContent = t("dialog.common.cancel");
    dom.confirmOkBtn.textContent = t("dialog.common.confirm");
    dom.addAccountTitle.textContent = t("dialog.add_account.title");
    dom.addAccountImportCurrentBtn.textContent = t("dialog.add_account.import_current");
    dom.addAccountImportOAuthBtn.textContent = t("dialog.add_account.import_oauth");
    dom.addAccountLoginNewBtn.textContent = t("dialog.add_account.login_new");
    dom.addAccountCancelBtn.textContent = t("dialog.common.cancel");
    dom.confirmTitle.textContent = t("dialog.confirm.title");
    dom.confirmMessage.textContent = t("dialog.confirm.default_message");
    dom.brandTitle.textContent = t("app.brand");
    dom.debugTitle.textContent = t("debug.title");
    dom.debugNotifyBtn.textContent = t("debug.notify");
    dom.dashboardTitle.textContent = t("dashboard.title");
    dom.dashboardSubtitle.textContent = t("dashboard.subtitle");
    dom.dashTotalLabel.textContent = t("dashboard.total_accounts");
    dom.dashAvg5Label.textContent = t("dashboard.avg_5h");
    dom.dashAvg7Label.textContent = t("dashboard.avg_7d");
    dom.dashLowLabel.textContent = t("dashboard.low_accounts");
    dom.dashLowListTitle.textContent = t("dashboard.low_list_title");
    dom.dashCurrentTitle.textContent = t("dashboard.current_title");
    dom.dashCurrent5Label.textContent = t("dashboard.current_5h");
    dom.dashCurrent7Label.textContent = t("dashboard.current_7d");
    dom.dashboardSwitchBtn.textContent = t("dashboard.switch_button");
    dom.versionText.textContent = t("about.version_prefix", { version: state.appVersion });
    renderRefreshCountdowns();
    renderIdeOptions();
    switchSettingsSubTab(state.settingsSubTab);
    renderDashboard();
    applyCountText();
    switchTab(document.querySelector(".tab-btn.active")?.getAttribute("data-tab") || "dashboard");
  }

  function applyCountText() {
    const total = state.filteredAccounts.length;
    dom.countText.textContent = total > 0 ? t("count.format", { total }) : t("count.empty");
  }

  function mapStatusMessage(msg) {
    const code = String(msg?.code || "");
    if (!code) return String(msg?.message || "");
    const key = `status_code.${code}`;
    if (state.i18n[key]) {
      if (code === "restart_failed") {
        return t(key, { ide: getIdeDisplayName(state.currentIdeExe) });
      }
      return t(key);
    }
    return String(msg?.message || code);
  }

  function makeAccountKey(name, group) {
    return `${String(group || "personal").toLowerCase()}::${String(name || "")}`;
  }

  function setRefreshBusy(mode = "", accountKey = "") {
    state.refreshMode = mode;
    state.refreshTargetKey = accountKey || "";

    if (state.refreshBusyTimer) {
      clearTimeout(state.refreshBusyTimer);
      state.refreshBusyTimer = null;
    }

    const isBusy = mode === "all" || mode === "account";
    dom.refreshBtn.disabled = isBusy;
    dom.refreshBtn.classList.toggle("loading", mode === "all");

    if (isBusy) {
      state.refreshBusyTimer = setTimeout(() => {
        setRefreshBusy("", "");
      }, 20000);
    }

    renderAccounts();
  }

  function formatRemain(v) {
    return Number.isFinite(Number(v)) && Number(v) >= 0 ? `${Number(v)}%` : "-";
  }

  function toPercentNumber(v) {
    const n = Number(v);
    if (!Number.isFinite(n)) return null;
    if (n < 0) return null;
    if (n > 100) return 100;
    return n;
  }

  function formatPercentValue(v) {
    const n = toPercentNumber(v);
    return n === null ? "-" : `${Math.round(n)}%`;
  }

  function renderDashboard() {
    const accounts = Array.isArray(state.accounts) ? state.accounts : [];
    const usable = accounts.filter((x) => x && x.usageOk);
    const sum5 = usable.reduce((acc, x) => acc + (toPercentNumber(x.quota5hRemainingPercent) ?? 0), 0);
    const sum7 = usable.reduce((acc, x) => acc + (toPercentNumber(x.quota7dRemainingPercent) ?? 0), 0);
    const avg5 = usable.length > 0 ? sum5 / usable.length : null;
    const avg7 = usable.length > 0 ? sum7 / usable.length : null;
    const lowCount = usable.filter((x) => {
      const q5 = toPercentNumber(x.quota5hRemainingPercent);
      return q5 !== null && q5 < 20;
    }).length;
    const lowAccounts = usable
      .map((x) => ({ name: String(x.name || ""), q5: toPercentNumber(x.quota5hRemainingPercent) }))
      .filter((x) => x.q5 !== null && x.q5 < 20)
      .sort((a, b) => a.q5 - b.q5);

    dom.dashTotalValue.textContent = String(accounts.length);
    dom.dashAvg5Value.textContent = formatPercentValue(avg5);
    dom.dashAvg7Value.textContent = formatPercentValue(avg7);
    dom.dashLowValue.textContent = String(lowCount);
    if (lowAccounts.length > 0) {
      dom.dashLowList.innerHTML = lowAccounts.slice(0, 8).map((x) => `
        <div class="dashboard-low-item">
          <span class="dashboard-low-name">${escapeHtml(x.name || "-")}</span>
          <span class="dashboard-low-quota">${escapeHtml(formatPercentValue(x.q5))}</span>
        </div>
      `).join("");
    } else {
      dom.dashLowList.innerHTML = `<div class="dashboard-low-empty">${escapeHtml(t("dashboard.low_list_empty"))}</div>`;
    }

    const current = accounts.find((x) => x && x.isCurrent);
    if (!current) {
      dom.dashCurrentName.textContent = t("dashboard.current_name_empty");
      dom.dashCurrent5Value.textContent = "-";
      dom.dashCurrent7Value.textContent = "-";
      dom.dashCurrent5Bar.style.width = "0%";
      dom.dashCurrent7Bar.style.width = "0%";
      return;
    }

    const q5 = toPercentNumber(current.quota5hRemainingPercent);
    const q7 = toPercentNumber(current.quota7dRemainingPercent);
    dom.dashCurrentName.textContent = current.name || t("dashboard.current_name_empty");
    dom.dashCurrent5Value.textContent = formatPercentValue(q5);
    dom.dashCurrent7Value.textContent = formatPercentValue(q7);
    dom.dashCurrent5Bar.style.width = `${q5 === null ? 0 : q5}%`;
    dom.dashCurrent7Bar.style.width = `${q7 === null ? 0 : q7}%`;
  }

  function formatHoursFromSeconds(v) {
    if (!Number.isFinite(Number(v)) || Number(v) < 0) return "-";
    return (Number(v) / 3600).toFixed(1);
  }

  function formatCountdown(sec) {
    const total = Number(sec);
    if (!Number.isFinite(total) || total < 0) return t("refresh.countdown_default");
    const mm = Math.floor(total / 60).toString().padStart(2, "0");
    const ss = Math.floor(total % 60).toString().padStart(2, "0");
    return `${mm}:${ss}`;
  }

  function clampRefreshMinutes(v, fallback = 5) {
    const n = Number(v);
    if (!Number.isFinite(n)) return fallback;
    if (n < 1) return 1;
    if (n > 240) return 240;
    return Math.round(n);
  }

  function buildConfigPayload() {
    return {
      language: state.currentLanguage,
      ideExe: state.currentIdeExe,
      autoUpdate: state.autoUpdate,
      autoRefreshCurrent: state.autoRefreshCurrent,
      lowQuotaAutoPrompt: state.lowQuotaAutoPrompt,
      autoRefreshAllMinutes: state.autoRefreshAllMinutes,
      autoRefreshCurrentMinutes: state.autoRefreshCurrentMinutes,
      theme: state.themeMode
    };
  }

  function clearPendingConfigState() {
    state.hasPendingConfigWrite = false;
    state.pendingConfigSnapshot = null;
    if (state.pendingConfigAckTimer) {
      clearTimeout(state.pendingConfigAckTimer);
      state.pendingConfigAckTimer = null;
    }
  }

  function configMatchesPending(msg) {
    if (!state.pendingConfigSnapshot) return false;
    const pending = state.pendingConfigSnapshot;
    const msgLanguage = msg.language || "zh-CN";
    const msgIdeExe = msg.ideExe || "Code.exe";
    const msgAutoUpdate = msg.autoUpdate !== false && msg.autoUpdate !== "false";
    const msgAutoRefreshCurrent = msg.autoRefreshCurrent !== false && msg.autoRefreshCurrent !== "false";
    const msgLowQuotaAutoPrompt = msg.lowQuotaAutoPrompt !== false && msg.lowQuotaAutoPrompt !== "false";
    const msgAutoRefreshAllMinutes = clampRefreshMinutes(msg.autoRefreshAllMinutes, 15);
    const msgAutoRefreshCurrentMinutes = clampRefreshMinutes(msg.autoRefreshCurrentMinutes, 5);
    const msgTheme = normalizeThemeMode(msg.theme || "auto");
    return msgLanguage === pending.language
      && msgIdeExe === pending.ideExe
      && msgAutoUpdate === pending.autoUpdate
      && msgAutoRefreshCurrent === pending.autoRefreshCurrent
      && msgLowQuotaAutoPrompt === pending.lowQuotaAutoPrompt
      && msgAutoRefreshAllMinutes === pending.autoRefreshAllMinutes
      && msgAutoRefreshCurrentMinutes === pending.autoRefreshCurrentMinutes
      && msgTheme === pending.theme;
  }

  function saveConfigNow() {
    if (!state.configLoaded) return;
    const payload = buildConfigPayload();
    state.hasPendingConfigWrite = true;
    state.pendingConfigSnapshot = payload;
    if (state.pendingConfigAckTimer) clearTimeout(state.pendingConfigAckTimer);
    state.pendingConfigAckTimer = setTimeout(() => {
      clearPendingConfigState();
    }, 5000);
    post("set_config", payload);
  }

  function queueSaveConfig() {
    if (state.saveConfigTimer) clearTimeout(state.saveConfigTimer);
    state.hasPendingConfigWrite = true;
    state.saveConfigTimer = setTimeout(() => {
      saveConfigNow();
      state.saveConfigTimer = null;
    }, 250);
  }

  function flushPendingConfigWrite() {
    if (!state.configLoaded) return;
    if (state.saveConfigTimer) {
      clearTimeout(state.saveConfigTimer);
      state.saveConfigTimer = null;
      saveConfigNow();
      return;
    }
    if (state.hasPendingConfigWrite) {
      saveConfigNow();
    }
  }

  function renderRefreshCountdowns() {
    dom.settingsAllRefreshCountdown.textContent = `${t("settings.countdown_prefix")}${formatCountdown(state.allRefreshRemainSec)}`;
    dom.settingsCurrentRefreshCountdown.textContent = state.autoRefreshCurrent
      ? `${t("settings.countdown_prefix")}${formatCountdown(state.currentRefreshRemainSec)}`
      : `${t("settings.countdown_prefix")}${t("refresh.disabled")}`;
  }

  function renderAccounts() {
    if (!state.filteredAccounts.length) {
      dom.accountsBody.innerHTML = `<tr><td colspan="4" style="text-align:center;color:#6b7c93;">${escapeHtml(t("accounts.empty"))}</td></tr>`;
      return;
    }

    const shortName = (name) => {
      const v = String(name || "");
      return v.length > 14 ? `${v.slice(0, 14)}...` : v;
    };

    dom.accountsBody.innerHTML = state.filteredAccounts.map((item) => {
      const isThisRefreshing = state.refreshMode === "account" && state.refreshTargetKey === makeAccountKey(item.name, item.group);
      const disableRefreshAction = state.refreshMode === "all" || isThisRefreshing;
      return `
      <tr>
        <td>
          <div class="account-cell" title="${escapeHtml(item.name)}">
            <span class="account-name">${escapeHtml(shortName(item.name))}</span>
            ${item.isCurrent ? `<span class="tag">${escapeHtml(t("tag.current"))}</span>` : ""}
            <span class="tag group ${item.group === "business" ? "business" : "personal"}">${escapeHtml(item.group === "business" ? t("tag.group_business") : t("tag.group_personal"))}</span>
          </div>
        </td>
        <td>
          <div class="quota-box">
            <span class="quota-name">${escapeHtml(t("quota.gpt"))}</span>
            ${item.usageOk
              ? escapeHtml(t("quota.format", {
                  q5: formatRemain(item.quota5hRemainingPercent),
                  q7: formatRemain(item.quota7dRemainingPercent),
                  r5: formatHoursFromSeconds(item.quota5hResetAfterSeconds),
                  r7: formatHoursFromSeconds(item.quota7dResetAfterSeconds)
                }))
              : escapeHtml(t("quota.placeholder"))}
          </div>
        </td>
        <td>${escapeHtml(item.updatedAt || "-")}</td>
        <td>
          <div class="actions">
            <button class="btn-action switch" data-action="switch" data-name="${escapeHtml(item.name)}" data-group="${escapeHtml(item.group || "personal")}" title="${escapeHtml(t("action.switch_title"))}">${escapeHtml(t("action.switch"))}</button>
            <button class="btn-action refresh ${isThisRefreshing ? "loading" : ""}" data-action="refresh" data-name="${escapeHtml(item.name)}" data-group="${escapeHtml(item.group || "personal")}" title="${escapeHtml(t("action.refresh_title"))}" ${disableRefreshAction ? "disabled" : ""}>${escapeHtml(t("action.refresh"))}</button>
            <button class="btn-action delete" data-action="delete" data-name="${escapeHtml(item.name)}" data-group="${escapeHtml(item.group || "personal")}" title="${escapeHtml(t("action.delete_title"))}">${escapeHtml(t("action.delete"))}</button>
          </div>
        </td>
      </tr>
    `;
    }).join("");
  }

  function applySearch() {
    const q = dom.searchInput.value.trim().toLowerCase();
    let list = [...state.accounts];
    if (state.groupFilter !== "all") list = list.filter((x) => x.group === state.groupFilter);
    state.filteredAccounts = !q ? list : list.filter((x) => String(x.name || "").toLowerCase().includes(q));
    applyCountText();
    renderAccounts();
  }

  function bindEvents() {
    document.addEventListener("contextmenu", (e) => e.preventDefault());
    document.addEventListener("dragstart", (e) => e.preventDefault());

    document.querySelectorAll(".tab-btn").forEach((btn) => {
      btn.addEventListener("click", () => switchTab(btn.getAttribute("data-tab") || "accounts"));
    });
    document.querySelectorAll("[data-settings-tab]").forEach((btn) => {
      btn.addEventListener("click", () => {
        switchSettingsSubTab(btn.getAttribute("data-settings-tab") || "general");
      });
    });

    dom.searchInput.addEventListener("input", applySearch);
    dom.dashboardSwitchBtn.addEventListener("click", () => switchTab("accounts"));
    dom.addCurrentBtn.addEventListener("click", () => {
      dom.addAccountModal.classList.add("show");
    });

    document.querySelectorAll("[data-group-filter]").forEach((btn) => {
      btn.addEventListener("click", () => {
        state.groupFilter = btn.getAttribute("data-group-filter") || "all";
        document.querySelectorAll("[data-group-filter]").forEach((x) => x.classList.remove("active"));
        btn.classList.add("active");
        applySearch();
      });
    });

    dom.accountsBody.addEventListener("click", (e) => {
      const target = e.target.closest("button[data-action]");
      if (!target) return;
      if (target.disabled) return;
      const action = target.getAttribute("data-action");
      const name = target.getAttribute("data-name");
      const group = target.getAttribute("data-group") || "personal";
      if (!name) return;
      if (action === "switch") {
        post("switch_account", {
          account: name,
          group,
          language: state.currentLanguage,
          ideExe: state.currentIdeExe
        });
      } else if (action === "refresh") {
        if (state.refreshMode) return;
        setRefreshBusy("account", makeAccountKey(name, group));
        post("refresh_account", { account: name, group });
      } else if (action === "delete") {
        openConfirm({
          title: t("dialog.delete.title"),
          message: t("dialog.delete.message", { name }),
          onConfirm: () => post("delete_account", { account: name, group })
        });
      }
    });

    dom.autoUpdateToggle.addEventListener("change", () => {
      state.autoUpdate = dom.autoUpdateToggle.checked;
      refreshSettingsOptions();
      queueSaveConfig();
    });

    document.querySelectorAll("[data-theme-option]").forEach((btn) => {
      btn.addEventListener("click", () => {
        state.themeMode = normalizeThemeMode(btn.getAttribute("data-theme-option"));
        refreshSettingsOptions();
        applyTheme();
        queueSaveConfig();
      });
    });

    dom.autoRefreshCurrentToggle.addEventListener("change", () => {
      state.autoRefreshCurrent = dom.autoRefreshCurrentToggle.checked;
      refreshSettingsOptions();
      renderRefreshCountdowns();
      queueSaveConfig();
    });
    dom.lowQuotaAutoPromptToggle.addEventListener("change", () => {
      state.lowQuotaAutoPrompt = dom.lowQuotaAutoPromptToggle.checked;
      if (!state.lowQuotaAutoPrompt && state.lowQuotaPromptOpen) {
        closeConfirm(true);
      }
      refreshSettingsOptions();
      queueSaveConfig();
    });

    const handleAllMinutesChanged = () => {
      state.autoRefreshAllMinutes = clampRefreshMinutes(dom.autoRefreshAllMinutesInput.value, state.autoRefreshAllMinutes || 15);
      refreshSettingsOptions();
      queueSaveConfig();
    };
    const handleCurrentMinutesChanged = () => {
      state.autoRefreshCurrentMinutes = clampRefreshMinutes(dom.autoRefreshCurrentMinutesInput.value, state.autoRefreshCurrentMinutes || 5);
      refreshSettingsOptions();
      queueSaveConfig();
    };
    dom.autoRefreshAllMinutesInput.addEventListener("input", handleAllMinutesChanged);
    dom.autoRefreshAllMinutesInput.addEventListener("change", handleAllMinutesChanged);
    dom.autoRefreshCurrentMinutesInput.addEventListener("input", handleCurrentMinutesChanged);
    dom.autoRefreshCurrentMinutesInput.addEventListener("change", handleCurrentMinutesChanged);

    dom.backupCancelBtn.addEventListener("click", () => dom.backupModal.classList.remove("show"));
    dom.backupModal.addEventListener("click", (e) => {
      if (e.target === dom.backupModal) dom.backupModal.classList.remove("show");
    });
    dom.backupConfirmBtn.addEventListener("click", () => {
      const name = dom.backupNameInput.value.trim();
      if (!name) {
        dom.backupNameInput.focus();
        return;
      }
      if (name.length > 32) {
        showToast(t("status_code.name_too_long"), "warning");
        return;
      }
      post("backup_current", { name });
      dom.backupModal.classList.remove("show");
    });

    dom.importAuthCancelBtn.addEventListener("click", () => dom.importAuthModal.classList.remove("show"));
    dom.importAuthModal.addEventListener("click", (e) => {
      if (e.target === dom.importAuthModal) dom.importAuthModal.classList.remove("show");
    });
    dom.importAuthConfirmBtn.addEventListener("click", () => {
      const name = dom.importAuthNameInput.value.trim();
      if (!name) {
        dom.importAuthNameInput.focus();
        return;
      }
      if (name.length > 32) {
        showToast(t("status_code.name_too_long"), "warning");
        return;
      }
      post("import_auth_json", { name });
      dom.importAuthModal.classList.remove("show");
    });

    dom.addAccountCancelBtn.addEventListener("click", () => dom.addAccountModal.classList.remove("show"));
    dom.addAccountModal.addEventListener("click", (e) => {
      if (e.target === dom.addAccountModal) dom.addAccountModal.classList.remove("show");
    });
    dom.addAccountImportCurrentBtn.addEventListener("click", () => {
      dom.addAccountModal.classList.remove("show");
      dom.backupNameInput.value = "";
      dom.backupModal.classList.add("show");
      setTimeout(() => dom.backupNameInput.focus(), 10);
    });
    dom.addAccountImportOAuthBtn.addEventListener("click", () => {
      dom.addAccountModal.classList.remove("show");
      post("import_auth_json");
    });

    dom.confirmCancelBtn.addEventListener("click", () => closeConfirm(true));
    dom.confirmModal.addEventListener("click", (e) => {
      if (e.target === dom.confirmModal) closeConfirm();
    });
    dom.confirmOkBtn.addEventListener("click", () => {
      const fn = state.confirmAction;
      state.lowQuotaPromptOpen = false;
      closeConfirm(true);
      if (fn) fn();
    });

    dom.refreshBtn.addEventListener("click", () => {
      if (state.refreshMode) return;
      setRefreshBusy("all");
      post("refresh_accounts");
    });
    dom.importBtn.addEventListener("click", () => post("import_accounts"));
    dom.exportBtn.addEventListener("click", () => post("export_accounts"));
    dom.checkUpdateBtn.addEventListener("click", () => requestUpdateCheck("manual"));
    dom.aboutRepoLink.addEventListener("click", (e) => {
      e.preventDefault();
      const url = state.repoUrl || "https://github.com/isxlan0/Codex_AccountSwitch";
      post("open_external_url", { url });
    });
    dom.addAccountLoginNewBtn.addEventListener("click", () => {
      dom.addAccountModal.classList.remove("show");
      openConfirm({
        title: t("dialog.login_new.title"),
        message: t("dialog.login_new.message", { ide: getIdeDisplayName(state.currentIdeExe) }),
        onConfirm: () => post("login_new_account")
      });
    });

    dom.debugNotifyBtn.addEventListener("click", () => post("debug_test_low_quota_notify"));
  }

  function bindWebViewMessages() {
    if (!(window.chrome && window.chrome.webview)) return;

    window.chrome.webview.addEventListener("message", async (event) => {
      const msg = event.data;
      if (msg && typeof msg === "object" && msg.type === "accounts_list") {
        state.accounts = Array.isArray(msg.accounts)
          ? msg.accounts.map((x) => ({
              ...x,
              group: x.group === "business" ? "business" : "personal",
              isCurrent: x.isCurrent === true || x.isCurrent === "true",
              usageOk: x.usageOk === true || x.usageOk === "true"
            }))
          : [];
        applySearch();
        renderDashboard();
        log(`host: accounts loaded (${state.accounts.length})`);
        return;
      }

      if (msg && typeof msg === "object" && msg.type === "status") {
        log(`host status: level=${msg.level || "info"} code=${msg.code || ""}`);
        if (msg.code === "quota_refreshed" || msg.code === "account_quota_refreshed") {
          setRefreshBusy("", "");
        } else if (state.refreshMode && (msg.level === "error" || msg.level === "warning")) {
          setRefreshBusy("", "");
        }
        showToast(mapStatusMessage(msg), msg.level || "info");
        return;
      }

      if (msg && typeof msg === "object" && msg.type === "app_info") {
        state.appVersion = msg.version || state.appVersion;
        state.repoUrl = msg.repo || state.repoUrl;
        state.debug = state.debug || msg.debug === true || msg.debug === "true";
        dom.logEl.style.display = state.debug ? "block" : "none";
        dom.debugPanel.style.display = state.debug ? "block" : "none";
        dom.versionText.textContent = t("about.version_prefix", { version: state.appVersion });
        return;
      }

      if (msg && typeof msg === "object" && msg.type === "config") {
        if (!state.configLoaded) {
          state.currentLanguage = msg.language || "zh-CN";
          if (typeof msg.languageIndex === "number" && state.languageIndex[msg.languageIndex]) {
            state.currentLanguage = state.languageIndex[msg.languageIndex].code;
          }
          state.currentIdeExe = msg.ideExe || "Code.exe";
          state.autoUpdate = msg.autoUpdate !== false && msg.autoUpdate !== "false";
          state.autoRefreshCurrent = msg.autoRefreshCurrent !== false && msg.autoRefreshCurrent !== "false";
          state.lowQuotaAutoPrompt = msg.lowQuotaAutoPrompt !== false && msg.lowQuotaAutoPrompt !== "false";
          state.autoRefreshAllMinutes = clampRefreshMinutes(msg.autoRefreshAllMinutes, 15);
          state.autoRefreshCurrentMinutes = clampRefreshMinutes(msg.autoRefreshCurrentMinutes, 5);
          state.themeMode = normalizeThemeMode(msg.theme || "auto");
          applyTheme();
          state.firstRun = msg.firstRun === true || msg.firstRun === "true";
          post("get_languages", { code: state.currentLanguage });
          if (state.autoUpdate && !state.didAutoCheckUpdate) {
            state.didAutoCheckUpdate = true;
            requestUpdateCheck("auto");
          }
          if (state.firstRun) {
            switchTab("settings");
            showToast(t("settings.first_run_toast"), "warning");
          }
          state.configLoaded = true;
          refreshSettingsOptions();
          renderRefreshCountdowns();
        } else {
          if (state.hasPendingConfigWrite && !configMatchesPending(msg)) {
            return;
          }
          if (state.hasPendingConfigWrite) {
            clearPendingConfigState();
          }
          state.currentLanguage = msg.language || state.currentLanguage || "zh-CN";
          state.currentIdeExe = msg.ideExe || state.currentIdeExe || "Code.exe";
          state.autoUpdate = msg.autoUpdate !== false && msg.autoUpdate !== "false";
          state.autoRefreshAllMinutes = clampRefreshMinutes(msg.autoRefreshAllMinutes, state.autoRefreshAllMinutes || 15);
          state.autoRefreshCurrentMinutes = clampRefreshMinutes(msg.autoRefreshCurrentMinutes, state.autoRefreshCurrentMinutes || 5);
          state.autoRefreshCurrent = msg.autoRefreshCurrent !== false && msg.autoRefreshCurrent !== "false";
          state.lowQuotaAutoPrompt = msg.lowQuotaAutoPrompt !== false && msg.lowQuotaAutoPrompt !== "false";
          state.themeMode = normalizeThemeMode(msg.theme || state.themeMode || "auto");
          applyTheme();
          refreshSettingsOptions();
          renderRefreshCountdowns();
        }
        return;
      }

      if (msg && typeof msg === "object" && msg.type === "refresh_timers") {
        if (!state.hasPendingConfigWrite) {
          if (msg.currentEnabled === false || msg.currentEnabled === "false") state.autoRefreshCurrent = false;
          if (msg.currentEnabled === true || msg.currentEnabled === "true") state.autoRefreshCurrent = true;
          state.autoRefreshAllMinutes = clampRefreshMinutes(Number(msg.allIntervalSec) / 60, state.autoRefreshAllMinutes || 15);
          state.autoRefreshCurrentMinutes = clampRefreshMinutes(Number(msg.currentIntervalSec) / 60, state.autoRefreshCurrentMinutes || 5);
        }
        state.allRefreshRemainSec = Number(msg.allRemainingSec);
        state.currentRefreshRemainSec = Number(msg.currentRemainingSec);
        refreshSettingsOptions();
        renderRefreshCountdowns();
        return;
      }

      if (msg && typeof msg === "object" && msg.type === "low_quota_prompt") {
        if (!state.lowQuotaAutoPrompt) {
          post("cancel_low_quota_switch");
          return;
        }
        state.lowQuotaPromptOpen = true;
        openConfirm({
          title: t("low_quota.prompt_title"),
          message: t("low_quota.prompt_message", {
            current: msg.currentName || "-",
            currentQuota: msg.currentQuota ?? "-",
            best: msg.bestName || "-",
            bestQuota: msg.bestQuota ?? "-"
          }),
          persistent: true,
          onConfirm: () => post("confirm_low_quota_switch")
        });
        return;
      }

      if (msg && typeof msg === "object" && msg.type === "import_auth_need_name") {
        dom.importAuthNameInput.value = "";
        dom.importAuthModal.classList.add("show");
        setTimeout(() => dom.importAuthNameInput.focus(), 10);
        return;
      }

      if (msg && typeof msg === "object" && msg.type === "language_index") {
        const langs = Array.isArray(msg.languages) ? msg.languages : [];
        if (langs.length > 0) {
          state.languageIndex = langs;
          renderLanguageOptions();
          refreshSettingsOptions();
        }
        return;
      }

      if (msg && typeof msg === "object" && msg.type === "language_pack") {
        if (msg.ok && msg.strings && typeof msg.strings === "object") {
          applyLanguageStrings(msg.code || state.currentLanguage, msg.strings);
        } else {
          applyLanguageStrings("zh-CN", DEFAULT_I18N);
        }
        return;
      }

      if (msg && typeof msg === "object" && msg.type === "update_info") {
        if (!msg.ok) {
          dom.versionText.textContent = t("about.version_check_failed", { version: state.appVersion });
          if (state.updateCheckContext !== "auto") {
            showToast(msg.error || t("update.failed"), "error");
          }
          return;
        }
        const latest = msg.latest || "";
        if (msg.hasUpdate) {
          dom.versionText.textContent = t("about.version_new", { version: state.appVersion, latest });
          showToast(t("update.new", { latest }), "warning");
          promptUpdateDialog(msg);
        } else {
          dom.versionText.textContent = t("about.version_latest", { version: state.appVersion });
          if (state.updateCheckContext !== "auto") {
            showToast(t("update.latest"), "success");
          }
        }
        return;
      }

      const text = typeof msg === "string" ? msg : JSON.stringify(msg);
      log(`host: ${text}`);
    });
  }

  function bootstrap() {
    state.i18n = { ...DEFAULT_I18N };
    applyI18n();
    initLanguageIndexFallback();
    renderLanguageOptions();
    bindEvents();
    bindWebViewMessages();
    window.addEventListener("beforeunload", flushPendingConfigWrite);
    window.addEventListener("pagehide", flushPendingConfigWrite);
    document.addEventListener("visibilitychange", () => {
      if (document.visibilityState === "hidden") flushPendingConfigWrite();
    });
    window.addEventListener("resize", syncLayoutDensity);
    if (mediaDark) {
      const onSystemThemeChange = () => {
        if (state.themeMode === "auto") applyTheme();
      };
      if (typeof mediaDark.addEventListener === "function") {
        mediaDark.addEventListener("change", onSystemThemeChange);
      } else if (typeof mediaDark.addListener === "function") {
        mediaDark.addListener(onSystemThemeChange);
      }
    }
    syncLayoutDensity();
    applyTheme();
    applyLanguageStrings("zh-CN", DEFAULT_I18N);
    dom.logEl.style.display = state.debug ? "block" : "none";
    dom.debugPanel.style.display = state.debug ? "block" : "none";
    renderRefreshCountdowns();

    post("get_app_info");
    post("get_config");
    post("get_languages", { code: "zh-CN" });
    post("list_accounts");
  }

  bootstrap();
})();

