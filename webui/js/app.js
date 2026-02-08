(function () {
  "use strict";

  const IDE_LIST = ["Code.exe", "Trae.exe", "Kiro.exe", "Antigravity.exe"];
  const DEFAULT_I18N = {
    "app.brand":  "Codex Account Switch",
    "tab.accounts":  "Accounts",
    "tab.about":  "About",
    "tab.settings":  "Settings",
    "toolbar.refresh":  "Refresh",
    "toolbar.login_new":  "Login New",
    "toolbar.backup_current":  "Backup Current",
    "toolbar.import":  "Import",
    "toolbar.export":  "Export",
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
    "settings.language_label":  "Language",
    "settings.ide_label":  "IDE Executable",
    "settings.theme_label":  "Theme",
    "settings.theme_auto":  "Auto",
    "settings.theme_light":  "Light",
    "settings.theme_dark":  "Dark",
    "settings.theme_hint":  "Auto follows your Windows theme.",
    "settings.auto_update_label":  "Auto Update",
    "settings.auto_update_on":  "Enabled",
    "settings.auto_update_off":  "Disabled",
    "settings.auto_update_hint":  "Check update automatically at startup and prompt before downloading.",
    "settings.save":  "Save Settings",
    "settings.current_note":  "Current: {lang} / {ide}",
    "settings.first_run_toast":  "Please confirm default settings for first launch",
    "dialog.backup.title":  "Backup Current Account",
    "dialog.backup.name_label":  "Account Name",
    "dialog.backup.name_placeholder":  "Enter account name",
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
    "ide.Code.exe":  "VSCode",
    "ide.Trae.exe":  "Trae",
    "ide.Kiro.exe":  "Kiro",
    "ide.Antigravity.exe":  "Antigravity"
}
;

  const dom = {
    brandTitle: document.getElementById("brandTitle"),
    tabBtnAccounts: document.getElementById("tabBtnAccounts"),
    tabBtnAbout: document.getElementById("tabBtnAbout"),
    tabBtnSettings: document.getElementById("tabBtnSettings"),
    refreshBtn: document.getElementById("refreshBtn"),
    loginNewBtn: document.getElementById("loginNewBtn"),
    backupBtnTop: document.getElementById("backupBtnTop"),
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
    settingsLanguageLabel: document.getElementById("settingsLanguageLabel"),
    settingsIdeLabel: document.getElementById("settingsIdeLabel"),
    settingsThemeLabel: document.getElementById("settingsThemeLabel"),
    settingsThemeHint: document.getElementById("settingsThemeHint"),
    themeAutoBtn: document.getElementById("themeAutoBtn"),
    themeLightBtn: document.getElementById("themeLightBtn"),
    themeDarkBtn: document.getElementById("themeDarkBtn"),
    settingsAutoUpdateLabel: document.getElementById("settingsAutoUpdateLabel"),
    settingsAutoUpdateHint: document.getElementById("settingsAutoUpdateHint"),
    languageOptions: document.getElementById("languageOptions"),
    ideOptions: document.getElementById("ideOptions"),
    autoUpdateOnBtn: document.getElementById("autoUpdateOnBtn"),
    autoUpdateOffBtn: document.getElementById("autoUpdateOffBtn"),
    settingsSaveBtn: document.getElementById("settingsSaveBtn"),
    settingsNote: document.getElementById("settingsNote"),
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
    currentLanguage: "zh-CN",
    currentIdeExe: "Code.exe",
    autoUpdate: true,
    themeMode: "auto",
    firstRun: false,
    languageIndex: [],
    i18n: {},
    didAutoCheckUpdate: false,
    updateCheckContext: "manual",
    refreshMode: "",
    refreshTargetKey: "",
    refreshBusyTimer: null
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
    let text = state.i18n[key];
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

  function getLanguageDisplayName(code) {
    const meta = findLanguageMeta(code);
    return meta ? meta.name : code;
  }

  function switchTab(tab) {
    document.querySelectorAll(".tab-btn").forEach((x) => x.classList.toggle("active", x.getAttribute("data-tab") === tab));
    document.querySelectorAll(".tab-panel").forEach((x) => x.classList.toggle("active", x.id === `tab-${tab}`));
  }

  function openConfirm(options) {
    dom.confirmTitle.textContent = options?.title || t("dialog.confirm.title");
    dom.confirmMessage.textContent = options?.message || t("dialog.confirm.default_message");
    state.confirmAction = typeof options?.onConfirm === "function" ? options.onConfirm : null;
    dom.confirmModal.classList.add("show");
  }

  function closeConfirm() {
    dom.confirmModal.classList.remove("show");
    state.confirmAction = null;
  }

  function updateSettingsNote() {
    dom.settingsNote.textContent = t("settings.current_note", {
      lang: getLanguageDisplayName(state.currentLanguage),
      ide: getIdeDisplayName(state.currentIdeExe)
    });
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
    document.querySelectorAll("[data-auto-update-option]").forEach((x) => {
      const val = x.getAttribute("data-auto-update-option") === "true";
      x.classList.toggle("active", val === state.autoUpdate);
    });
    document.querySelectorAll("[data-theme-option]").forEach((x) => {
      x.classList.toggle("active", x.getAttribute("data-theme-option") === state.themeMode);
    });
    updateSettingsNote();
  }

  function applyI18n() {
    dom.tabBtnAccounts.textContent = t("tab.accounts");
    dom.tabBtnAbout.textContent = t("tab.about");
    dom.tabBtnSettings.textContent = t("tab.settings");
    dom.refreshBtn.textContent = t("toolbar.refresh");
    dom.loginNewBtn.textContent = t("toolbar.login_new");
    dom.backupBtnTop.textContent = t("toolbar.backup_current");
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
    dom.settingsLanguageLabel.textContent = t("settings.language_label");
    dom.settingsIdeLabel.textContent = t("settings.ide_label");
    dom.settingsThemeLabel.textContent = t("settings.theme_label");
    dom.settingsThemeHint.textContent = t("settings.theme_hint");
    dom.themeAutoBtn.textContent = t("settings.theme_auto");
    dom.themeLightBtn.textContent = t("settings.theme_light");
    dom.themeDarkBtn.textContent = t("settings.theme_dark");
    dom.settingsAutoUpdateLabel.textContent = t("settings.auto_update_label");
    dom.settingsAutoUpdateHint.textContent = t("settings.auto_update_hint");
    dom.autoUpdateOnBtn.textContent = t("settings.auto_update_on");
    dom.autoUpdateOffBtn.textContent = t("settings.auto_update_off");
    dom.settingsSaveBtn.textContent = t("settings.save");
    dom.backupTitle.textContent = t("dialog.backup.title");
    dom.backupNameLabel.textContent = t("dialog.backup.name_label");
    dom.backupNameInput.placeholder = t("dialog.backup.name_placeholder");
    dom.backupCancelBtn.textContent = t("dialog.common.cancel");
    dom.backupConfirmBtn.textContent = t("dialog.common.save");
    dom.confirmCancelBtn.textContent = t("dialog.common.cancel");
    dom.confirmOkBtn.textContent = t("dialog.common.confirm");
    dom.confirmTitle.textContent = t("dialog.confirm.title");
    dom.confirmMessage.textContent = t("dialog.confirm.default_message");
    dom.brandTitle.textContent = t("app.brand");
    dom.versionText.textContent = t("about.version_prefix", { version: state.appVersion });
    renderIdeOptions();
    updateSettingsNote();
    applyCountText();
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

  function formatHoursFromSeconds(v) {
    if (!Number.isFinite(Number(v)) || Number(v) < 0) return "-";
    return (Number(v) / 3600).toFixed(1);
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

    dom.searchInput.addEventListener("input", applySearch);

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

    dom.backupBtnTop.addEventListener("click", () => {
      dom.backupNameInput.value = "";
      dom.backupModal.classList.add("show");
      setTimeout(() => dom.backupNameInput.focus(), 10);
    });

    document.querySelectorAll("[data-auto-update-option]").forEach((btn) => {
      btn.addEventListener("click", () => {
        state.autoUpdate = btn.getAttribute("data-auto-update-option") === "true";
        refreshSettingsOptions();
      });
    });

    document.querySelectorAll("[data-theme-option]").forEach((btn) => {
      btn.addEventListener("click", () => {
        state.themeMode = normalizeThemeMode(btn.getAttribute("data-theme-option"));
        refreshSettingsOptions();
        applyTheme();
      });
    });

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

    dom.confirmCancelBtn.addEventListener("click", closeConfirm);
    dom.confirmModal.addEventListener("click", (e) => {
      if (e.target === dom.confirmModal) closeConfirm();
    });
    dom.confirmOkBtn.addEventListener("click", () => {
      const fn = state.confirmAction;
      closeConfirm();
      if (fn) fn();
    });

    dom.settingsSaveBtn.addEventListener("click", () => {
      const targetLang = state.currentLanguage;
      post("set_config", {
        language: targetLang,
        ideExe: state.currentIdeExe,
        autoUpdate: state.autoUpdate,
        theme: state.themeMode
      });
      requestLanguagePack(targetLang);
      updateSettingsNote();
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
    dom.loginNewBtn.addEventListener("click", () => {
      openConfirm({
        title: t("dialog.login_new.title"),
        message: t("dialog.login_new.message", { ide: getIdeDisplayName(state.currentIdeExe) }),
        onConfirm: () => post("login_new_account")
      });
    });
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
        dom.versionText.textContent = t("about.version_prefix", { version: state.appVersion });
        return;
      }

      if (msg && typeof msg === "object" && msg.type === "config") {
        state.currentLanguage = msg.language || "zh-CN";
        if (typeof msg.languageIndex === "number" && state.languageIndex[msg.languageIndex]) {
          state.currentLanguage = state.languageIndex[msg.languageIndex].code;
        }
        state.currentIdeExe = msg.ideExe || "Code.exe";
        state.autoUpdate = msg.autoUpdate !== false && msg.autoUpdate !== "false";
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

    post("get_app_info");
    post("get_config");
    post("get_languages", { code: "zh-CN" });
    post("list_accounts");
  }

  bootstrap();
})();

