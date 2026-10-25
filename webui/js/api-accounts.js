(function () {
  "use strict";
  const words = {
    title: ["第三方 API 账号", "Third-party API accounts"], add: ["新增 API", "Add API"], refresh: ["刷新", "Refresh"],
    importCCS: ["导入 CC Switch 账号", "Import CC Switch accounts"],
    importPath: ["CC Switch 数据路径（留空使用默认路径）", "CC Switch data path (empty uses default)"],
    official: ["官方账号代理池", "Official account proxy pool"], pool: ["加入代理池", "Proxy pool"], empty: ["暂无账号", "No accounts"],
    name: ["名称", "Name"], baseUrl: ["API Base URL", "API Base URL"], apiKey: ["API Key", "API Key"], model: ["模型", "Model"],
    edit: ["编辑", "Edit"], remove: ["删除", "Delete"],
    check: ["连通性 Ping", "Connectivity Ping"], balance: ["查询余额", "Query balance"], switch: ["切换使用", "Switch"],
    save: ["保存", "Save"], cancel: ["取消", "Cancel"], balanceTitle: ["配置式余额查询", "Balance query configuration"],
    url: ["余额查询 URL", "Balance query URL"], method: ["HTTP 方法", "HTTP method"], headers: ["请求头（JSON 对象）", "Headers (JSON object)"],
    body: ["请求体", "Request body"], path: ["JSON 余额路径（如 data.balance）", "JSON balance path (e.g. data.balance)"], unit: ["单位", "Unit"],
    advanced: ["高级：CC Switch JavaScript 模板", "Advanced: CC Switch JavaScript template"], enabled: ["使用高级脚本替代配置式查询", "Use advanced script instead of configured query"],
    scriptHint: ["脚本需返回 { request: { url, method, headers, body }, extractor: (response) => ... }。支持 {{apiKey}}、{{baseUrl}}、{{accessToken}}、{{userId}} 替换；HTTP 由宿主执行。", "Return { request: { url, method, headers, body }, extractor: (response) => ... }. Supports {{apiKey}}, {{baseUrl}}, {{accessToken}}, {{userId}} substitution. HTTP runs in the host."],
    code: ["JavaScript 模板", "JavaScript template"], timeout: ["脚本超时（秒）", "Script timeout (seconds)"],
    scriptKey: ["脚本 API Key（留空使用账号值）", "Script API key (empty uses account value)"], scriptUrl: ["脚本 Base URL（留空使用账号值）", "Script base URL (empty uses account value)"],
    accessToken: ["Access Token", "Access Token"], userId: ["User ID", "User ID"], checking: ["正在进行连通性 Ping…", "Pinging endpoint…"], querying: ["正在查询…", "Querying…"], ok: ["Ping 连通（不代表密钥或模型可用）", "Ping reachable (does not validate API key or model)"],
    required: ["请填写名称、Base URL、API Key 和模型。", "Enter name, base URL, API key and model."], invalidHeaders: ["请求头必须是 JSON 对象。", "Headers must be a JSON object."],
    requiredCode: ["请填写高级脚本。", "Enter an advanced script."], timeoutError: ["脚本执行超时", "Script execution timed out"]
  };
  Object.assign(words, {
    balanceTitle: ["余额设置", "Balance settings"], configured: ["配置式查询", "Configured query"], scripting: ["JavaScript 脚本", "JavaScript script"],
    editScript: ["编辑脚本与凭据", "Edit script and credentials"], scriptTitle: ["余额查询脚本", "Balance query script"], back: ["返回余额设置", "Back to balance settings"],
    show: ["显示", "Show"], hide: ["隐藏", "Hide"], remaining: ["余额", "Remaining"], used: ["已用", "Used"], total: ["总额", "Total"],
    invalid: ["账号无效", "Invalid account"], valid: ["账号有效", "Valid account"], noBalance: ["尚未查询余额", "Balance not queried"], noAmount: ["未返回余额数值", "No balance amount returned"],
    importTitle: ["导入 CC Switch 账号", "Import CC Switch accounts"], scriptHint: ["返回 { request: { url, method, headers, body }, extractor: (response) => ({ isValid, remaining, used, total, unit, planName, invalidMessage }) }，支持返回套餐数组。占位符：{{apiKey}}、{{baseUrl}}、{{accessToken}}、{{userId}}。", "Return { request: { url, method, headers, body }, extractor: (response) => ({ isValid, remaining, used, total, unit, planName, invalidMessage }) }. Plan arrays are supported. Variables: {{apiKey}}, {{baseUrl}}, {{accessToken}}, {{userId}}."],
    configuredHint: ["请求参数支持 {{apiKey}} 与 {{baseUrl}}；路径用于从响应 JSON 提取余额。", "Request values support {{apiKey}} and {{baseUrl}}. The path extracts the balance from response JSON."]
  });
  Object.assign(words, {
    addAccount: ["新增账号", "Add account"], editAccount: ["编辑账号", "Edit account"], chooseProvider: ["选择供应商", "Choose a provider"],
    pickHint: ["选择一个供应商预设，或从自定义配置开始。", "Choose a provider preset or start with a custom configuration."],
    custom: ["自定义配置", "Custom configuration"], customHint: ["连接兼容 OpenAI Responses 的 API", "Connect an OpenAI Responses-compatible API"],
    searchProviders: ["搜索供应商…", "Search providers…"], sort: ["排序", "Sort"], recommended: ["默认顺序", "Default order"], alphabetical: ["名称 A–Z", "Name A–Z"],
    next: ["下一步", "Next"], backPage: ["返回", "Back"], basics: ["基本信息", "Basic information"],
    apiConfig: ["API 配置", "API configuration"], configFiles: ["配置文件", "Configuration files"], website: ["官网地址", "Website URL"], notes: ["备注", "Notes"],
    configHint: ["编辑器与 API 配置同步；写入本地时仅更新 API Key、模型、供应商、地址和协议，其他编辑器内容不会覆盖本地配置。", "The editor stays synchronized with the API configuration. Only the API key, model, provider, URL and protocol are written locally; other editor content does not overwrite local settings."],
    responsesOnly: ["当前仅支持 OpenAI Responses 协议。", "Only the OpenAI Responses protocol is supported."],
    unsupported: ["此预设不兼容当前 Responses 接口。请返回选择其他预设，或使用自定义配置填写兼容的地址。", "This preset is incompatible with the Responses interface. Choose another preset or use a custom configuration with a compatible endpoint."],
    configRequired: ["config.toml 必须包含 model_provider、model，以及活动供应商表的 base_url；wire_api 如有填写须为 responses。", "config.toml must include model_provider, model, and base_url in the active provider table; wire_api, if specified, must be responses."],
    invalidAuth: ["auth.json 必须是 JSON 对象。", "auth.json must be a JSON object."],
    officialHint: ["此预设使用 ChatGPT 官方 OAuth 登录，请继续使用官方账号添加流程。", "This preset uses official ChatGPT OAuth. Continue with the official account flow."],
    oauthHint: ["此供应商需要 OAuth 授权，当前版本不支持将它作为静态 API Key 账号添加。", "This provider requires OAuth and cannot be added as a static API key account in this version."],
    officialAction: ["添加官方账号", "Add official account"], noProviders: ["没有匹配的供应商", "No matching providers"],
    usageEnabled: ["启用余额查询", "Enable balance query"], usageHint: ["选择查询模板并填写所需凭据；测试只运行当前草稿，不保存账号。", "Choose a template and enter its credentials. Testing runs the draft without saving the account."],
    template: ["查询模板", "Query template"], general: ["通用余额查询", "General balance"], newapi: ["New API", "New API"], sub2api: ["Sub2API", "Sub2API"],
    customScript: ["自定义 JavaScript", "Custom JavaScript"], legacy: ["旧配置式查询", "Existing configured query"], credentials: ["查询凭据", "Query credentials"],
    schedule: ["查询设置", "Query settings"], scriptCode: ["脚本内容", "Script source"], testScript: ["测试脚本", "Test script"], testResult: ["测试结果", "Test result"],
    testFirst: ["测试当前草稿后，结果会显示在这里。", "Run the draft to see the result here."], scriptDisabled: ["请先启用余额查询。", "Enable balance query first."],
    keyHint: ["留空使用当前账号的 API Key 与 Base URL。", "Leave blank to use the account API key and base URL."],
    newapiHint: ["使用 New API 的访问令牌与用户 ID，而非推理 API Key。", "Use the New API access token and user ID, not the inference API key."],
    fetchModels: ["↓ 获取模型", "↓ Fetch models"], chooseModel: ["选择可用模型（也可手动填写）", "Choose an available model (or enter one manually)"],
    modelsLoading: ["正在获取模型…", "Fetching models…"], modelsEmpty: ["接口未返回可用模型，可继续手动填写。", "No models returned. You can still enter a model manually."],
    modelsReady: ["已获取可用模型，请选择或继续手动填写。", "Models fetched. Choose one or continue entering a model manually."],
    configLoading: ["正在读取本机完整配置文件…", "Loading complete local configuration files…"],
    configLoaded: ["已读取本机完整配置，并叠加当前 API 草稿字段。", "Complete local configuration loaded with the current API draft fields applied."],
    configChangedDraft: ["本地配置已变化；当前未保存的编辑已保留，重新打开编辑页可载入最新配置。", "Local configuration changed. Unsaved edits are preserved; reopen the editor to load the latest configuration."],
    configUnavailable: ["本机配置读取失败，无法保存；请关闭后重新打开编辑器。", "Unable to load local configuration. Saving is unavailable; close and reopen the editor."],
    modelsNeedConfig: ["请先填写 Base URL 和 API Key。", "Enter the base URL and API key first."],
    configSynced: ["配置已同步", "Configuration synchronized"], authFile: ["auth.json", "auth.json"], configFile: ["config.toml", "config.toml"]
  });
  const state = { items: [], results: new Map(), checks: new Map(), timeoutSec: null, language: document.documentElement.lang, balancePending: new Set(), balanceSignatures: new Map() };
  const isEnglish = () => /^en/i.test(state.language);
  const t = key => words[key][isEnglish() ? 1 : 0];
  const localized = [];
  function el(tag, className, text) {
    const node = document.createElement(tag);
    if (className) node.className = className;
    if (text !== undefined) node.textContent = text;
    return node;
  }
  function label(node, key, property = "textContent") { localized.push({ node, key, property }); node[property] = t(key); return node; }
  function button(key, onClick, primary = false) {
    const node = label(el("button", "api-page-button" + (primary ? " primary" : "")), key);
    node.type = "button"; node.addEventListener("click", onClick); return node;
  }
  function post(action, payload = {}) { window.chrome.webview.postMessage({ action, ...payload }); }
  function changed() { window.dispatchEvent(new Event("api-accounts-updated")); }
  const getItem = id => state.items.find(item => item.id === id);
  const host = document.querySelector("main.content");
  const panel = el("section", "api-provider-page"); panel.hidden = true;
  panel.setAttribute("aria-labelledby", "apiProviderPageTitle");
  const header = el("header", "api-page-header");
  const backButton = button("backPage", goBack); backButton.classList.add("api-page-back");
  const heading = el("h1"); heading.id = "apiProviderPageTitle";
  header.append(backButton, heading, el("span", "api-page-app", "Codex"));
  const scroller = el("div", "api-page-scroll");
  const content = el("div", "api-page-content"); scroller.append(content);
  const error = el("div", "api-page-error"); error.setAttribute("role", "alert");
  const footer = el("footer", "api-page-footer");
  const footerHint = el("span", "api-page-footer-hint"); const footerActions = el("div", "api-page-actions");
  const cancel = button("cancel", closePage); const next = button("next", nextStep, true); const save = button("save", () => guarded(savePage), true);
  footerActions.append(cancel, next, save); footer.append(footerHint, footerActions);
  panel.append(header, scroller, error, footer); host.append(panel);
  const pages = {};
  ["pick", "form", "balance", "import"].forEach(key => { pages[key] = el("div", "api-page-step"); content.append(pages[key]); });
  let editingId = "", currentPage = "", selectedPreset = null, accountDraft = {}, scriptDraft = {}, balanceDraft = {}, previewId = "";
  let previousOverflow = "", previousScroll = 0, returnFocus;
  let editorConfigRequestId = "", editorConfigReady = false;
  let saveRequestId = "", previousAuthKey;
  let loadedEditorAuth = "", loadedEditorConfig = "";
  const fields = {}, fieldWraps = {};
  function guarded(action) { error.textContent = ""; try { action(); } catch (failure) { error.textContent = failure.message; } }
  function showPage(page) {
    saveRequestId = "";
    if (page !== "form") editorConfigRequestId = "";
    currentPage = page; error.textContent = "";
    Object.entries(pages).forEach(([key, node]) => { node.hidden = key !== page; });
    heading.textContent = t(page === "pick" ? "addAccount" : page === "form" ? editingId ? "editAccount" : "addAccount" : page === "balance" ? "balanceTitle" : "importTitle");
    next.hidden = page !== "pick"; save.hidden = page === "pick" || page === "import";
    footerHint.textContent = page === "pick" || page === "form" ? selectedPreset?.name || t("custom") : page === "balance" ? accountDraft.name : "";
    if (panel.hidden) {
      returnFocus = document.activeElement; previousOverflow = host.style.overflow; previousScroll = host.scrollTop;
      host.scrollTop = 0; host.style.overflow = "hidden"; panel.hidden = false;
      document.body.classList.add("api-account-page-open");
    }
    scroller.scrollTop = 0; heading.tabIndex = -1; heading.focus();
  }
  function closePage() { saveRequestId = ""; editorConfigRequestId = ""; editorConfigReady = false; resetModels(); panel.hidden = true; document.body.classList.remove("api-account-page-open"); host.style.overflow = previousOverflow; host.scrollTop = previousScroll; returnFocus?.focus(); }
  window.addEventListener("account-workspace-leave", () => { if (!panel.hidden) closePage(); });
  function goBack() { if (currentPage === "form" && !editingId) { showPage("pick"); return; } closePage(); }
  panel.addEventListener("keydown", event => { if (event.key === "Escape") { event.stopPropagation(); goBack(); } });
  function section(parent, key, hint) {
    const node = el("section", "api-page-section"); node.append(label(el("h2"), key));
    if (hint) node.append(label(el("p", "api-page-hint"), hint)); parent.append(node); return node;
  }
  function grid(parent) { const node = el("div", "api-page-grid"); parent.append(node); return node; }
  function field(parent, name, key, type = "text", wide = false) {
    const wrap = el("label", "api-page-field" + (wide ? " wide" : "")); wrap.append(label(el("span"), key));
    const input = el(type === "textarea" ? "textarea" : type === "select" ? "select" : "input");
    if (type !== "textarea" && type !== "select") input.type = type;
    input.name = name; input.autocomplete = "off";
    if (type === "password") {
      const row = el("div", "api-page-secret");
      const toggle = button("show", () => { input.type = input.type === "password" ? "text" : "password"; const item = localized.find(entry => entry.node === toggle); item.key = input.type === "password" ? "show" : "hide"; toggle.textContent = t(item.key); });
      row.append(input, toggle); wrap.append(row);
    } else wrap.append(input);
    fields[name] = input; fieldWraps[name] = wrap; parent.append(wrap); return input;
  }
  function option(select, value, key) { const node = label(el("option"), key); node.value = value; select.append(node); }
  function resetSecrets() {
    panel.querySelectorAll(".api-page-secret input").forEach(input => { input.type = "password"; const toggle = input.nextElementSibling; const entry = localized.find(item => item.node === toggle); entry.key = "show"; toggle.textContent = t("show"); });
  }
  const pickIntro = section(pages.pick, "chooseProvider", "pickHint");
  const pickControls = el("div", "api-preset-controls");
  const search = label(el("input", "api-preset-search"), "searchProviders", "placeholder"); search.type = "search"; search.setAttribute("aria-label", t("searchProviders"));
  const sort = el("select", "api-preset-sort"); sort.setAttribute("aria-label", t("sort")); option(sort, "default", "recommended"); option(sort, "name", "alphabetical");
  pickControls.append(search, sort); pickIntro.append(pickControls);
  const presetGrid = el("div", "api-preset-grid"); pickIntro.append(presetGrid);
  function renderPresets() {
    presetGrid.replaceChildren();
    const presets = [{ id: "custom", name: t("custom"), description: t("customHint"), custom: true }, ...window.CCS_CODEX_PRESETS.filter(preset => !preset.hidden)];
    const query = search.value.trim().toLowerCase();
    const visible = presets.filter(preset => `${preset.name} ${preset.baseUrl || ""} ${preset.category || ""}`.toLowerCase().includes(query));
    if (sort.value === "name") visible.sort((a, b) => a.name.localeCompare(b.name, isEnglish() ? "en" : "zh"));
    visible.forEach(preset => {
      const card = el("button", "api-preset-card"); card.type = "button";
      const selected = preset.id === (selectedPreset?.id || "custom"); card.classList.toggle("selected", selected); card.setAttribute("aria-pressed", String(selected));
      const glyph = el("span", "api-preset-glyph", preset.custom ? "+" : preset.name.slice(0, 1).toUpperCase());
      const icon = window.CCS_PROVIDER_ICONS[preset.icon];
      if (icon) {
        glyph.replaceChildren();
        if (icon.startsWith("data:")) { const image = el("img"); image.src = icon; image.alt = ""; glyph.append(image); }
        else glyph.innerHTML = icon;
        if (preset.color) glyph.style.color = preset.color;
      }
      const info = el("span", "api-preset-info"); info.append(el("strong", "", preset.name));
      const detail = preset.custom ? preset.description : preset.isOfficial || preset.category === "official" ? "ChatGPT · OAuth" : preset.requiresOAuth ? "OAuth" : preset.baseUrl || preset.websiteUrl || "API";
      info.append(el("small", "", detail)); card.append(glyph, info, el("span", "api-preset-selection", selected ? "✓" : ""));
      card.addEventListener("click", () => { selectedPreset = preset.custom ? null : preset; renderPresets(); footerHint.textContent = preset.name; }); presetGrid.append(card);
    });
    if (!visible.length) presetGrid.append(label(el("p", "api-page-hint"), "noProviders"));
  }
  search.addEventListener("input", renderPresets); sort.addEventListener("change", renderPresets);
  const oauthNotice = el("div", "api-page-notice"); pages.form.append(oauthNotice);
  const officialButton = button("officialAction", () => { closePage(); window.dispatchEvent(new Event("request-official-account")); }, true); pages.form.append(officialButton);
  const providerForm = el("div"); pages.form.append(providerForm);
  const basic = section(providerForm, "basics"); const basicGrid = grid(basic);
  field(basicGrid, "name", "name"); field(basicGrid, "websiteUrl", "website"); field(basicGrid, "notes", "notes", "textarea", true);
  const formTabs = el("div", "api-page-tabs"); providerForm.append(formTabs);
  const apiTab = button("apiConfig", () => setFormTab("api")); const configTab = button("configFiles", () => setFormTab("files")); formTabs.append(apiTab, configTab);
  const apiSection = section(providerForm, "apiConfig", "responsesOnly"); const apiGrid = grid(apiSection);
  field(apiGrid, "baseUrl", "baseUrl", "text", true); field(apiGrid, "apiKey", "apiKey", "password", true); field(apiGrid, "model", "model", "text", true);
  const modelRow = el("div", "api-page-actions api-model-row");
  const fetchModelsButton = button("fetchModels", () => guarded(fetchModels));
  modelRow.append(fields.model, fetchModelsButton); fieldWraps.model.append(modelRow);
  const modelPicker = el("select"); modelPicker.hidden = true;
  label(modelPicker, "chooseModel", "ariaLabel");
  const modelPlaceholder = label(el("option"), "chooseModel"); modelPlaceholder.value = ""; modelPicker.append(modelPlaceholder);
  const modelsStatus = el("p", "api-page-hint"); modelsStatus.setAttribute("role", "status");
  fieldWraps.model.append(modelPicker, modelsStatus);
  let modelsRequestId = "", modelsPhase = "", modelsBaseUrl = "", modelsApiKey = "";
  function renderModelsStatus() { modelsStatus.textContent = modelsPhase ? t(modelsPhase) : ""; }
  function resetModels() {
    modelsRequestId = ""; modelsPhase = ""; fetchModelsButton.disabled = false;
    modelPicker.replaceChildren(modelPlaceholder); modelPicker.hidden = true; renderModelsStatus();
  }
  function fetchModels() {
    const baseUrl = fields.baseUrl.value.trim(), apiKey = fields.apiKey.value.trim();
    if (!baseUrl || !apiKey) throw new Error(t("modelsNeedConfig"));
    resetModels(); modelsRequestId = crypto.randomUUID(); modelsBaseUrl = baseUrl; modelsApiKey = apiKey;
    modelsPhase = "modelsLoading"; renderModelsStatus(); fetchModelsButton.disabled = true;
    post("fetch_api_models", { requestId: modelsRequestId, id: editingId, baseUrl, apiKey });
  }
  modelPicker.addEventListener("change", () => {
    if (!modelPicker.value) return;
    fields.model.value = modelPicker.value; fields.model.dispatchEvent(new Event("input", { bubbles: true }));
  });
  ["baseUrl", "apiKey"].forEach(name => fields[name].addEventListener("input", resetModels));
  const protocolNotice = el("div", "api-page-notice"); apiSection.append(protocolNotice);
  const fileSection = section(providerForm, "configFiles", "configHint");
  const fileTabs = el("div", "api-file-tabs"); fileSection.append(fileTabs);
  const authTab = el("button", "api-page-button", "auth.json"); authTab.type = "button";
  const tomlTab = el("button", "api-page-button", "config.toml"); tomlTab.type = "button";
  authTab.addEventListener("click", () => setFileTab("auth")); tomlTab.addEventListener("click", () => setFileTab("config")); fileTabs.append(authTab, tomlTab);
  const fileGrid = grid(fileSection);
  field(fileGrid, "auth", "authFile", "textarea", true);
  field(fileGrid, "config", "configFile", "textarea", true);
  fields.auth.className = fields.config.className = "api-config-source";
  const syncNotice = el("p", "api-page-hint"); syncNotice.setAttribute("role", "status"); providerForm.append(syncNotice);
  function setFormTab(tab) { apiSection.hidden = tab !== "api"; fileSection.hidden = tab !== "files"; apiTab.classList.toggle("active", tab === "api"); configTab.classList.toggle("active", tab === "files"); }
  function setFileTab(tab) { fieldWraps.auth.hidden = tab !== "auth"; fieldWraps.config.hidden = tab !== "config"; authTab.classList.toggle("active", tab === "auth"); tomlTab.classList.toggle("active", tab === "config"); }
  function atom(source) {
    const value = source.trim();
    if (value.startsWith('"')) return JSON.parse(value.match(/^"(?:[^"\\]|\\.)*"/)[0]);
    if (value.startsWith("'")) return value.slice(1, value.indexOf("'", 1));
    return value.split("#")[0].trim();
  }
  function configEntries(text) {
    let table = ""; const entries = [];
    text.split(/\r?\n/).forEach((line, index) => {
      const tableMatch = line.match(/^\s*\[([^\]]+)\]\s*(?:#.*)?$/);
      if (tableMatch) { table = tableMatch[1].replace(/["']/g, ""); return; }
      const valueMatch = line.match(/^\s*([A-Za-z_][\w-]*)\s*=\s*(.*)$/);
      if (valueMatch) entries.push({ table, key: valueMatch[1], raw: valueMatch[2], index });
    });
    return entries;
  }
  function readConfig(text) {
    const entries = configEntries(text);
    const read = (table, key) => { const entry = entries.find(item => item.table === table && item.key === key); return entry ? atom(entry.raw) : ""; };
    const provider = read("", "model_provider");
    return { provider, model: read("", "model"), baseUrl: read(`model_providers.${provider}`, "base_url"), wireApi: read(`model_providers.${provider}`, "wire_api"), bearerToken: read(`model_providers.${provider}`, "experimental_bearer_token") };
  }
  function writeConfig(text, table, key, value) {
    const lines = text.split(/\r?\n/); const entry = configEntries(text).find(item => item.table === table && item.key === key);
    if (entry) { lines[entry.index] = `${key} = ${JSON.stringify(value)}`; return lines.join("\n"); }
    if (!table) { const index = lines.findIndex(line => /^\s*\[/.test(line)); lines.splice(index < 0 ? lines.length : index, 0, `${key} = ${JSON.stringify(value)}`); return lines.join("\n"); }
    const index = lines.findIndex(line => line.trim().replace(/["']/g, "") === `[${table}]`);
    if (index < 0) throw new Error(t("configRequired"));
    lines.splice(index + 1, 0, `${key} = ${JSON.stringify(value)}`); return lines.join("\n");
  }
  function readAuth() { const auth = JSON.parse(fields.auth.value); if (!auth || typeof auth !== "object" || Array.isArray(auth)) throw new Error(t("invalidAuth")); return auth; }
  function showProtocol(config) {
    const apiFormat = accountDraft.metadata?.apiFormat || selectedPreset?.apiFormat;
    const compatible = (!config.wireApi || config.wireApi === "responses") && (!apiFormat || apiFormat === "openai_responses");
    protocolNotice.textContent = compatible ? "" : `${t("unsupported")} (${apiFormat || config.wireApi || "—"})`;
    protocolNotice.hidden = compatible; save.disabled = !!saveRequestId || !editorConfigReady || !compatible;
  }
  function syncFromFiles() {
    resetModels();
    const auth = readAuth(); const config = readConfig(fields.config.value);
    previousAuthKey = auth.OPENAI_API_KEY;
    fields.apiKey.value = config.bearerToken || (typeof auth.OPENAI_API_KEY === "string" ? auth.OPENAI_API_KEY : "");
    fields.baseUrl.value = config.baseUrl; fields.model.value = config.model;
    showProtocol(config); syncNotice.textContent = t("configSynced"); syncNotice.classList.remove("error");
  }
  function syncFromApi() {
    const auth = readAuth(); auth.OPENAI_API_KEY = fields.apiKey.value; fields.auth.value = JSON.stringify(auth, null, 2);
    previousAuthKey = auth.OPENAI_API_KEY;
    const config = readConfig(fields.config.value);
    if (!config.provider) throw new Error(t("configRequired"));
    let text = writeConfig(fields.config.value, "", "model", fields.model.value);
    if (configEntries(text).some(entry => entry.table === `model_providers.${config.provider}` && entry.key === "experimental_bearer_token"))
      text = writeConfig(text, `model_providers.${config.provider}`, "experimental_bearer_token", fields.apiKey.value);
    text = writeConfig(text, `model_providers.${config.provider}`, "base_url", fields.baseUrl.value); fields.config.value = text;
    showProtocol(readConfig(text));
  }
  ["auth", "config"].forEach(name => fields[name].addEventListener("input", () => {
    try {
      if (name === "auth") {
        const auth = readAuth();
        if (typeof auth.OPENAI_API_KEY === "string" && auth.OPENAI_API_KEY !== previousAuthKey) {
          const config = readConfig(fields.config.value);
          const table = `model_providers.${config.provider}`;
          if (configEntries(fields.config.value).some(entry => entry.table === table && entry.key === "experimental_bearer_token"))
            fields.config.value = writeConfig(fields.config.value, table, "experimental_bearer_token", auth.OPENAI_API_KEY);
        }
      }
      syncFromFiles();
    } catch (failure) { syncNotice.textContent = failure.message; syncNotice.classList.add("error"); save.disabled = true; }
  }));
  ["apiKey", "baseUrl", "model"].forEach(name => fields[name].addEventListener("input", () => guarded(syncFromApi)));
  function setEditorConfigEnabled(enabled) {
    ["apiKey", "baseUrl", "model", "auth", "config"].forEach(name => { fields[name].disabled = !enabled; });
    fetchModelsButton.disabled = !enabled;
  }
  function loadForm(item, preset) {
    resetModels(); editorConfigRequestId = ""; editorConfigReady = false;
    accountDraft = { ...item }; const metadata = item.metadata || {};
    const settings = metadata.settingsConfig || preset?.settingsConfig || window.CCS_CODEX_CUSTOM_TEMPLATE;
    const draftConfig = readConfig(settings.config || "");
    fields.name.value = item.name || preset?.name || ""; fields.websiteUrl.value = metadata.websiteUrl || preset?.websiteUrl || ""; fields.notes.value = metadata.notes || "";
    fields.apiKey.value = item.id ? item.apiKey : settings.auth?.OPENAI_API_KEY || "";
    fields.baseUrl.value = item.id ? item.baseUrl : draftConfig.baseUrl;
    fields.model.value = item.id ? item.model : draftConfig.model;
    fields.auth.value = ""; fields.config.value = "";
    const official = preset?.isOfficial || preset?.category === "official";
    const oauth = official || preset?.requiresOAuth;
    oauthNotice.hidden = !oauth; officialButton.hidden = !official; providerForm.hidden = !!oauth;
    oauthNotice.textContent = t(official ? "officialHint" : "oauthHint"); save.disabled = true;
    protocolNotice.hidden = true;
    resetSecrets(); setFormTab("api"); setFileTab("auth"); setEditorConfigEnabled(false);
    syncNotice.classList.remove("error"); syncNotice.textContent = oauth ? "" : t("configLoading");
    if (!oauth) {
      editorConfigRequestId = crypto.randomUUID();
      post("get_api_editor_config", { requestId: editorConfigRequestId, id: editingId, apiKey: fields.apiKey.value, baseUrl: fields.baseUrl.value, model: fields.model.value });
    }
  }
  function openEditor(id) {
    editorConfigRequestId = ""; editorConfigReady = false;
    editingId = id || ""; previewId = "";
    if (id) { const item = getItem(id); selectedPreset = window.CCS_CODEX_PRESETS.find(preset => preset.id === item.metadata?.presetId) || null; loadForm(item, selectedPreset); showPage("form"); }
    else { selectedPreset = null; search.value = ""; sort.value = "default"; renderPresets(); showPage("pick"); }
  }
  function nextStep() { guarded(() => { loadForm({}, selectedPreset); showPage("form"); }); }
  function saveAccount() {
    if (!editorConfigReady) throw new Error(t("configUnavailable"));
    const auth = readAuth(); const config = readConfig(fields.config.value);
    if (typeof auth.OPENAI_API_KEY === "string") auth.OPENAI_API_KEY = auth.OPENAI_API_KEY.trim();
    if (!config.provider || !config.model || !config.baseUrl) throw new Error(t("configRequired"));
    const apiFormat = accountDraft.metadata?.apiFormat || selectedPreset?.apiFormat;
    if ((config.wireApi && config.wireApi !== "responses") || (apiFormat && apiFormat !== "openai_responses")) throw new Error(t("unsupported"));
    const account = { ...accountDraft, ...getItem(editingId), id: editingId, name: fields.name.value.trim(), apiKey: String(config.bearerToken || auth.OPENAI_API_KEY || "").trim(), model: config.model, baseUrl: config.baseUrl };
    if (["name", "apiKey", "model", "baseUrl"].some(key => !account[key])) throw new Error(t("required"));
    account.metadata = { ...account.metadata, presetId: selectedPreset?.id || account.metadata?.presetId || "custom", websiteUrl: fields.websiteUrl.value.trim(), notes: fields.notes.value, settingsConfig: { ...account.metadata?.settingsConfig, auth, config: fields.config.value }, apiFormat: "openai_responses" };
    if (!editingId && selectedPreset?.usageScript) account.usageScript = selectedPreset.usageScript;
    submitSave("save_api_account", account);
  }
  const usage = section(pages.balance, "balanceTitle", "usageHint");
  const enabledLabel = el("label", "api-usage-enabled");
  const enabled = el("input"); enabled.type = "checkbox"; enabledLabel.append(enabled, label(el("span"), "usageEnabled")); usage.append(enabledLabel);
  const usageBody = el("div"); usage.append(usageBody);
  const templateGrid = grid(usageBody);
  const template = field(templateGrid, "template", "template", "select", true);
  option(template, "custom", "customScript"); option(template, "general", "general"); option(template, "newapi", "newapi"); option(template, "sub2api", "sub2api");
  const credentialSection = section(usageBody, "credentials"); const credentialHint = el("p", "api-page-hint"); credentialSection.append(credentialHint);
  const credentialGrid = grid(credentialSection);
  field(credentialGrid, "scriptApiKey", "scriptKey", "password"); field(credentialGrid, "scriptBaseUrl", "scriptUrl"); field(credentialGrid, "accessToken", "accessToken", "password"); field(credentialGrid, "userId", "userId");
  const legacySection = section(usageBody, "legacy", "configuredHint"); const legacyGrid = grid(legacySection);
  field(legacyGrid, "url", "url", "text", true); field(legacyGrid, "method", "method"); field(legacyGrid, "path", "path"); field(legacyGrid, "headers", "headers", "textarea", true); field(legacyGrid, "body", "body", "textarea", true); field(legacyGrid, "unit", "unit");
  const scheduleSection = section(usageBody, "schedule"); const scheduleGrid = grid(scheduleSection);
  field(scheduleGrid, "timeout", "timeout", "number"); fields.timeout.min = "0";
  const source = el("details", "api-script-details"); source.append(label(el("summary"), "scriptCode"), label(el("p", "api-page-hint"), "scriptHint"));
  field(source, "code", "code", "textarea", true).className = "api-config-source api-usage-source"; usageBody.append(source);
  const previewSection = section(pages.balance, "testResult");
  const previewButton = button("testScript", () => guarded(previewBalance)); previewSection.append(previewButton);
  const balancePreview = el("div", "api-preview-result"); balancePreview.setAttribute("role", "status"); previewSection.append(balancePreview);
  function updateUsage() {
    usageBody.hidden = !enabled.checked; previewButton.disabled = !enabled.checked;
    const kind = template.value;
    legacySection.hidden = kind !== "legacy"; source.hidden = kind === "legacy";
    credentialSection.hidden = kind === "legacy" || kind === "custom";
    fieldWraps.scriptApiKey.hidden = kind === "newapi"; fieldWraps.accessToken.hidden = kind !== "newapi" && kind !== "custom"; fieldWraps.userId.hidden = kind !== "newapi" && kind !== "custom";
    credentialHint.textContent = t(kind === "newapi" ? "newapiHint" : "keyHint");
  }
  template.addEventListener("change", () => {
    const kind = template.value;
    if (kind !== "legacy") fields.code.value = window.CCS_USAGE_TEMPLATES[kind];
    if (kind === "custom") ["scriptApiKey", "scriptBaseUrl", "accessToken", "userId"].forEach(key => { fields[key].value = ""; });
    else if (kind === "general" || kind === "sub2api") { fields.accessToken.value = ""; fields.userId.value = ""; }
    else if (kind === "newapi") fields.scriptApiKey.value = "";
    source.open = kind === "custom"; updateUsage();
  });
  enabled.addEventListener("change", updateUsage);
  function openBalance(id) {
    editingId = id; previewId = ""; accountDraft = { ...getItem(id) }; scriptDraft = { ...accountDraft.usageScript }; balanceDraft = { ...accountDraft.balanceConfig };
    template.querySelector('option[value="legacy"]')?.remove();
    const legacy = scriptDraft.templateType === "legacy" || (!scriptDraft.enabled && (balanceDraft.url || balanceDraft.configuredUrl));
    if (legacy) option(template, "legacy", "legacy");
    template.value = legacy ? "legacy" : ["custom", "general", "newapi", "sub2api"].includes(scriptDraft.templateType) ? scriptDraft.templateType : "custom";
    enabled.checked = scriptDraft.enabled === true || !!balanceDraft.url;
    fields.code.value = scriptDraft.code || window.CCS_USAGE_TEMPLATES[template.value] || "";
    fields.timeout.value = scriptDraft.timeout ?? state.timeoutSec ?? "";
    fields.scriptApiKey.value = scriptDraft.apiKey || ""; fields.scriptBaseUrl.value = scriptDraft.baseUrl || ""; fields.accessToken.value = scriptDraft.accessToken || ""; fields.userId.value = scriptDraft.userId || "";
    ["path", "body", "unit"].forEach(key => { fields[key].value = balanceDraft[key] || ""; });
    fields.url.value = balanceDraft.url || balanceDraft.configuredUrl || ""; fields.method.value = balanceDraft.method || "GET"; fields.headers.value = JSON.stringify(balanceDraft.headers || {}, null, 2);
    source.open = template.value === "custom"; resetSecrets(); updateUsage(); save.disabled = false;
    balancePreview.textContent = t("testFirst"); balancePreview.classList.remove("error"); showPage("balance");
  }
  function configuredCode(config) {
    return `({ request: ${JSON.stringify({ url: config.url, method: config.method, headers: config.headers, body: config.body })}, extractor: (response) => { const data = typeof response === 'string' ? JSON.parse(response) : response; const remaining = ${JSON.stringify(config.path)}.split('.').filter(Boolean).reduce((value, key) => value[key], data); return { isValid: true, remaining, unit: ${JSON.stringify(config.unit)} }; } })`;
  }
  function collectBalance() {
    const usageScript = { ...scriptDraft, enabled: enabled.checked, templateType: template.value, language: "javascript", code: fields.code.value, apiKey: fields.scriptApiKey.value, baseUrl: fields.scriptBaseUrl.value, accessToken: fields.accessToken.value, userId: fields.userId.value };
    if (fields.timeout.value !== "") usageScript.timeout = Number(fields.timeout.value); else delete usageScript.timeout;
    const balanceConfig = { ...balanceDraft, url: "" };
    if (template.value === "legacy") {
      const headers = JSON.parse(fields.headers.value);
      if (!headers || typeof headers !== "object" || Array.isArray(headers)) throw new Error(t("invalidHeaders"));
      ["method", "path", "body", "unit"].forEach(key => { balanceConfig[key] = fields[key].value; });
      balanceConfig.headers = headers; balanceConfig.configuredUrl = fields.url.value.trim();
      usageScript.code = configuredCode({ ...balanceConfig, url: balanceConfig.configuredUrl });
    } else if (balanceDraft.url) balanceConfig.configuredUrl = balanceDraft.url;
    if (usageScript.enabled && !usageScript.code.trim()) throw new Error(t("requiredCode"));
    return { ...accountDraft, ...getItem(editingId), balanceConfig, usageScript };
  }
  function previewBalance() {
    if (!enabled.checked) throw new Error(t("scriptDisabled"));
    const account = collectBalance(); previewId = `preview-${crypto.randomUUID()}`;
    balancePreview.textContent = t("querying"); balancePreview.classList.remove("error"); previewButton.disabled = true;
    post("preview_api_balance", { id: previewId, account });
  }
  function submitSave(action, payload) {
    saveRequestId = crypto.randomUUID(); save.disabled = true;
    post(action, { ...payload, requestId: saveRequestId });
  }
  function savePage() {
    if (saveRequestId) return;
    if (currentPage === "form") saveAccount();
    else if (currentPage === "balance") {
      const { balanceConfig, usageScript } = collectBalance();
      submitSave("save_api_balance", { id: editingId, balanceConfig, usageScript });
    }
  }
  const importSection = section(pages.import, "importTitle");
  const importGrid = grid(importSection); field(importGrid, "importPath", "importPath", "text", true);
  importSection.append(button("importCCS", () => { post("import_ccs_api", { path: fields.importPath.value.trim() }); closePage(); }, true));
  function queryBalance(id) { if (state.balancePending.has(id)) return; state.balancePending.add(id); changed(); post("query_api_balance", { id }); }
  function syncBalances(refreshAll = false) {
    const activeIds = new Set();
    state.items.forEach(item => {
      const script = item.usageScript;
      const configured = script?.enabled === true ? !!script.code?.trim() : !!item.balanceConfig?.url;
      if (!configured) return;
      activeIds.add(item.id);
      const signature = JSON.stringify({ baseUrl: item.baseUrl, apiKey: item.apiKey, usageScript: script, balanceConfig: item.balanceConfig });
      if ((refreshAll || state.balanceSignatures.get(item.id) !== signature) && !state.balancePending.has(item.id)) {
        state.balanceSignatures.set(item.id, signature);
        queryBalance(item.id);
      }
    });
    state.balanceSignatures.forEach((_, id) => { if (!activeIds.has(id)) state.balanceSignatures.delete(id); });
  }
  function renderBalance(id) {
    const result = state.results.get(id);
    if (state.balancePending.has(id)) return t("querying");
    if (!result) return t("noBalance");
    if (result.error) return result.error;
    const data = result.data;
    const plans = Array.isArray(data) ? data : [data];
    return plans.map(plan => {
      if (plan === null || typeof plan !== "object") return typeof plan === "number" ? plan.toFixed(2) : typeof plan === "string" ? plan : t("noAmount");
      const prefix = plan.planName ? `${plan.planName} · ` : "";
      if (plan.isValid === false) return `${prefix}${plan.invalidMessage || t("invalid")}`;
      const amount = value => `${value === -1 ? "∞" : typeof value === "number" ? value.toFixed(2) : value}${plan.unit ? ` ${plan.unit}` : ""}`;
      const parts = [];
      const remaining = plan.remaining ?? plan.balance;
      if (remaining !== undefined) parts.push(`${t("remaining")} ${amount(remaining)}`);
      if (plan.used !== undefined) parts.push(`${t("used")} ${amount(plan.used)}`);
      if (plan.total !== undefined) parts.push(`${t("total")} ${amount(plan.total)}`);
      if (!parts.length) parts.push(t(plan.isValid === true ? "valid" : "noAmount"));
      return `${prefix}${parts.join(" · ")}`;
    }).join("\n");
  }
  window.apiAccountsUi = {
    getItems: () => state.items.map(item => ({ ...item, isCurrent: state.activeApiAccount === item.id, isFixedUpstream: state.proxyDispatchMode === "fixed" && state.proxyFixedApiAccount === item.id })),
    openEditor, openBalance, queryBalance, renderBalance, openCCSImport: () => showPage("import"),
    importCurrent: () => post("import_current_api"),
    check: id => { state.checks.set(id, { pending: true, text: t("checking") }); changed(); post("check_api", { id }); },
    switchAccount: id => post("switch_api_account", { id }),
    remove: id => post("delete_api_account", { id }),
    setPool: (id, enabled) => post("set_api_pool", { id, enabled }),
    getStatus: id => ({ ...state.checks.get(id), balancePending: state.balancePending.has(id), current: state.activeApiAccount === id, fixed: state.proxyDispatchMode === "fixed" && state.proxyFixedApiAccount === id }),
    getOfficialPool: () => state.officialPool || []
  };
  function runScript(msg) {
    const source = `self.onmessage = async ({data}) => { try { let code = data.code; for (const [key, value] of Object.entries(data.variables)) code = code.split('{{' + key + '}}').join(String(value)); const script = (0, eval)(code); const result = data.phase === 'request' ? script.request : await script.extractor(data.response); self.postMessage({ ok: true, result }); } catch (error) { self.postMessage({ ok: false, error: error.message }); } };`;
    const url = URL.createObjectURL(new Blob([source], { type: "text/javascript" }));
    const worker = new Worker(url);
    const seconds = msg.timeoutSec ?? state.timeoutSec;
    let timer;
    function finish(result) { clearTimeout(timer); worker.terminate(); URL.revokeObjectURL(url); post("api_script_result", { requestId: msg.requestId, ...result }); }
    worker.onmessage = event => finish(event.data);
    worker.onerror = event => { event.preventDefault(); finish({ ok: false, error: event.message }); };
    if (seconds > 0) timer = setTimeout(() => finish({ ok: false, error: t("timeoutError") }), seconds * 1000);
    try {
      const response = msg.responseText !== undefined ? JSON.parse(msg.responseText) : msg.response;
      worker.postMessage({ code: msg.code, variables: msg.variables, phase: msg.phase, response });
    } catch (error) {
      finish({ ok: false, error: error.message });
    }
  }
  if (window.chrome?.webview) {
    window.chrome.webview.addEventListener("message", event => {
      const msg = event.data;
      if (msg.type === "api_account_save_result") {
        if (!saveRequestId || msg.requestId !== saveRequestId || panel.hidden) return;
        saveRequestId = "";
        if (msg.ok) closePage();
        else { save.disabled = false; error.textContent = msg.error; }
      } else if (msg.type === "api_accounts") {
        state.items = msg.items; state.officialPool = msg.officialPool; state.timeoutSec = msg.timeoutSec; state.activeApiAccount = msg.activeApiAccount; state.proxyFixedApiAccount = msg.proxyFixedApiAccount; state.proxyDispatchMode = msg.proxyDispatchMode;
        syncBalances(); changed();
      } else if (msg.type === "refresh_api_balances") {
        syncBalances(true);
      } else if (msg.type === "codex_config_synced") {
        post("get_config");
        if (!panel.hidden && currentPage === "form" && editingId && editingId === msg.accountId) {
          if (editorConfigReady && fields.auth.value === loadedEditorAuth && fields.config.value === loadedEditorConfig) {
            const item = getItem(editingId);
            accountDraft = { ...item };
            editorConfigRequestId = crypto.randomUUID(); editorConfigReady = false;
            setEditorConfigEnabled(false); save.disabled = true;
            post("get_api_editor_config", { requestId: editorConfigRequestId, id: editingId, apiKey: item.apiKey, baseUrl: item.baseUrl, model: item.model });
          } else {
            syncNotice.textContent = t("configChangedDraft");
          }
        }
      } else if (msg.type === "api_editor_config_result") {
        if (!editorConfigRequestId || msg.requestId !== editorConfigRequestId || panel.hidden || currentPage !== "form") return;
        editorConfigRequestId = "";
        if (!msg.ok) {
          save.disabled = true; syncNotice.textContent = t("configUnavailable"); syncNotice.classList.add("error"); error.textContent = msg.error; return;
        }
        fields.auth.value = JSON.stringify(msg.auth, null, 2); fields.config.value = msg.config;
        loadedEditorAuth = fields.auth.value; loadedEditorConfig = fields.config.value;
        editorConfigReady = true; setEditorConfigEnabled(true);
        try { syncFromFiles(); syncNotice.textContent = t("configLoaded"); }
        catch (failure) { save.disabled = true; syncNotice.textContent = failure.message; syncNotice.classList.add("error"); }
      } else if (msg.type === "api_models_result") {
        if (!modelsRequestId || msg.requestId !== modelsRequestId) return;
        if (fields.baseUrl.value.trim() !== modelsBaseUrl || fields.apiKey.value.trim() !== modelsApiKey) { resetModels(); return; }
        modelsRequestId = ""; fetchModelsButton.disabled = false;
        if (!msg.ok) { modelsPhase = ""; renderModelsStatus(); error.textContent = msg.error; return; }
        msg.models.forEach(id => { const item = el("option", "", id); item.value = id; modelPicker.append(item); });
        modelPicker.hidden = msg.models.length === 0;
        modelsPhase = msg.models.length ? "modelsReady" : "modelsEmpty"; renderModelsStatus();
      } else if (msg.type === "accounts_list") post("get_api_accounts");
      else if (msg.type === "api_balance_result") {
        state.balancePending.delete(msg.id);
        let data = msg.result;
        if (msg.ok && typeof data === "string" && /^[\[{]/.test(data.trim())) { try { data = JSON.parse(data); } catch { /* Plain string results remain displayable. */ } }
        state.results.set(msg.id, msg.ok ? { data } : { error: msg.error });
        if (msg.id === previewId) { balancePreview.textContent = renderBalance(msg.id); balancePreview.classList.toggle("error", !msg.ok); previewButton.disabled = !enabled.checked; }
        syncBalances();
        changed();
      } else if (msg.type === "api_check_result") { state.checks.set(msg.id, { pending: false, error: !msg.ok, text: msg.ok ? t("ok") : msg.error }); changed(); }
      else if (msg.type === "api_script_eval") runScript(msg);
      else if (msg.type === "language_pack" || msg.type === "config") {
        const language = msg.type === "language_pack" ? msg.code : msg.language;
        if (language) {
          state.language = language; localized.forEach(({ node, key, property }) => { node[property] = t(key); });
          if (!panel.hidden) { heading.textContent = t(currentPage === "pick" ? "addAccount" : currentPage === "form" ? editingId ? "editAccount" : "addAccount" : currentPage === "balance" ? "balanceTitle" : "importTitle"); if (currentPage === "pick") renderPresets(); }
          renderModelsStatus(); changed();
        }
      }
    });
    post("get_api_accounts");
  }
  changed();
})();
