(function () {
  "use strict";

  const IDE_LIST = ["Code.exe", "Trae.exe", "Kiro.exe", "Antigravity.exe"];
  const FALLBACK_API_MODELS = [
    "gpt-5",
    "gpt-5-codex",
    "gpt-5-codex-mini",
    "gpt-5.1",
    "gpt-5.1-codex",
    "gpt-5.1-codex-mini",
    "gpt-5.1-codex-max",
    "gpt-5.2",
    "gpt-5.2-codex",
    "gpt-5.3-codex"
  ];
  const DEFAULT_I18N = {
    "app.brand": "Codex Account Switch",
    "tab.dashboard": "Dashboard",
    "tab.accounts": "Accounts",
    "tab.api": "API Proxy",
    "tab.traffic": "Traffic Logs",
    "tab.token": "Token Stats",
    "tab.cloud": "Cloud Accounts",
    "tab.about": "About",
    "tab.settings": "Settings",
    "toolbar.refresh": "Refresh Quota",
    "toolbar.add_current": "Add Account",
    "toolbar.login_new": "Login New",
    "toolbar.backup_current": "Backup Current",
    "toolbar.import_auth": "Add Account",
    "toolbar.import": "Import Backup",
    "toolbar.export": "Export Backup",
    "toolbar.clean_abnormal": "Clean Abnormal",
    "refresh.disabled": "Disabled",
    "refresh.countdown_prefix": "Next refresh in ",
    "refresh.countdown_suffix": "",
    "refresh.countdown_default": "--:--",
    "search.placeholder": "Search accounts...",
    "group.all": "All",
    "group.free": "Free",
    "group.plus": "Plus",
    "group.team": "Team",
    "group.pro": "Pro",
    "table.account": "Account",
    "table.quota": "Model Quota",
    "table.recent": "Last Used",
    "table.action": "Action",
    "about.title": "Codex Account Switch",
    "about.subtitle": "Professional Account Management",
    "about.author_label": "Author",
    "about.author_name": "Xiao Lan",
    "about.repo_label": "Open Source",
    "about.repo_link": "View Code",
    "about.check_update": "Check Update",
    "about.version_prefix": "Current Version: {version}",
    "about.version_checking": "Current Version: {version}, checking...",
    "about.version_check_failed": "Current Version: {version} (check failed)",
    "about.version_new": "Current Version: {version}, Latest: {latest}",
    "about.version_latest": "Current Version: {version} (latest)",
    "settings.title": "Default Settings",
    "settings.subtitle": "These settings are used on first launch and all future switches.",
    "settings.tab.general": "General",
    "settings.tab.account": "Account",
    "settings.tab.cloud": "Cloud Sync",
    "settings.language_label": "Language",
    "settings.ide_label": "IDE Executable",
    "settings.theme_label": "Theme",
    "settings.theme_auto": "Auto",
    "settings.theme_light": "Light",
    "settings.theme_dark": "Dark",
    "settings.theme_hint": "Auto follows your Windows theme.",
    "settings.auto_update_label": "Auto Update",
    "settings.auto_update_hint": "Check update automatically at startup and prompt before downloading.",
    "settings.tab_visibility_section": "Menu Visibility",
    "settings.tab_visibility_hint": "Choose which top tabs are shown. Hidden tabs stay hidden until re-enabled. Settings is always visible.",
    "settings.tab_visibility_badge": "Required",
    "settings.tab_visibility_locked": "This tab is required and always visible.",
    "settings.quota_refresh_section": "Quota Refresh",
    "settings.enable_auto_refresh_quota_label": "Enable Auto Quota Refresh",
    "settings.enable_auto_refresh_quota_hint": "When enabled, importing accounts will auto-refresh plan type and quota.",
    "settings.auto_mark_abnormal_accounts_label": "Auto Mark Abnormal Accounts",
    "settings.auto_mark_abnormal_accounts_hint": "When enabled, token-invalid or unauthorized accounts are marked abnormal and skipped for auto retry.",
    "settings.auto_delete_abnormal_accounts_label": "Auto Delete Abnormal Accounts",
    "settings.auto_delete_abnormal_accounts_hint": "When enabled, abnormal accounts are automatically deleted.",
    "settings.auto_refresh_all_label": "Auto Refresh All Accounts",
    "settings.auto_refresh_all_hint": "Refresh all accounts every {minutes} minutes in background.",
    "settings.auto_refresh_current_label": "Auto Refresh Current Account",
    "settings.auto_refresh_current_hint": "Refresh current active account quota every {minutes} minutes.",
    "settings.low_quota_prompt_label": "Low Quota Auto Switch Prompt",
    "settings.low_quota_prompt_hint": "When enabled, low quota will trigger switch prompt and notification window automatically.",
    "settings.webdav_section": "WebDAV Sync",
    "settings.webdav_enabled_label": "Enable WebDAV Sync",
    "settings.webdav_enabled_hint": "Sync account auth backups with other devices through your WebDAV storage.",
    "settings.webdav_warning": "Unless you fully trust the WebDAV provider, syncing may cause data leakage.",
    "settings.webdav_url_label": "WebDAV URL",
    "settings.webdav_url_hint": "Example: https://dav.example.com/remote.php/dav/files/username",
    "settings.webdav_remote_path_label": "Remote Folder",
    "settings.webdav_remote_path_hint": "App-only subfolder to store manifest and account files.",
    "settings.webdav_username_label": "Username",
    "settings.webdav_username_hint": "WebDAV login username.",
    "settings.webdav_password_label": "Password / App Password",
    "settings.webdav_password_hint": "Saved locally with Windows protection; leave blank to keep existing password.",
    "settings.webdav_password_saved": "Password saved",
    "settings.webdav_password_missing": "Password not saved",
    "settings.webdav_clear_password": "Clear saved password",
    "settings.webdav_auto_sync_label": "Auto Sync",
    "settings.webdav_auto_sync_hint": "When enabled, bidirectional sync runs every {minutes} minutes.",
    "settings.webdav_status_label": "Sync Status",
    "settings.webdav_last_sync": "Last sync: {time}",
    "settings.webdav_last_sync_empty": "Last sync: not yet",
    "settings.webdav_next_sync": "Next auto sync: {time}",
    "settings.webdav_next_sync_disabled": "Next auto sync: disabled",
    "settings.webdav_test": "Test Connection",
    "settings.webdav_upload": "Upload Now",
    "settings.webdav_download": "Download Now",
    "settings.webdav_reset_upload": "Reset Cloud & Upload",
    "settings.webdav_sync": "Bidirectional Sync",
    "settings.webdav_running": "Sync in progress...",
    "settings.webdav_idle": "Ready",
    "settings.webdav_interval_hint": "Enter 1-1440",
    "settings.refresh_minutes_label": "Interval (minutes)",
    "settings.minutes_hint": "Enter 1-240",
    "settings.countdown_prefix": "Remaining: ",
    "settings.first_run_toast": "Please confirm default settings for first launch",
    "cloud_account.title": "Cloud Accounts",
    "cloud_account.subtitle": "Download cloud auth.json files from third-party links provided by Token-JSON-Provider and keep local duplicates untouched.",
    "cloud_account.dependency_hint": "This feature depends on Token-JSON-Provider support: https://github.com/isxlan0/Token-JSON-Provider",
    "cloud_account.url_label": "Download URL",
    "cloud_account.url_hint": "Example: http://127.0.0.1:8000",
    "cloud_account.password_label": "Access Password",
    "cloud_account.password_hint": "Saved locally with Windows protection; leave blank to keep the current password.",
    "cloud_account.password_saved": "Password saved",
    "cloud_account.password_missing": "Password not saved",
    "cloud_account.clear_password": "Clear saved password",
    "cloud_account.auto_download_label": "Auto Download Cloud Accounts",
    "cloud_account.auto_download_hint": "When enabled, provider items are checked every {minutes} minutes. Local duplicates are kept.",
    "cloud_account.interval_label": "Interval (minutes)",
    "cloud_account.interval_hint": "Enter 1-1440",
    "cloud_account.status_label": "Download Status",
    "cloud_account.status_idle": "Ready",
    "cloud_account.status_running": "Downloading cloud accounts...",
    "cloud_account.status_running_progress": "Downloading cloud accounts... {progress}",
    "cloud_account.last_download": "Last download: {time}",
    "cloud_account.last_download_empty": "Last download: not yet",
    "cloud_account.next_download": "Next auto download: {time}",
    "cloud_account.next_download_disabled": "Next auto download: disabled",
    "cloud_account.download_now": "Download Cloud Accounts",
    "cloud_account.download_now_running": "Downloading...",
    "cloud_account.download_now_progress": "Downloading... {progress}",
    "dialog.webdav_conflict.title": "Resolve WebDAV Conflicts",
    "dialog.webdav_conflict.desc": "Choose which version to keep for each conflicting account.",
    "dialog.webdav_conflict.keep_local": "Keep Local",
    "dialog.webdav_conflict.keep_remote": "Keep Cloud",
    "dialog.webdav_conflict.local_note": "Use this device's auth.json and upload it to WebDAV.",
    "dialog.webdav_conflict.remote_note": "Use the WebDAV version and overwrite local auth.json.",
    "dialog.webdav_conflict.apply": "Apply Choices",
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
    "api.title": "API Playground",
    "api.subtitle": "Use selected account token to request Codex Responses API.",
    "api.model_label": "Model ID",
    "api.prompt_label": "Input Content",
    "api.output_label": "Model Output",
    "api.send": "Send Request",
    "api.model_placeholder": "e.g. gpt-5.3-codex",
    "api.prompt_placeholder": "Type your message here...",
    "proxy.title": "Service Config",
    "proxy.start": "Start Service",
    "proxy.stop": "Stop Service",
    "proxy.status_stopped": "Service stopped",
    "proxy.status_running": "Running on port {port}",
    "proxy.port_label": "Listen Port",
    "proxy.port_hint": "Local API proxy listen port. Default 8045.",
    "proxy.timeout_label": "Request Timeout (sec)",
    "proxy.timeout_hint": "Default 120 sec, range 30-7200. Includes long reasoning/stream wait.",
    "proxy.auto_start_label": "Auto-start with app",
    "proxy.auto_start_hint": "Start local API proxy automatically when app launches.",
    "proxy.allow_lan_label": "Allow LAN access",
    "proxy.allow_lan_hint_off": "Bind to 127.0.0.1 only (local-only, privacy-first).",
    "proxy.allow_lan_hint_on": "Bind to 0.0.0.0, LAN devices can access this proxy port.",
    "proxy.api_key_label": "API Key",
    "proxy.api_key_hint": "Use Authorization: Bearer <API_KEY> or x-api-key on each proxy request.",
    "proxy.stealth_mode_label": "Codex Client Use Local Proxy Mode",
    "proxy.stealth_mode_hint": "Start local proxy service first.",
    "api.subtab.proxy": "Proxy",
    "api.subtab.test": "Test",
    "proxy.dispatch_mode_label": "Account Dispatch",
    "proxy.dispatch_mode_hint": "Select account scheduling strategy for proxy forwarding.",
    "proxy.dispatch_mode_round_robin": "Round-robin",
    "proxy.dispatch_mode_random": "Random",
    "proxy.dispatch_mode_fixed": "Fixed account",
    "proxy.fixed_account_label": "Fixed Account",
    "proxy.fixed_account_hint": "Fixed mode always follows the current account selected in Account Management.",
    "proxy.fixed_account_current": "Current selected account",
    "quick.theme_title": "Switch Theme",
    "quick.language_title": "Switch Language",
    "traffic.title": "Traffic Logs",
    "traffic.subtitle": "View call details for all accounts or one specific account.",
    "traffic.label.account": "Account",
    "traffic.label.filter": "Filter",
    "traffic.filter.all": "All",
    "traffic.filter.error": "Errors Only",
    "traffic.search_placeholder": "Keyword filter (account/model/path)",
    "traffic.refresh": "Refresh",
    "traffic.th.status": "Status",
    "traffic.th.method": "Method",
    "traffic.th.model": "Model",
    "traffic.th.protocol": "Protocol",
    "traffic.th.account": "Account",
    "traffic.th.path": "Path",
    "traffic.th.tokens": "Token Usage",
    "traffic.th.elapsed": "Elapsed",
    "traffic.th.called_at": "Called At",
    "traffic.label.page_size": "Per Page",
    "traffic.prev": "Previous",
    "traffic.next": "Next",
    "traffic.empty": "No traffic logs",
    "traffic.page_info_empty": "Page 0 / 0",
    "traffic.page_info": "Page {page} / {totalPages}, total {total}",
    "token.title": "Token Stats",
    "token.subtitle": "Token usage statistics by account and time range.",
    "token.label.account": "Account",
    "token.label.period": "Period",
    "token.period.hour": "Hour",
    "token.period.day": "Day",
    "token.period.week": "Week",
    "token.refresh": "Refresh",
    "token.card.input": "Input Tokens",
    "token.card.active_account": "Active Account",
    "token.card.output": "Output Tokens",
    "token.card.total": "Total Tokens",
    "token.trend.title": "Token Usage Trend",
    "token.trend.model": "By Model",
    "token.trend.account": "By Account",
    "token.trend.empty": "No trend data",
    "token.trend.period.hour": "Hour (5-minute bins)",
    "token.trend.period.day": "Day (hourly bins)",
    "token.trend.period.week": "Week (daily bins)",
    "token.account_stats.title": "Account Breakdown",
    "token.account_stats.th.account": "Account",
    "token.account_stats.th.calls": "Calls",
    "token.account_stats.th.output": "Output Tokens",
    "token.account_stats.th.total": "Total Tokens",
    "token.account_stats.empty": "No account stats",
    "token.model_stats.th.model": "Model",
    "token.model_stats.th.output": "Output Tokens",
    "token.model_stats.th.total": "Total Tokens",
    "token.model_stats.empty": "No token stats",
    "confirm.switch_restart_ide": "Switch to {name} now? This will restart {ide}.",
    "confirm.switch_proxy_mode": "Switch to {name} now? Proxy mode is enabled, restart IDE or CLI to take effect.",
    "dialog.add_account.title": "Add New Account",
    "dialog.add_account.tab_oauth": "OAuth Auth",
    "dialog.add_account.tab_manual": "Manual Paste",
    "dialog.add_account.tab_current": "Import Current",
    "dialog.add_account.tab_file": "Quick Import",
    "dialog.add_account.oauth_desc": "Open browser to complete Codex login authorization, automatically obtain and save Token.",
    "dialog.add_account.oauth_btn": "Start OAuth Login",
    "dialog.add_account.oauth_monitor_title": "OAuth Authorization Monitoring",
    "dialog.add_account.oauth_status_idle": "Click start to open browser and listen for callback on local port {port}.",
    "dialog.add_account.oauth_status_listening": "Listening on local callback port {port}, waiting for authorization result.",
    "dialog.add_account.oauth_status_browser_opened": "Browser opened. Complete authorization there, then wait for callback.",
    "dialog.add_account.oauth_status_callback_received": "Callback received, exchanging token...",
    "dialog.add_account.oauth_status_done": "Authorization completed.",
    "dialog.add_account.oauth_status_browser_failed": "Browser did not open automatically. Open the authorization link manually.",
    "dialog.add_account.oauth_link_label": "Authorization Link",
    "dialog.add_account.oauth_open_link": "Open Link",
    "dialog.add_account.oauth_callback_label": "Browser did not redirect automatically? Paste callback URL here:",
    "dialog.add_account.oauth_callback_placeholder": "Paste full callback URL, e.g. http://localhost:1455/auth/callback?code=...&state=...",
    "dialog.add_account.oauth_submit_callback": "Submit Callback URL",
    "dialog.add_account.manual_desc": "Paste auth.json content below, supports batch paste for multiple accounts.",
    "dialog.add_account.manual_btn": "Import Token",
    "dialog.add_account.current_desc": "Import currently logged-in account from this device.",
    "dialog.add_account.current_btn": "Import Current Account",
    "dialog.add_account.file_desc": "Select existing OAuth authorization files to import.",
    "dialog.add_account.file_btn": "Select OAuth Files",
    "dialog.add_account.import_current": "Import Current Logged-in Account",
    "dialog.add_account.import_oauth": "Quick Import Existing OAuth",
    "dialog.add_account.login_new": "Login New Account",
    "dialog.backup.title": "Backup Current Account",
    "dialog.backup.name_label": "Account Name",
    "dialog.backup.name_placeholder": "Enter account name",
    "dialog.rename.title": "Rename Account",
    "dialog.rename.name_label": "New Account Name",
    "dialog.rename.name_placeholder": "Enter new account name",
    "dialog.import_auth.title": "Quick Import Existing OAuth",
    "dialog.import_auth.name_label": "Imported Account Name",
    "dialog.import_auth.name_placeholder": "Enter account name",
    "dialog.common.cancel": "Cancel",
    "dialog.common.save": "Save",
    "dialog.common.confirm": "Confirm",
    "dialog.confirm.title": "Please Confirm",
    "dialog.confirm.default_message": "Confirm this action?",
    "dialog.delete.title": "Delete Account",
    "dialog.delete.message": "Delete backup for {name}?",
    "clean_abnormal.confirm": "Delete {count} abnormal accounts?",
    "clean_abnormal.empty": "No abnormal accounts",
    "dialog.login_new.title": "Login New Account",
    "dialog.login_new.message": "Clear current login files and restart {ide}?",
    "accounts.empty": "No account backups",
    "tag.current": "Current",
    "tag.abnormal": "Abnormal",
    "tag.group_business": "Business",
    "tag.group_personal": "Personal",
    "tag.plan_free": "Free",
    "tag.plan_plus": "Plus",
    "tag.plan_team": "Team",
    "tag.plan_pro": "Pro",
    "tag.plan_unknown": "Unknown",
    "quota.gpt": "Codex",
    "quota.placeholder": "No quota data",
    "quota.format": "5H {q5} | 7D {q7} | Reset {r5}/{r7}",
    "quota.format_free": "7D {q7} | Reset {r7}",
    "action.switch_title": "Switch to this account",
    "action.switch": "Switch",
    "action.rename_title": "Rename account",
    "action.rename": "Rename",
    "action.refresh_title": "Refresh quota",
    "action.refresh": "Refresh",
    "action.delete_title": "Delete account",
    "action.delete": "Delete",
    "bulk.mode_on": "Exit Multi-select",
    "bulk.mode_off": "Multi-select",
    "bulk.refresh": "Batch Refresh",
    "bulk.refresh_running": "Refreshing quota...",
    "bulk.refresh_progress": "Refreshing quota... {progress}",
    "bulk.delete": "Batch Delete",
    "bulk.selected": "Selected {count}",
    "bulk.select_all": "Select all",
    "bulk.delete_confirm_title": "Batch Delete Accounts",
    "bulk.delete_confirm_message": "Delete {count} selected accounts?",
    "count.format": "Showing 1 to {total}, total {total}",
    "count.empty": "Showing 0 to 0, total 0",
    "status_code.invalid_name": "Save failed: invalid account name",
    "status_code.name_too_long": "Account name max length is 32",
    "status_code.auth_missing": "Save failed: current account file missing",
    "status_code.duplicate_name": "Duplicate name, please change it",
    "status_code.create_dir_failed": "Save failed: cannot create backup directory",
    "status_code.write_failed": "Operation failed: cannot write file",
    "status_code.not_found": "Operation failed: target not found",
    "status_code.userprofile_missing": "Operation failed: user directory not found",
    "status_code.restart_failed": "Failed to restart {ide}, please restart manually",
    "status_code.config_saved": "Settings saved",
    "status_code.backup_saved": "Account backup saved",
    "status_code.account_renamed": "Account renamed",
    "status_code.account_renamed_and_refreshed": "Account renamed and quota refreshed",
    "status_code.auth_json_invalid": "This file may not be a valid auth.json (missing required fields)",
    "status_code.auth_json_imported": "OAuth imported",
    "status_code.import_auth_batch_done": "Import completed: success {success}, failed {failed}",
    "status_code.import_auth_batch_partial": "Import partially succeeded: success {success}, failed {failed}",
    "status_code.import_auth_batch_failed": "Import failed: success {success}, failed {failed}",
    "status_code.cloud_account_batch_done": "Cloud import completed: success {success}, failed {failed}, skipped {skipped}",
    "status_code.cloud_account_batch_partial": "Cloud import partially succeeded: success {success}, failed {failed}, skipped {skipped}",
    "status_code.cloud_account_batch_failed": "Cloud import failed: success {success}, failed {failed}, skipped {skipped}",
    "status_code.last_error_prefix": "Last error",
    "status_code.low_quota_notification": "Low quota detected. Click tray notification to confirm switch.",
    "status_code.switch_success": "Switched successfully, restarting {ide}",
    "status_code.delete_success": "Deleted successfully",
    "status_code.login_new_success": "Login files cleared, restarting {ide}",
    "status_code.import_success": "Import completed",
    "status_code.export_success": "Export completed",
    "status_code.oauth_success": "OAuth authorization completed",
    "status_code.oauth_timeout": "OAuth authorization timed out",
    "status_code.oauth_error": "OAuth authorization failed",
    "status_code.browser_open_failed": "Browser failed to open automatically",
    "status_code.no_auth_code": "OAuth callback missing auth code",
    "status_code.token_request_failed": "Token exchange failed",
    "status_code.invalid_token_response": "Token response is invalid",
    "status_code.invalid_token_data": "Token data is invalid",
    "status_code.batch_import_success": "Batch import completed",
    "status_code.batch_import_failed": "Batch import failed",
    "status_code.no_json_found": "No valid JSON content detected",
    "status_code.proxy_started": "Proxy service started",
    "status_code.proxy_stopped": "Proxy service stopped",
    "status_code.proxy_not_running": "Proxy service is not running",
    "status_code.proxy_already_running": "Proxy service is already running",
    "status_code.proxy_invalid_port": "Proxy port is invalid",
    "status_code.proxy_wsa_startup_failed": "Proxy network initialization failed",
    "status_code.proxy_socket_failed": "Proxy socket creation failed",
    "status_code.proxy_bind_failed": "Proxy bind failed, port may be in use",
    "status_code.proxy_listen_failed": "Proxy listen failed",
    "status_code.wsa_startup_failed": "Network initialization failed",
    "status_code.thread_create_failed": "Failed to create worker thread",
    "status_code.challenge_gen_failed": "Failed to generate OAuth challenge",
    "status_code.state_gen_failed": "Failed to generate OAuth state",
    "status_code.random_gen_failed": "Failed to generate secure random bytes",
    "low_quota.prompt_title": "Low Quota Alert",
    "low_quota.prompt_message": "Current account {current} is down to {currentQuota}% in {window} window.\nSwitch to {best} ({bestQuota}%) and restart IDE?",
    "low_quota.prompt_message_dynamic": "Current account {current} is down to {currentQuota}% in {window} window.\nSwitch to {best} ({bestQuota}%) and restart IDE?",
    "status_code.switch_success_proxy_mode": "Switched successfully in proxy mode. Restart IDE or CLI to take effect.",
    "status_code.proxy_usage_limit_reached": "Current account quota exhausted. Auto-switching and retrying request.",
    "status_code.proxy_account_abnormal": "Current account is abnormal and unavailable. Switching and retrying request.",
    "status_code.proxy_auto_switched": "Auto-switched to another account and retrying request.",
    "status_code.proxy_abnormal_auto_switched": "Abnormal account skipped. Auto-switched to another account and retrying request.",
    "status_code.proxy_fixed_auto_switched": "Fixed account quota unavailable. Auto-switched to another account.",
    "status_code.proxy_low_quota": "Proxy mode detected low quota on current account.",
    "status_code.debug_action_ok": "Debug action triggered",
    "status_code.debug_action_unsupported": "This action is only available in Debug build",
    "status_code.import_cancelled": "Import cancelled",
    "status_code.export_cancelled": "Export cancelled",
    "status_code.open_url_failed": "Failed to open link",
    "status_code.open_url_success": "Link opened",
    "status_code.save_config_failed": "Failed to save settings",
    "status_code.cloud_account_invalid_config": "Cloud account config is incomplete",
    "status_code.cloud_account_download_success": "Latest cloud account downloaded",
    "status_code.cloud_account_duplicate_skipped": "Duplicate account detected, kept local copy",
    "status_code.cloud_account_provider_empty": "Provider has no downloadable account",
    "status_code.cloud_account_provider_request_failed": "Failed to request cloud account provider",
    "status_code.cloud_account_provider_invalid_index": "Provider index response is invalid",
    "status_code.cloud_account_provider_item_failed": "Failed to fetch latest cloud account",
    "status_code.cloud_account_invalid_payload": "Latest provider item is not a valid auth.json object",
    "status_code.cloud_account_download_running": "Cloud account download is already running",
    "status_code.cloud_account_progress": "Downloading cloud accounts... {progress}",
    "status_code.quota_refreshed": "Quota refreshed",
    "status_code.quota_refresh_running": "Quota refresh is already running, please wait...",
    "status_code.quota_refresh_failed": "Quota refresh failed",
    "status_code.quota_refresh_progress": "Refreshing quota... {progress}",
    "status_code.account_quota_refreshed": "Account quota refreshed",
    "status_code.import_auth_batch_progress": "Importing accounts... {progress}",
    "status_code.account_abnormal_marked": "Account auth is invalidated and has been marked abnormal. Please sign in again.",
    "status_code.account_abnormal_auto_deleted": "Abnormal account auto-deleted",
    "status_code.batch_refresh_done": "Batch quota refresh completed",
    "status_code.batch_delete_done": "Batch delete completed",
    "status_code.batch_delete_partial": "Batch delete partially completed",
    "status_code.batch_delete_failed": "Batch delete failed",
    "status_code.batch_empty": "Please select at least one account",
    "progress.count": "{current}/{total} items",
    "toolbar.refresh_running": "Refreshing quota...",
    "toolbar.refresh_progress": "Refreshing quota... {progress}",
    "toolbar.import_running": "Importing accounts...",
    "toolbar.import_progress": "Importing accounts... {progress}",
    "status_code.api_invalid_input": "API input is incomplete",
    "status_code.api_no_selected_account": "No active account selected in Account Management",
    "status_code.api_account_auth_missing": "Selected account auth file not found",
    "status_code.api_request_success": "API request succeeded",
    "status_code.api_request_failed": "API request failed",
    "status_code.invalid_callback_url": "Invalid callback URL",
    "status_code.oauth_session_not_active": "No OAuth session is active",
    "status_code.oauth_callback_submitted": "Callback URL submitted",
    "status_code.oauth_session_busy": "An OAuth session is already in progress",
    "status_code.unknown_action": "Unknown action",
    "update.failed": "Update check failed, please try again later",
    "update.latest": "Already up to date",
    "update.new": "New version detected: {latest}",
    "update.dialog.title": "Update Available",
    "update.dialog.current_label": "Current",
    "update.dialog.latest_label": "Latest",
    "update.dialog.notes_label": "Release Notes",
    "update.dialog.confirm_question": "Download and install the latest release now?",
    "update.dialog.message": "Current: {current}\nLatest: {latest}\n\nRelease Notes:\n{notes}\n\nDownload and install the latest release now?",
    "debug.title": "Debug Tools",
    "debug.notify": "Test: Low-Quota Notify",
    "low_quota.window_5h": "5-hour",
    "low_quota.window_7d": "7-day",
    "status_text.proxy_low_quota_current": "Proxy current account {account} remaining quota only {quota}% ({window} window)",
    "status_text.proxy_low_quota_suggest": ", recommended switch to {account} ({quota}%)",
    "status_text.proxy_usage_limit_reached": "Account {account} quota exhausted (usage_limit_reached)",
    "status_text.proxy_usage_limit_switching_retry": ", switching and retrying request",
    "status_text.proxy_account_abnormal_marked": "Account {account} request failed ({reason}), marked as abnormal",
    "status_text.proxy_abnormal_switching_retry": ", switching and retrying request",
    "status_text.proxy_auto_switched_retry": "Auto-switched to {account}, retrying request",
    "status_text.proxy_abnormal_auto_switched_retry": "Auto-switched to {account} after abnormal account, retrying request",
    "status_text.proxy_auto_switch_failed": "Auto account switch failed: {error}",
    "status_text.proxy_fixed_auto_switched": "Fixed account quota unavailable, auto-switched to {account} and continuing request",
    "status_text.tray_switched_account": "Tray switched to account {account}",
    "status_text.low_quota_auto_switched": "Current account quota is low, auto-switched to the account with highest quota",
    "tray.notify_title": "Codex Quota Alert",
    "tray.proxy_auto_switching_suffix": ", auto switching account",
    "tray.proxy_auto_switched_retry": "Auto-switched to account {account}, and retrying current request",
    "tray.proxy_abnormal_auto_switched_retry": "Abnormal account skipped, switched to {account} and retrying",
    "tray.proxy_fixed_auto_switched": "Fixed account quota unavailable, auto-switched to {account}",
    "tray.low_quota_window_default": "quota",
    "tray.low_quota_window_suffix": "{window} quota",
    "tray.low_quota_current": "Current account {account} {window} dropped to {quota}%",
    "tray.low_quota_switch_suggest": ", can switch to {account} ({quota}%)",
    "tray.low_quota_click_hint": ". Click this notification to choose whether to switch and restart IDE.",
    "tray.menu.open_window": "Open Window",
    "tray.menu.minimize_to_tray": "Minimize to Tray",
    "tray.menu.exit": "Exit",
    "tray.menu.quick_switch": "Quick Switch Account",
    "tray.menu.no_switchable": "No switchable account",
    "tray.quota_format": "5H {q5} | 7D {q7}",
    "tray.quota_na": "--",
    "ide.Code.exe": "VSCode",
    "ide.Trae.exe": "Trae",
    "ide.Kiro.exe": "Kiro",
    "ide.Antigravity.exe": "Antigravity"
  }
    ;

  const ZH_FALLBACK_I18N = {
    "app.brand": "Codex Account Switch",
    "tab.accounts": "账号管理",
    "tab.about": "关于",
    "tab.settings": "设置",
    "toolbar.login_new": "登录新账号",
    "toolbar.backup_current": "备份当前账号",
    "refresh.disabled": "已关闭",
    "refresh.countdown_prefix": "下次刷新倒计时 ",
    "refresh.countdown_suffix": "",
    "refresh.countdown_default": "--:--",
    "search.placeholder": "搜索账号...",
    "group.all": "全部",
    "table.account": "账号",
    "table.quota": "模型配额",
    "table.recent": "最近使用",
    "table.action": "操作",
    "about.title": "Codex Account Switch",
    "about.subtitle": "专业账号管理",
    "about.author_label": "作者",
    "about.author_name": "Xiao Lan",
    "about.repo_label": "开源地址",
    "about.repo_link": "查看代码",
    "about.check_update": "检查更新",
    "about.version_prefix": "当前版本: {version}",
    "about.version_checking": "当前版本: {version}，检查中...",
    "about.version_check_failed": "当前版本: {version}（检查失败）",
    "about.version_new": "当前版本: {version}，最新: {latest}",
    "about.version_latest": "当前版本: {version}（已最新）",
    "settings.title": "默认设置",
    "settings.subtitle": "首次启动和后续切换都会使用这些设置。",
    "settings.language_label": "语言",
    "settings.ide_label": "IDE 可执行文件",
    "settings.theme_label": "主题",
    "settings.theme_auto": "自动",
    "settings.theme_light": "浅色",
    "settings.theme_dark": "深色",
    "settings.theme_hint": "自动模式会跟随 Windows 主题。",
    "settings.auto_update_label": "自动更新",
    "settings.auto_update_hint": "启动时自动检查更新，并在下载前提示。",
    "settings.tab_visibility_section": "菜单显示设置",
    "settings.tab_visibility_hint": "选择顶部导航中显示哪些页面。关闭后对应 Tab 不再显示，设置页始终固定显示。",
    "settings.tab_visibility_badge": "必选",
    "settings.tab_visibility_locked": "此页面为必选项，始终显示。",
    "settings.quota_refresh_section": "额度刷新",
    "settings.enable_auto_refresh_quota_label": "开启自动刷新额度",
    "settings.enable_auto_refresh_quota_hint": "开启后，导入账号时会自动刷新账号类型和额度。",
    "settings.auto_mark_abnormal_accounts_label": "自动标记异常账号",
    "settings.auto_mark_abnormal_accounts_hint": "开启后，遇到 token 失效或鉴权失败的账号会被标记为异常，并在自动重试时跳过。",
    "settings.auto_delete_abnormal_accounts_label": "自动删除异常账号",
    "settings.auto_delete_abnormal_accounts_hint": "开启后，异常账号会被自动删除。",
    "settings.webdav_section": "WebDAV 云同步",
    "settings.webdav_enabled_label": "启用 WebDAV 同步",
    "settings.webdav_enabled_hint": "通过 WebDAV 云存储在多台设备之间同步账号 auth 备份。",
    "settings.webdav_warning": "除非您十分信任 WebDAV 地址提供商，否则同步可能导致数据泄露。",
    "settings.webdav_url_label": "WebDAV 地址",
    "settings.webdav_url_hint": "例如：https://dav.example.com/remote.php/dav/files/username",
    "settings.webdav_remote_path_label": "远端目录",
    "settings.webdav_remote_path_hint": "应用专用子目录，用于保存清单和账号文件。",
    "settings.webdav_username_label": "用户名",
    "settings.webdav_username_hint": "WebDAV 登录用户名。",
    "settings.webdav_password_label": "密码 / 应用专用密码",
    "settings.webdav_password_hint": "使用 Windows 保护保存在本机；留空则保留当前已保存密码。",
    "settings.webdav_password_saved": "密码已保存",
    "settings.webdav_password_missing": "密码未保存",
    "settings.webdav_clear_password": "清除已保存密码",
    "settings.webdav_auto_sync_label": "自动同步",
    "settings.webdav_auto_sync_hint": "开启后，每 {minutes} 分钟执行一次双向同步。",
    "settings.webdav_status_label": "同步状态",
    "settings.webdav_last_sync": "最近同步：{time}",
    "settings.webdav_last_sync_empty": "最近同步：尚未执行",
    "settings.webdav_next_sync": "下次自动同步：{time}",
    "settings.webdav_next_sync_disabled": "下次自动同步：已关闭",
    "settings.webdav_test": "测试连接",
    "settings.webdav_upload": "立即上传",
    "settings.webdav_download": "立即下载",
    "settings.webdav_reset_upload": "删除云端并同步最新",
    "settings.webdav_sync": "立即双向同步",
    "settings.webdav_running": "同步进行中...",
    "settings.webdav_idle": "已就绪",
    "settings.webdav_interval_hint": "输入 1-1440",
    "settings.minutes_hint": "输入 1-240",
    "settings.first_run_toast": "首次启动请先确认默认设置",
    "dialog.webdav_conflict.title": "处理 WebDAV 冲突",
    "dialog.webdav_conflict.desc": "请为每个冲突账号选择保留本地版本还是云端版本。",
    "dialog.webdav_conflict.keep_local": "保留本地",
    "dialog.webdav_conflict.keep_remote": "保留云端",
    "dialog.webdav_conflict.local_note": "使用当前设备上的 auth.json，并上传覆盖到 WebDAV。",
    "dialog.webdav_conflict.remote_note": "使用 WebDAV 中的版本，并覆盖本地 auth.json。",
    "dialog.webdav_conflict.apply": "应用选择",
    "dialog.backup.title": "备份当前账号",
    "dialog.backup.name_label": "账号名称",
    "dialog.backup.name_placeholder": "请输入账号名称",
    "dialog.import_auth.title": "快速导入已有 OAuth",
    "dialog.import_auth.name_label": "导入账号名称",
    "dialog.import_auth.name_placeholder": "请输入账号名称",
    "dialog.common.cancel": "取消",
    "dialog.common.save": "保存",
    "dialog.common.confirm": "确认",
    "dialog.confirm.title": "请确认",
    "dialog.confirm.default_message": "确认执行此操作吗？",
    "dialog.delete.title": "删除账号",
    "dialog.delete.message": "确认删除账号备份 {name} 吗？",
    "clean_abnormal.confirm": "是否删除 {count} 个异常账号？",
    "clean_abnormal.empty": "当前没有异常账号",
    "dialog.login_new.title": "登录新账号",
    "dialog.login_new.message": "将清理当前登录文件并重启 {ide}，是否继续？",
    "accounts.empty": "暂无账号备份",
    "tag.current": "当前",
    "tag.abnormal": "异常",
    "tag.group_business": "企业",
    "tag.group_personal": "个人",
    "quota.gpt": "Codex",
    "quota.placeholder": "暂无配额数据",
    "action.switch_title": "切换到此账号",
    "action.switch": "切换",
    "action.refresh_title": "刷新配额",
    "action.refresh": "刷新",
    "count.format": "显示 1 到 {total}，共 {total}",
    "count.empty": "显示 0 到 0，共 0",
    "status_code.invalid_name": "保存失败：账号名无效",
    "status_code.name_too_long": "账号名最长 32 个字符",
    "status_code.auth_missing": "保存失败：当前账号文件不存在",
    "status_code.duplicate_name": "名称重复，请修改后重试",
    "status_code.create_dir_failed": "保存失败：无法创建备份目录",
    "status_code.write_failed": "操作失败：无法写入文件",
    "status_code.not_found": "操作失败：未找到目标",
    "status_code.userprofile_missing": "操作失败：未找到用户目录",
    "status_code.restart_failed": "重启 {ide} 失败，请手动重启",
    "status_code.config_saved": "设置已保存",
    "status_code.cloud_account_invalid_config": "云账号配置不完整",
    "status_code.cloud_account_download_success": "最新云账号下载成功",
    "status_code.cloud_account_duplicate_skipped": "检测到重复账号，已保留本地版本",
    "status_code.cloud_account_provider_empty": "Provider 当前没有可下载账号",
    "status_code.cloud_account_provider_request_failed": "请求云账号 Provider 失败",
    "status_code.cloud_account_provider_invalid_index": "Provider 索引返回格式无效",
    "status_code.cloud_account_provider_item_failed": "获取最新云账号失败",
    "status_code.cloud_account_invalid_payload": "最新云端数据不是有效 auth.json 对象",
    "status_code.cloud_account_download_running": "云账号下载任务已在进行中",
    "status_code.cloud_account_progress": "正在下载云账号... {progress}",
    "status_code.backup_saved": "账号备份已保存",
    "status_code.auth_json_invalid": "该文件可能不是有效 auth.json（缺少必要字段）",
    "status_code.auth_json_imported": "OAuth 导入成功",
    "status_code.low_quota_notification": "检测到低额度，请点击托盘通知确认是否切换账号",
    "low_quota.prompt_title": "低额度提醒",
    "status_code.debug_action_ok": "调试操作已触发",
    "status_code.debug_action_unsupported": "该操作仅在 Debug 构建可用",
    "status_code.import_cancelled": "导入已取消",
    "status_code.export_cancelled": "导出已取消",
    "status_code.open_url_failed": "打开链接失败",
    "status_code.open_url_success": "链接已打开",
    "status_code.save_config_failed": "设置保存失败",
    "status_code.quota_refreshed": "额度已刷新",
    "status_code.quota_refresh_running": "额度刷新正在进行中，请稍候…",
    "status_code.quota_refresh_failed": "额度刷新失败",
    "status_code.account_quota_refreshed": "账号额度已刷新",
    "status_code.account_abnormal_marked": "账号认证已失效，已标记为异常，请重新登录",
    "status_code.account_abnormal_auto_deleted": "异常账号已自动删除",
    "status_code.unknown_action": "未知操作",
    "update.failed": "检查更新失败，请稍后重试",
    "update.latest": "当前已是最新版本",
    "update.new": "检测到新版本：{latest}",
    "update.dialog.title": "发现新版本",
    "update.dialog.current_label": "当前版本",
    "update.dialog.latest_label": "最新版本",
    "update.dialog.notes_label": "更新内容",
    "update.dialog.confirm_question": "是否立即下载并安装最新版本？",
    "update.dialog.message": "当前版本: {current}\n最新版本: {latest}\n\n更新内容:\n{notes}\n\n是否立即下载并安装最新版本？",
    "debug.title": "调试工具",
    "debug.notify": "测试：低额度通知",
    "ide.Code.exe": "VSCode",
    "ide.Trae.exe": "Trae",
    "ide.Kiro.exe": "Kiro",
    "ide.Antigravity.exe": "Antigravity",
    "tab.dashboard": "仪表盘",
    "tab.api": "API 反代",
    "tab.traffic": "流量日志",
    "tab.token": "Token统计",
    "tab.cloud": "云账号",
    "settings.tab.general": "通用",
    "settings.tab.account": "账号",
    "settings.tab.cloud": "云同步",
    "toolbar.refresh": "刷新额度",
    "toolbar.add_current": "添加账号",
    "toolbar.import_auth": "添加账号",
    "toolbar.import": "导入备份",
    "toolbar.export": "导出备份",
    "toolbar.clean_abnormal": "一键清理失效账号",
    "settings.refresh_minutes_label": "间隔（分钟）",
    "settings.enable_auto_refresh_quota_label": "开启自动刷新额度",
    "settings.enable_auto_refresh_quota_hint": "开启后，导入账号时会自动刷新账号类型和额度。",
    "settings.auto_mark_abnormal_accounts_label": "自动标记异常账号",
    "settings.auto_mark_abnormal_accounts_hint": "开启后，遇到 token 失效或鉴权失败的账号会被标记为异常，并在自动重试时跳过。",
    "settings.auto_refresh_all_label": "后台自动刷新全部账号额度",
    "settings.auto_refresh_current_label": "自动刷新当前活动账号额度",
    "settings.auto_refresh_all_hint": "每 {minutes} 分钟自动刷新一次全部账号额度。",
    "settings.auto_refresh_current_hint": "每 {minutes} 分钟自动刷新一次当前账号额度。",
    "settings.low_quota_prompt_label": "低额度自动提示切换账号",
    "settings.low_quota_prompt_hint": "开启后，额度过低时会自动弹出切换账号确认与提示信息窗口。",
    "group.free": "Free",
    "group.plus": "Plus",
    "group.team": "Team",
    "group.pro": "Pro",
    "settings.auto_refresh_current_on": "开启",
    "settings.auto_refresh_current_off": "关闭",
    "settings.countdown_prefix": "剩余时间：",
    "cloud_account.title": "云账号",
    "cloud_account.subtitle": "从 Token-JSON-Provider 所提供的第三方链接下载云账号 auth.json，并在命中重复时保留本地版本。",
    "cloud_account.dependency_hint": "此功能依赖 Token-JSON-Provider 提供支持：https://github.com/isxlan0/Token-JSON-Provider",
    "cloud_account.url_label": "下载链接",
    "cloud_account.url_hint": "例如：http://127.0.0.1:8000",
    "cloud_account.password_label": "访问密码",
    "cloud_account.password_hint": "使用 Windows 保护保存在本机；留空则保留当前已保存密码。",
    "cloud_account.password_saved": "密码已保存",
    "cloud_account.password_missing": "密码未保存",
    "cloud_account.clear_password": "清除已保存密码",
    "cloud_account.auto_download_label": "自动下载云账号",
    "cloud_account.auto_download_hint": "开启后，每 {minutes} 分钟检查一次云端账号列表；如与本地重复则保留本地。",
    "cloud_account.interval_label": "间隔（分钟）",
    "cloud_account.interval_hint": "输入 1-1440",
    "cloud_account.status_label": "下载状态",
    "cloud_account.status_idle": "已就绪",
    "cloud_account.status_running": "正在下载云账号...",
    "cloud_account.status_running_progress": "正在下载云账号... {progress}",
    "cloud_account.last_download": "最近下载：{time}",
    "cloud_account.last_download_empty": "最近下载：尚未执行",
    "cloud_account.next_download": "下次自动下载：{time}",
    "cloud_account.next_download_disabled": "下次自动下载：已关闭",
    "cloud_account.download_now": "一键下载云账号",
    "cloud_account.download_now_running": "正在下载...",
    "cloud_account.download_now_progress": "正在下载... {progress}",
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
    "api.title": "API 调试",
    "api.subtitle": "使用所选账号令牌请求 Codex Responses API。",
    "api.model_label": "模型 ID",
    "api.prompt_label": "发送内容",
    "api.output_label": "模型返回内容",
    "api.send": "发送请求",
    "api.model_placeholder": "例如 gpt-5.3-codex",
    "api.prompt_placeholder": "请输入请求内容...",
    "proxy.title": "服务配置",
    "proxy.start": "启动服务",
    "proxy.stop": "停止服务",
    "proxy.status_stopped": "服务已停止",
    "proxy.status_running": "服务运行中（端口 {port}）",
    "proxy.port_label": "监听端口",
    "proxy.port_hint": "本地 API 代理监听端口，默认 8045。",
    "proxy.timeout_label": "请求超时（秒）",
    "proxy.timeout_hint": "默认 120 秒，范围 30-7200 秒。用于等待上游响应（包含长文本/长推理）。",
    "proxy.auto_start_label": "跟随应用自动启动",
    "proxy.auto_start_hint": "应用启动时自动启动本地 API 代理服务。",
    "proxy.allow_lan_label": "允许局域网访问",
    "proxy.allow_lan_hint_off": "仅监听 127.0.0.1，仅本机可访问（隐私优先）。",
    "proxy.allow_lan_hint_on": "监听 0.0.0.0，局域网其他设备也可访问该端口。",
    "proxy.api_key_label": "API 密钥",
    "proxy.api_key_hint": "调用代理时请在请求头传入：Authorization: Bearer <API_KEY> 或 x-api-key。",
    "proxy.stealth_mode_label": "Codex 客户端使用本地反代模式",
    "proxy.stealth_mode_hint": "⚠️ 需要先启动本地反向代理服务",
    "api.subtab.proxy": "反向代理",
    "api.subtab.test": "测试",
    "proxy.dispatch_mode_label": "账号调度模式",
    "proxy.dispatch_mode_hint": "决定反向代理请求使用哪个账号。如果是固定模式则使用账号管理内选中的账号使用。",
    "proxy.dispatch_mode_round_robin": "轮询账号",
    "proxy.dispatch_mode_random": "随机账号",
    "proxy.dispatch_mode_fixed": "固定账号",
    "proxy.fixed_account_label": "固定账号",
    "proxy.fixed_account_hint": "固定模式下自动使用账号管理当前选中账号。",
    "proxy.fixed_account_current": "当前选中账号",
    "quick.theme_title": "切换主题",
    "quick.language_title": "切换语言",
    "traffic.title": "流量日志",
    "traffic.subtitle": "查看全部账号或指定账号的调用明细",
    "traffic.label.account": "账号",
    "traffic.label.filter": "过滤",
    "traffic.filter.all": "全部",
    "traffic.filter.error": "仅错误日志",
    "traffic.search_placeholder": "关键字过滤(账号/模型/路径)",
    "traffic.refresh": "刷新",
    "traffic.th.status": "状态",
    "traffic.th.method": "方法",
    "traffic.th.model": "模型",
    "traffic.th.protocol": "协议",
    "traffic.th.account": "账号",
    "traffic.th.path": "路径",
    "traffic.th.tokens": "Token 消耗",
    "traffic.th.elapsed": "耗时",
    "traffic.th.called_at": "调用时间",
    "traffic.label.page_size": "每页",
    "traffic.prev": "上一页",
    "traffic.next": "下一页",
    "traffic.empty": "暂无流量日志",
    "traffic.page_info_empty": "第 0 / 0 页",
    "traffic.page_info": "第 {page} / {totalPages} 页，共 {total} 条",
    "token.title": "Token 统计",
    "token.subtitle": "按账号和时间窗口统计 Token 消费",
    "token.label.account": "账号",
    "token.label.period": "周期",
    "token.period.hour": "小时",
    "token.period.day": "日",
    "token.period.week": "周",
    "token.refresh": "刷新",
    "token.card.input": "输入 Token",
    "token.card.active_account": "活跃账号",
    "token.card.output": "输出 Token",
    "token.card.total": "总 Token",
    "token.trend.title": "Token 使用趋势",
    "token.trend.model": "分模型趋势",
    "token.trend.account": "分账号趋势",
    "token.trend.empty": "暂无趋势数据",
    "token.trend.period.hour": "小时(5分钟粒度)",
    "token.trend.period.day": "日(小时粒度)",
    "token.trend.period.week": "周(天粒度)",
    "token.account_stats.title": "分账号统计",
    "token.account_stats.th.account": "账号",
    "token.account_stats.th.calls": "调用次数",
    "token.account_stats.th.output": "输出 Token",
    "token.account_stats.th.total": "总 Token",
    "token.account_stats.empty": "暂无账号统计",
    "token.model_stats.th.model": "使用模型",
    "token.model_stats.th.output": "输出 Token",
    "token.model_stats.th.total": "总 Token",
    "token.model_stats.empty": "暂无 Token 统计",
    "confirm.switch_restart_ide": "确认切换到 {name}？将重启 {ide}。",
    "confirm.switch_proxy_mode": "确认切换到 {name}？当前为反代模式，不会结束 IDE，请手动重启 IDE 或 CLI 生效。",
    "dialog.add_account.title": "添加新账号",
    "dialog.add_account.tab_oauth": "OAuth授权",
    "dialog.add_account.tab_manual": "手动粘贴",
    "dialog.add_account.tab_current": "导入当前",
    "dialog.add_account.tab_file": "快速导入",
    "dialog.add_account.oauth_desc": "打开浏览器完成 Codex 登录授权，自动获取并保存 Token。",
    "dialog.add_account.oauth_btn": "开始 OAuth 登录",
    "dialog.add_account.oauth_monitor_title": "OAuth 授权监控",
    "dialog.add_account.oauth_status_idle": "点击开始后将打开浏览器，并监听本地 {port} 端口回调。",
    "dialog.add_account.oauth_status_listening": "正在监听本地回调端口 {port}，等待授权返回。",
    "dialog.add_account.oauth_status_browser_opened": "浏览器已打开，请在浏览器中完成授权并等待回调。",
    "dialog.add_account.oauth_status_callback_received": "已收到回调，正在交换令牌...",
    "dialog.add_account.oauth_status_done": "授权完成。",
    "dialog.add_account.oauth_status_browser_failed": "浏览器未自动打开，请手动打开授权链接。",
    "dialog.add_account.oauth_link_label": "授权链接",
    "dialog.add_account.oauth_open_link": "打开链接",
    "dialog.add_account.oauth_callback_label": "浏览器没有自动跳转？请在此处粘贴回调链接：",
    "dialog.add_account.oauth_callback_placeholder": "粘贴完整回调链接，例如：http://localhost:1455/auth/callback?code=...&state=...",
    "dialog.add_account.oauth_submit_callback": "提交回调链接",
    "dialog.add_account.manual_desc": "在下方粘贴 auth.json 内容，支持批量粘贴多个账号。",
    "dialog.add_account.manual_btn": "导入令牌",
    "dialog.add_account.current_desc": "导入本设备当前已登录的账号。",
    "dialog.add_account.current_btn": "导入当前账号",
    "dialog.add_account.file_desc": "选择已有的 OAuth 授权文件进行导入。",
    "dialog.add_account.file_btn": "选择 OAuth 文件",
    "dialog.add_account.import_current": "导入当前登录账号",
    "dialog.add_account.import_oauth": "快速导入已有OAuth",
    "dialog.add_account.login_new": "登录新账号",
    "dialog.rename.title": "重命名账号",
    "dialog.rename.name_label": "新账号名",
    "dialog.rename.name_placeholder": "请输入新账号名",
    "action.rename_title": "重命名账号",
    "action.rename": "重命名",
    "action.delete_title": "删除账号",
    "action.delete": "删除",
    "bulk.mode_on": "退出多选",
    "bulk.mode_off": "多选模式",
    "bulk.refresh": "批量刷新额度",
    "bulk.refresh_running": "正在刷新额度...",
    "bulk.refresh_progress": "正在刷新额度... {progress}",
    "bulk.delete": "批量删除账号",
    "bulk.selected": "已选 {count} 项",
    "bulk.select_all": "全选",
    "bulk.delete_confirm_title": "批量删除账号",
    "bulk.delete_confirm_message": "确认删除已选中的 {count} 个账号吗？",
    "status_code.batch_refresh_done": "批量刷新额度完成",
    "status_code.quota_refresh_running": "额度刷新正在进行中，请稍候…",
    "status_code.quota_refresh_failed": "额度刷新失败",
    "status_code.quota_refresh_progress": "正在刷新额度... {progress}",
    "status_code.batch_delete_done": "批量删除完成",
    "status_code.batch_delete_partial": "批量删除部分完成",
    "status_code.batch_delete_failed": "批量删除失败",
    "status_code.batch_empty": "请至少选择一个账号",
    "status_code.api_invalid_input": "API参数不完整",
    "status_code.api_no_selected_account": "账号管理中未选中当前账号",
    "status_code.api_account_auth_missing": "所选账号auth文件不存在",
    "status_code.api_request_success": "API请求成功",
    "status_code.api_request_failed": "API请求失败",
    "status_code.invalid_callback_url": "回调链接无效",
    "status_code.oauth_session_not_active": "当前没有进行中的 OAuth 授权",
    "status_code.oauth_callback_submitted": "已提交回调链接",
    "status_code.oauth_session_busy": "已有进行中的 OAuth 授权",
    "status_code.account_renamed": "账号已重命名",
    "status_code.account_renamed_and_refreshed": "账号已重命名并刷新额度",
    "status_code.import_auth_batch_done": "导入完成：成功 {success}，失败 {failed}",
    "status_code.import_auth_batch_partial": "导入部分成功：成功 {success}，失败 {failed}",
    "status_code.import_auth_batch_failed": "导入失败：成功 {success}，失败 {failed}",
    "status_code.import_auth_batch_progress": "正在导入账号... {progress}",
    "status_code.cloud_account_batch_done": "云账号导入完成：成功 {success}，失败 {failed}，跳过 {skipped}",
    "status_code.cloud_account_batch_partial": "云账号导入部分成功：成功 {success}，失败 {failed}，跳过 {skipped}",
    "status_code.cloud_account_batch_failed": "云账号导入失败：成功 {success}，失败 {failed}，跳过 {skipped}",
    "status_code.last_error_prefix": "最后错误",
    "progress.count": "{current}/{total}个",
    "toolbar.refresh_running": "正在刷新额度...",
    "toolbar.refresh_progress": "正在刷新额度... {progress}",
    "toolbar.import_running": "正在导入账号...",
    "toolbar.import_progress": "正在导入账号... {progress}",
    "status_code.switch_success": "切换成功，正在重启 {ide}",
    "status_code.delete_success": "删除成功",
    "status_code.login_new_success": "已清理登录文件，正在重启 {ide}",
    "status_code.import_success": "导入成功",
    "status_code.export_success": "导出成功",
    "status_code.oauth_success": "OAuth 授权完成",
    "status_code.oauth_timeout": "OAuth 授权超时",
    "status_code.oauth_error": "OAuth 授权失败",
    "status_code.browser_open_failed": "浏览器未能自动打开",
    "status_code.no_auth_code": "OAuth 回调缺少授权码",
    "status_code.token_request_failed": "令牌交换失败",
    "status_code.invalid_token_response": "令牌响应无效",
    "status_code.invalid_token_data": "令牌数据无效",
    "status_code.batch_import_success": "批量导入完成",
    "status_code.batch_import_failed": "批量导入失败",
    "status_code.no_json_found": "未检测到有效 JSON 内容",
    "status_code.proxy_started": "代理服务已启动",
    "status_code.proxy_stopped": "代理服务已停止",
    "status_code.proxy_not_running": "代理服务未运行",
    "status_code.proxy_already_running": "代理服务已在运行",
    "status_code.proxy_invalid_port": "代理端口无效",
    "status_code.proxy_wsa_startup_failed": "代理网络初始化失败",
    "status_code.proxy_socket_failed": "代理套接字创建失败",
    "status_code.proxy_bind_failed": "代理绑定失败，端口可能被占用",
    "status_code.proxy_listen_failed": "代理监听失败",
    "status_code.wsa_startup_failed": "网络初始化失败",
    "status_code.thread_create_failed": "创建工作线程失败",
    "status_code.challenge_gen_failed": "生成 OAuth challenge 失败",
    "status_code.state_gen_failed": "生成 OAuth state 失败",
    "status_code.random_gen_failed": "生成安全随机字节失败",
    "status_code.switch_success_proxy_mode": "反代模式下切换成功，请重启 IDE 或 CLI 生效。",
    "status_code.proxy_usage_limit_reached": "当前账号额度已用尽，正在自动切换并重试请求。",
    "status_code.proxy_account_abnormal": "当前账号鉴权异常，正在自动切换并重试请求。",
    "status_code.proxy_auto_switched": "已自动切换到其他账号并重试请求。",
    "status_code.proxy_abnormal_auto_switched": "已跳过异常账号并自动切换重试。",
    "status_code.proxy_fixed_auto_switched": "固定账号额度不可用，已自动切换到其他账号。",
    "status_code.proxy_low_quota": "反代模式检测到当前账号额度较低。",
    "low_quota.prompt_message": "当前账号 {current} 在 {window} 窗口额度已降至 {currentQuota}% 。\n是否切换到 {best}（{bestQuota}%）并重启 IDE？",
    "low_quota.prompt_message_dynamic": "当前账号 {current} 在 {window} 窗口额度已降至 {currentQuota}% 。\n是否切换到 {best}（{bestQuota}%）并重启 IDE？",
    "low_quota.window_5h": "5小时",
    "low_quota.window_7d": "7天",
    "status_text.proxy_low_quota_current": "反代当前账号 {account} 可用额度仅剩 {quota}%（{window}窗口）",
    "status_text.proxy_low_quota_suggest": "，建议切换到 {account}（{quota}%）",
    "status_text.proxy_usage_limit_reached": "账号 {account} 已用尽额度（usage_limit_reached）",
    "status_text.proxy_usage_limit_switching_retry": "，正在切换并重试请求",
    "status_text.proxy_account_abnormal_marked": "账号 {account} 请求失败（{reason}），已标记为异常",
    "status_text.proxy_abnormal_switching_retry": "，正在切换并重试请求",
    "status_text.proxy_auto_switched_retry": "已自动切换到 {account}，正在重试请求",
    "status_text.proxy_abnormal_auto_switched_retry": "因异常账号已自动切换到 {account}，正在重试请求",
    "status_text.proxy_auto_switch_failed": "自动切换账号失败: {error}",
    "status_text.proxy_fixed_auto_switched": "固定账号额度不可用，已自动切换到 {account} 并继续请求",
    "status_text.tray_switched_account": "托盘已切换到账号 {account}",
    "status_text.low_quota_auto_switched": "当前账号额度过低，已自动切换到额度最高的账号",
    "tray.notify_title": "Codex额度提醒",
    "tray.proxy_auto_switching_suffix": "，正在自动切换账号",
    "tray.proxy_auto_switched_retry": "已自动切换到账号 {account}，并继续重试当前请求",
    "tray.proxy_abnormal_auto_switched_retry": "已跳过异常账号并切换到 {account} 重试",
    "tray.proxy_fixed_auto_switched": "固定账号额度不可用，已自动切换到 {account}",
    "tray.low_quota_window_default": "额度",
    "tray.low_quota_window_suffix": "{window}额度",
    "tray.low_quota_current": "当前账号 {account} 的{window}已降至 {quota}%",
    "tray.low_quota_switch_suggest": "，可切换到 {account}（{quota}%）",
    "tray.low_quota_click_hint": "。点击此通知可选择是否切换并重启IDE。",
    "tray.menu.open_window": "打开页面",
    "tray.menu.minimize_to_tray": "最小化到托盘",
    "tray.menu.exit": "退出程序",
    "tray.menu.quick_switch": "快速切换账号",
    "tray.menu.no_switchable": "暂无可切换账号",
    "tray.quota_format": "5小时 {q5} | 7天 {q7}",
    "tray.quota_na": "--",
    "tag.plan_free": "Free",
    "tag.plan_plus": "Plus",
    "tag.plan_team": "Team",
    "tag.plan_pro": "Pro",
    "tag.plan_unknown": "未知类型",
    "quota.format_free": "7D {q7} | 重置 {r7}"
  };

  const dom = {
    brandTitle: document.getElementById("brandTitle"),
    tabBtnDashboard: document.getElementById("tabBtnDashboard"),
    tabBtnAccounts: document.getElementById("tabBtnAccounts"),
    tabBtnApi: document.getElementById("tabBtnApi"),
    tabBtnTraffic: document.getElementById("tabBtnTraffic"),
    tabBtnToken: document.getElementById("tabBtnToken"),
    tabBtnCloud: document.getElementById("tabBtnCloud"),
    tabBtnAbout: document.getElementById("tabBtnAbout"),
    tabBtnSettings: document.getElementById("tabBtnSettings"),
    toolbarQuick: document.getElementById("toolbarQuick"),
    quickThemeBtn: document.getElementById("quickThemeBtn"),
    quickLangBtn: document.getElementById("quickLangBtn"),
    quickLangMenu: document.getElementById("quickLangMenu"),
    toolbarActions: document.querySelector(".toolbar-actions"),
    addCurrentBtn: document.getElementById("addCurrentBtn"),
    refreshBtn: document.getElementById("refreshBtn"),
    importBtn: document.getElementById("importBtn"),
    exportBtn: document.getElementById("exportBtn"),
    cleanAbnormalBtn: document.getElementById("cleanAbnormalBtn"),
    searchInput: document.getElementById("searchInput"),
    groupAllBtn: document.getElementById("groupAllBtn"),
    groupFreeBtn: document.getElementById("groupFreeBtn"),
    groupPlusBtn: document.getElementById("groupPlusBtn"),
    groupTeamBtn: document.getElementById("groupTeamBtn"),
    groupProBtn: document.getElementById("groupProBtn"),
    bulkRow: document.getElementById("bulkRow"),
    multiSelectToggleBtn: document.getElementById("multiSelectToggleBtn"),
    bulkRefreshBtn: document.getElementById("bulkRefreshBtn"),
    bulkDeleteBtn: document.getElementById("bulkDeleteBtn"),
    bulkSelectedText: document.getElementById("bulkSelectedText"),
    accountsSectionTitle: document.getElementById("accountsSectionTitle"),
    proxyStealthModeToggle: document.getElementById("proxyStealthModeToggle"),
    proxyStealthModeLabel: document.getElementById("proxyStealthModeLabel"),
    proxyStealthModeHint: document.getElementById("proxyStealthModeHint"),
    selectAllCheckbox: document.getElementById("selectAllCheckbox"),
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
    settingsTabCloudBtn: document.getElementById("settingsTabCloudBtn"),
    settingsPaneGeneral: document.getElementById("settingsPaneGeneral"),
    settingsPaneAccount: document.getElementById("settingsPaneAccount"),
    settingsPaneCloud: document.getElementById("settingsPaneCloud"),
    settingsLanguageLabel: document.getElementById("settingsLanguageLabel"),
    settingsIdeLabel: document.getElementById("settingsIdeLabel"),
    settingsThemeLabel: document.getElementById("settingsThemeLabel"),
    settingsThemeHint: document.getElementById("settingsThemeHint"),
    themeAutoBtn: document.getElementById("themeAutoBtn"),
    themeLightBtn: document.getElementById("themeLightBtn"),
    themeDarkBtn: document.getElementById("themeDarkBtn"),
    settingsAutoUpdateLabel: document.getElementById("settingsAutoUpdateLabel"),
    settingsAutoUpdateHint: document.getElementById("settingsAutoUpdateHint"),
    settingsTabVisibilitySectionTitle: document.getElementById("settingsTabVisibilitySectionTitle"),
    settingsTabVisibilitySectionHint: document.getElementById("settingsTabVisibilitySectionHint"),
    settingsTabVisibilityList: document.getElementById("settingsTabVisibilityList"),
    settingsQuotaSectionTitle: document.getElementById("settingsQuotaSectionTitle"),
    settingsDisableAutoRefreshQuotaLabel: document.getElementById("settingsDisableAutoRefreshQuotaLabel"),
    settingsDisableAutoRefreshQuotaHint: document.getElementById("settingsDisableAutoRefreshQuotaHint"),
    settingsAutoMarkAbnormalAccountsLabel: document.getElementById("settingsAutoMarkAbnormalAccountsLabel"),
    settingsAutoMarkAbnormalAccountsHint: document.getElementById("settingsAutoMarkAbnormalAccountsHint"),
    settingsAutoDeleteAbnormalAccountsLabel: document.getElementById("settingsAutoDeleteAbnormalAccountsLabel"),
    settingsAutoDeleteAbnormalAccountsHint: document.getElementById("settingsAutoDeleteAbnormalAccountsHint"),
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
    settingsWebDavSectionTitle: document.getElementById("settingsWebDavSectionTitle"),
    settingsWebDavWarning: document.getElementById("settingsWebDavWarning"),
    settingsWebDavEnabledLabel: document.getElementById("settingsWebDavEnabledLabel"),
    settingsWebDavEnabledHint: document.getElementById("settingsWebDavEnabledHint"),
    webdavEnabledToggle: document.getElementById("webdavEnabledToggle"),
    settingsWebDavUrlLabel: document.getElementById("settingsWebDavUrlLabel"),
    settingsWebDavUrlHint: document.getElementById("settingsWebDavUrlHint"),
    webdavUrlInput: document.getElementById("webdavUrlInput"),
    settingsWebDavRemotePathLabel: document.getElementById("settingsWebDavRemotePathLabel"),
    settingsWebDavRemotePathHint: document.getElementById("settingsWebDavRemotePathHint"),
    webdavRemotePathInput: document.getElementById("webdavRemotePathInput"),
    settingsWebDavUsernameLabel: document.getElementById("settingsWebDavUsernameLabel"),
    settingsWebDavUsernameHint: document.getElementById("settingsWebDavUsernameHint"),
    webdavUsernameInput: document.getElementById("webdavUsernameInput"),
    settingsWebDavPasswordLabel: document.getElementById("settingsWebDavPasswordLabel"),
    settingsWebDavPasswordHint: document.getElementById("settingsWebDavPasswordHint"),
    webdavPasswordInput: document.getElementById("webdavPasswordInput"),
    webdavClearPasswordBtn: document.getElementById("webdavClearPasswordBtn"),
    webdavPasswordState: document.getElementById("webdavPasswordState"),
    settingsWebDavAutoSyncLabel: document.getElementById("settingsWebDavAutoSyncLabel"),
    settingsWebDavAutoSyncHint: document.getElementById("settingsWebDavAutoSyncHint"),
    webdavAutoSyncToggle: document.getElementById("webdavAutoSyncToggle"),
    settingsWebDavIntervalLabel: document.getElementById("settingsWebDavIntervalLabel"),
    webdavSyncIntervalInput: document.getElementById("webdavSyncIntervalInput"),
    settingsWebDavStatusLabel: document.getElementById("settingsWebDavStatusLabel"),
    webdavStatusDot: document.getElementById("webdavStatusDot"),
    webdavStatusText: document.getElementById("webdavStatusText"),
    webdavLastSyncText: document.getElementById("webdavLastSyncText"),
    webdavNextSyncText: document.getElementById("webdavNextSyncText"),
    webdavTestBtn: document.getElementById("webdavTestBtn"),
    webdavUploadBtn: document.getElementById("webdavUploadBtn"),
    webdavDownloadBtn: document.getElementById("webdavDownloadBtn"),
    webdavResetUploadBtn: document.getElementById("webdavResetUploadBtn"),
    webdavSyncBtn: document.getElementById("webdavSyncBtn"),
    languageOptions: document.getElementById("languageOptions"),
    ideOptions: document.getElementById("ideOptions"),
    autoUpdateToggle: document.getElementById("autoUpdateToggle"),
    disableAutoRefreshQuotaToggle: document.getElementById("disableAutoRefreshQuotaToggle"),
    autoMarkAbnormalAccountsToggle: document.getElementById("autoMarkAbnormalAccountsToggle"),
    autoDeleteAbnormalAccountsToggle: document.getElementById("autoDeleteAbnormalAccountsToggle"),
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
    dashCurrent5Row: document.getElementById("dashCurrent5Row"),
    dashCurrent5Label: document.getElementById("dashCurrent5Label"),
    dashCurrent7Label: document.getElementById("dashCurrent7Label"),
    dashCurrent5Progress: document.getElementById("dashCurrent5Progress"),
    dashCurrent5Value: document.getElementById("dashCurrent5Value"),
    dashCurrent7Value: document.getElementById("dashCurrent7Value"),
    dashCurrent5Bar: document.getElementById("dashCurrent5Bar"),
    dashCurrent7Bar: document.getElementById("dashCurrent7Bar"),
    dashboardSwitchBtn: document.getElementById("dashboardSwitchBtn"),
    apiTitle: document.getElementById("apiTitle"),
    apiSub: document.getElementById("apiSub"),
    apiSubTabTestBtn: document.getElementById("apiSubTabTestBtn"),
    apiSubTabProxyBtn: document.getElementById("apiSubTabProxyBtn"),
    apiPaneTest: document.getElementById("apiPaneTest"),
    apiPaneProxy: document.getElementById("apiPaneProxy"),
    apiModelLabel: document.getElementById("apiModelLabel"),
    apiModelInput: document.getElementById("apiModelInput"),
    apiPromptLabel: document.getElementById("apiPromptLabel"),
    apiPromptTextarea: document.getElementById("apiPromptTextarea"),
    apiSendBtn: document.getElementById("apiSendBtn"),
    apiOutputLabel: document.getElementById("apiOutputLabel"),
    apiOutputTextarea: document.getElementById("apiOutputTextarea"),
    proxyTitle: document.getElementById("proxyTitle"),
    proxyStatusDot: document.getElementById("proxyStatusDot"),
    proxyStatusText: document.getElementById("proxyStatusText"),
    proxyPortLabel: document.getElementById("proxyPortLabel"),
    proxyPortInput: document.getElementById("proxyPortInput"),
    proxyPortHint: document.getElementById("proxyPortHint"),
    proxyTimeoutLabel: document.getElementById("proxyTimeoutLabel"),
    proxyTimeoutInput: document.getElementById("proxyTimeoutInput"),
    proxyTimeoutHint: document.getElementById("proxyTimeoutHint"),
    proxyAutoStartLabel: document.getElementById("proxyAutoStartLabel"),
    proxyAutoStartHint: document.getElementById("proxyAutoStartHint"),
    proxyAllowLanLabel: document.getElementById("proxyAllowLanLabel"),
    proxyAllowLanHint: document.getElementById("proxyAllowLanHint"),
    proxyApiKeyLabel: document.getElementById("proxyApiKeyLabel"),
    proxyApiKeyInput: document.getElementById("proxyApiKeyInput"),
    proxyApiKeyHint: document.getElementById("proxyApiKeyHint"),
    proxyDispatchModeLabel: document.getElementById("proxyDispatchModeLabel"),
    proxyDispatchModeSelect: document.getElementById("proxyDispatchModeSelect"),
    proxyDispatchModeHint: document.getElementById("proxyDispatchModeHint"),
    proxyFixedAccountLabel: document.getElementById("proxyFixedAccountLabel"),
    proxyFixedAccountSelect: document.getElementById("proxyFixedAccountSelect"),
    proxyFixedAccountHint: document.getElementById("proxyFixedAccountHint"),
    proxyAutoStartToggle: document.getElementById("proxyAutoStartToggle"),
    proxyAllowLanToggle: document.getElementById("proxyAllowLanToggle"),
    proxyStartBtn: document.getElementById("proxyStartBtn"),
    proxyStopBtn: document.getElementById("proxyStopBtn"),
    trafficTitle: document.getElementById("trafficTitle"),
    trafficSubtitle: document.getElementById("trafficSubtitle"),
    trafficAccountFilterLabel: document.getElementById("trafficAccountFilterLabel"),
    trafficQuickFilterLabel: document.getElementById("trafficQuickFilterLabel"),
    trafficAccountFilter: document.getElementById("trafficAccountFilter"),
    trafficQuickFilterAllOption: document.querySelector("#trafficQuickFilter option[value='all']"),
    trafficQuickFilterErrorOption: document.querySelector("#trafficQuickFilter option[value='error']"),
    trafficRefreshBtn: document.getElementById("trafficRefreshBtn"),
    trafficBody: document.getElementById("trafficBody"),
    trafficQuickFilter: document.getElementById("trafficQuickFilter"),
    trafficSearchInput: document.getElementById("trafficSearchInput"),
    trafficThStatus: document.getElementById("trafficThStatus"),
    trafficThMethod: document.getElementById("trafficThMethod"),
    trafficThModel: document.getElementById("trafficThModel"),
    trafficThProtocol: document.getElementById("trafficThProtocol"),
    trafficThAccount: document.getElementById("trafficThAccount"),
    trafficThPath: document.getElementById("trafficThPath"),
    trafficThTokens: document.getElementById("trafficThTokens"),
    trafficThElapsed: document.getElementById("trafficThElapsed"),
    trafficThCalledAt: document.getElementById("trafficThCalledAt"),
    trafficPageSizeLabel: document.getElementById("trafficPageSizeLabel"),
    trafficPageSizeSelect: document.getElementById("trafficPageSizeSelect"),
    trafficPrevBtn: document.getElementById("trafficPrevBtn"),
    trafficNextBtn: document.getElementById("trafficNextBtn"),
    trafficPageInfo: document.getElementById("trafficPageInfo"),
    tokenTitle: document.getElementById("tokenTitle"),
    tokenSubtitle: document.getElementById("tokenSubtitle"),
    tokenAccountFilterLabel: document.getElementById("tokenAccountFilterLabel"),
    tokenAccountFilter: document.getElementById("tokenAccountFilter"),
    tokenPeriodSelectLabel: document.getElementById("tokenPeriodSelectLabel"),
    tokenPeriodSelect: document.getElementById("tokenPeriodSelect"),
    tokenPeriodHourOption: document.querySelector("#tokenPeriodSelect option[value='hour']"),
    tokenPeriodDayOption: document.querySelector("#tokenPeriodSelect option[value='day']"),
    tokenPeriodWeekOption: document.querySelector("#tokenPeriodSelect option[value='week']"),
    tokenRefreshBtn: document.getElementById("tokenRefreshBtn"),
    tokenCardInputLabel: document.getElementById("tokenCardInputLabel"),
    tokenCardActiveLabel: document.getElementById("tokenCardActiveLabel"),
    tokenCardOutputLabel: document.getElementById("tokenCardOutputLabel"),
    tokenCardTotalLabel: document.getElementById("tokenCardTotalLabel"),
    tokenInputValue: document.getElementById("tokenInputValue"),
    tokenActiveAccount: document.getElementById("tokenActiveAccount"),
    tokenOutputValue: document.getElementById("tokenOutputValue"),
    tokenTotalValue: document.getElementById("tokenTotalValue"),
    tokenTrendTitle: document.getElementById("tokenTrendTitle"),
    tokenModelBody: document.getElementById("tokenModelBody"),
    tokenTrendModelBtn: document.getElementById("tokenTrendModelBtn"),
    tokenTrendAccountBtn: document.getElementById("tokenTrendAccountBtn"),
    tokenTrendList: document.getElementById("tokenTrendList"),
    tokenAccountStatsTitle: document.getElementById("tokenAccountStatsTitle"),
    tokenAccountThAccount: document.getElementById("tokenAccountThAccount"),
    tokenAccountThCalls: document.getElementById("tokenAccountThCalls"),
    tokenAccountThOutput: document.getElementById("tokenAccountThOutput"),
    tokenAccountThTotal: document.getElementById("tokenAccountThTotal"),
    tokenModelThModel: document.getElementById("tokenModelThModel"),
    tokenModelThOutput: document.getElementById("tokenModelThOutput"),
    tokenModelThTotal: document.getElementById("tokenModelThTotal"),
    cloudAccountTitle: document.getElementById("cloudAccountTitle"),
    cloudAccountSubtitle: document.getElementById("cloudAccountSubtitle"),
    cloudAccountDependencyHint: document.getElementById("cloudAccountDependencyHint"),
    cloudAccountUrlLabel: document.getElementById("cloudAccountUrlLabel"),
    cloudAccountUrlInput: document.getElementById("cloudAccountUrlInput"),
    cloudAccountUrlHint: document.getElementById("cloudAccountUrlHint"),
    cloudAccountPasswordLabel: document.getElementById("cloudAccountPasswordLabel"),
    cloudAccountPasswordInput: document.getElementById("cloudAccountPasswordInput"),
    cloudAccountPasswordHint: document.getElementById("cloudAccountPasswordHint"),
    cloudAccountClearPasswordBtn: document.getElementById("cloudAccountClearPasswordBtn"),
    cloudAccountPasswordState: document.getElementById("cloudAccountPasswordState"),
    cloudAccountAutoDownloadLabel: document.getElementById("cloudAccountAutoDownloadLabel"),
    cloudAccountAutoDownloadToggle: document.getElementById("cloudAccountAutoDownloadToggle"),
    cloudAccountIntervalLabel: document.getElementById("cloudAccountIntervalLabel"),
    cloudAccountIntervalInput: document.getElementById("cloudAccountIntervalInput"),
    cloudAccountAutoDownloadHint: document.getElementById("cloudAccountAutoDownloadHint"),
    cloudAccountNextDownloadText: document.getElementById("cloudAccountNextDownloadText"),
    cloudAccountStatusLabel: document.getElementById("cloudAccountStatusLabel"),
    cloudAccountStatusDot: document.getElementById("cloudAccountStatusDot"),
    cloudAccountStatusText: document.getElementById("cloudAccountStatusText"),
    cloudAccountLastDownloadText: document.getElementById("cloudAccountLastDownloadText"),
    cloudAccountDownloadBtn: document.getElementById("cloudAccountDownloadBtn"),
    tokenAccountBody: document.getElementById("tokenAccountBody"),
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
    renameModal: document.getElementById("renameModal"),
    renameTitle: document.getElementById("renameTitle"),
    renameNameLabel: document.getElementById("renameNameLabel"),
    renameNameInput: document.getElementById("renameNameInput"),
    renameCancelBtn: document.getElementById("renameCancelBtn"),
    renameConfirmBtn: document.getElementById("renameConfirmBtn"),
    importAuthModal: document.getElementById("importAuthModal"),
    importAuthTitle: document.getElementById("importAuthTitle"),
    importAuthNameLabel: document.getElementById("importAuthNameLabel"),
    importAuthNameInput: document.getElementById("importAuthNameInput"),
    importAuthCancelBtn: document.getElementById("importAuthCancelBtn"),
    importAuthConfirmBtn: document.getElementById("importAuthConfirmBtn"),
    addAccountModal: document.getElementById("addAccountModal"),
    addAccountTitle: document.getElementById("addAccountTitle"),
    addTabOAuthBtn: document.getElementById("addTabOAuthBtn"),
    addTabManualBtn: document.getElementById("addTabManualBtn"),
    addTabCurrentBtn: document.getElementById("addTabCurrentBtn"),
    addTabFileBtn: document.getElementById("addTabFileBtn"),
    addPaneOAuth: document.getElementById("addPaneOAuth"),
    addPaneManual: document.getElementById("addPaneManual"),
    addPaneCurrent: document.getElementById("addPaneCurrent"),
    addPaneFile: document.getElementById("addPaneFile"),
    addPaneOAuthDesc: document.getElementById("addPaneOAuthDesc"),
    addPaneManualDesc: document.getElementById("addPaneManualDesc"),
    addPaneCurrentDesc: document.getElementById("addPaneCurrentDesc"),
    addPaneFileDesc: document.getElementById("addPaneFileDesc"),
    addAccountOAuthBtn: document.getElementById("addAccountOAuthBtn"),
    oauthMonitorPanel: document.getElementById("oauthMonitorPanel"),
    oauthMonitorTitle: document.getElementById("oauthMonitorTitle"),
    oauthMonitorStatus: document.getElementById("oauthMonitorStatus"),
    oauthAuthLinkLabel: document.getElementById("oauthAuthLinkLabel"),
    oauthAuthLinkInput: document.getElementById("oauthAuthLinkInput"),
    oauthOpenLinkBtn: document.getElementById("oauthOpenLinkBtn"),
    oauthCallbackLabel: document.getElementById("oauthCallbackLabel"),
    oauthCallbackTextarea: document.getElementById("oauthCallbackTextarea"),
    oauthSubmitCallbackBtn: document.getElementById("oauthSubmitCallbackBtn"),
    addAccountManualImportBtn: document.getElementById("addAccountManualImportBtn"),
    addAccountImportCurrentBtn: document.getElementById("addAccountImportCurrentBtn"),
    addAccountImportOAuthBtn: document.getElementById("addAccountImportOAuthBtn"),
    manualTokenTextarea: document.getElementById("manualTokenTextarea"),
    addAccountCancelBtn: document.getElementById("addAccountCancelBtn"),
    debugPanel: document.getElementById("debugPanel"),
    debugTitle: document.getElementById("debugTitle"),
    debugNotifyBtn: document.getElementById("debugNotifyBtn"),
    confirmModal: document.getElementById("confirmModal"),
    confirmTitle: document.getElementById("confirmTitle"),
    confirmMessage: document.getElementById("confirmMessage"),
    confirmCancelBtn: document.getElementById("confirmCancelBtn"),
    confirmOkBtn: document.getElementById("confirmOkBtn"),
    webdavConflictModal: document.getElementById("webdavConflictModal"),
    webdavConflictTitle: document.getElementById("webdavConflictTitle"),
    webdavConflictDesc: document.getElementById("webdavConflictDesc"),
    webdavConflictList: document.getElementById("webdavConflictList"),
    webdavConflictCancelBtn: document.getElementById("webdavConflictCancelBtn"),
    webdavConflictConfirmBtn: document.getElementById("webdavConflictConfirmBtn"),
    accountsWrap: document.querySelector(".accounts-wrap")
  };

  const TOP_LEVEL_TABS = [
    { key: "dashboard", icon: "\ud83d\udcca", button: dom.tabBtnDashboard, panelId: "tab-dashboard", hideable: true },
    { key: "accounts", icon: "\ud83d\udc65", button: dom.tabBtnAccounts, panelId: "tab-accounts", hideable: true },
    { key: "api", icon: "\ud83d\udd0c", button: dom.tabBtnApi, panelId: "tab-api", hideable: true },
    { key: "traffic", icon: "\ud83d\udcc3", button: dom.tabBtnTraffic, panelId: "tab-traffic", hideable: true },
    { key: "token", icon: "\ud83d\udcb0", button: dom.tabBtnToken, panelId: "tab-token", hideable: true },
    { key: "cloud", icon: "\u2601\ufe0f", button: dom.tabBtnCloud, panelId: "tab-cloud", hideable: true },
    { key: "about", icon: "\ud83d\udca1", button: dom.tabBtnAbout, panelId: "tab-about", hideable: true },
    { key: "settings", icon: "\u2699\ufe0f", button: dom.tabBtnSettings, panelId: "tab-settings", hideable: false }
  ];

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
    enableAutoRefreshQuota: true,
    autoMarkAbnormalAccounts: true,
    autoDeleteAbnormalAccounts: false,
    autoRefreshCurrent: true,
    lowQuotaAutoPrompt: true,
    autoRefreshAllMinutes: 15,
    autoRefreshCurrentMinutes: 5,
    settingsSubTab: "general",
    currentTab: "dashboard",
    themeMode: readCachedThemeMode(),
    firstRun: false,
    languageIndex: [],
    i18n: {},
    tabVisibility: getDefaultTabVisibility(),
    didAutoCheckUpdate: false,
    updateCheckContext: "manual",
    refreshMode: "",
    refreshTargetKey: "",
    refreshProgressCurrent: 0,
    refreshProgressTotal: 0,
    refreshProgressScope: "",
    importMode: "",
    importProgressCurrent: 0,
    importProgressTotal: 0,
    multiSelectMode: false,
    selectedAccountKeys: [],
    bulkMode: "",
    cleanAbnormalBusy: false,
    renameTargetName: "",
    renameTargetGroup: "personal",
    refreshBusyTimer: null,
    saveConfigTimer: null,
    configLoaded: false,
    hasPendingConfigWrite: false,
    pendingConfigSnapshot: null,
    pendingConfigAckTimer: null,
    lowQuotaPromptOpen: false,
    allRefreshRemainSec: -1,
    currentRefreshRemainSec: -1,
    addAccountTab: "oauth",
    oauthFlowActive: false,
    oauthAuthUrl: "",
    oauthPort: 1455,
    oauthFlowStage: "",
    oauthFlowMessage: "",
    apiSubTab: "proxy",
    apiModels: [],
    apiRequestBusy: false,
    proxyRunning: false,
    proxyAllowLan: false,
    proxyAutoStart: false,
    proxyApiKey: "",
    proxyStealthMode: false,
    proxyDispatchMode: "round_robin",
    proxyFixedAccount: "",
    proxyFixedGroup: "personal",
    proxyApiKeyEditing: false,
    cloudAccountUrl: "",
    cloudAccountPasswordConfigured: false,
    cloudAccountPasswordClear: false,
    cloudAccountAutoDownload: false,
    cloudAccountIntervalMinutes: 60,
    cloudAccountLastDownloadAt: "",
    cloudAccountLastDownloadStatus: "",
    cloudAccountDownloadRunning: false,
    cloudAccountRemainingSec: 0,
    cloudAccountProgressCurrent: 0,
    cloudAccountProgressTotal: 0,
    webdavEnabled: false,
    webdavAutoSync: true,
    webdavSyncIntervalMinutes: 15,
    webdavUrl: "",
    webdavRemotePath: "/CodexAccountSwitch",
    webdavUsername: "",
    webdavPasswordConfigured: false,
    webdavPasswordClear: false,
    webdavLastSyncAt: "",
    webdavLastSyncStatus: "",
    webdavSyncRunning: false,
    webdavSyncRemainingSec: 0,
    webdavConflicts: [],
    trafficLogs: [],
    trafficAccountFilter: "all",
    tokenStats: null,
    tokenAccountFilter: "all",
    tokenPeriod: "day",
    tokenTrendMode: "model",
    trafficQuickFilter: "all",
    trafficKeyword: "",
    trafficPageSize: 50,
    trafficPage: 1,
    accountsAutoRefreshMs: 8000,
    lastAccountsFetchAt: 0,
    trafficAutoRefreshMs: 6000,
    tokenAutoRefreshMs: 8000,
    lastTrafficFetchAt: 0,
    lastTokenFetchAt: 0,
    autoRefreshLoopTimer: null,
    quickLangMenuOpen: false
  };

  const customSelectRegistry = new Map();

  const mediaDark = window.matchMedia ? window.matchMedia("(prefers-color-scheme: dark)") : null;

  function log(msg) {
    if (!state.debug) return;
    dom.logEl.textContent = `[${new Date().toLocaleTimeString()}] ${msg}\n` + dom.logEl.textContent;
    if (window.console && typeof window.console.debug === "function") {
      window.console.debug(`[CAS] ${msg}`);
    }
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

  const progressToastMap = new Map();

  function showProgressToast(key, message, level = "info") {
    const toastKey = String(key || "").trim();
    if (!toastKey) return;
    const text = String(message || "").trim();
    if (!text) {
      clearProgressToast(toastKey);
      return;
    }
    let el = progressToastMap.get(toastKey);
    if (!el) {
      el = document.createElement("div");
      dom.toastWrap.appendChild(el);
      progressToastMap.set(toastKey, el);
    }
    el.className = `toast ${level}`;
    el.textContent = text;
    el.style.opacity = "1";
    el.style.transform = "";
  }

  function clearProgressToast(key) {
    const toastKey = String(key || "").trim();
    if (!toastKey) return;
    const el = progressToastMap.get(toastKey);
    if (!el) return;
    progressToastMap.delete(toastKey);
    el.style.opacity = "0";
    el.style.transform = "translateY(-6px)";
    setTimeout(() => el.remove(), 220);
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
    const isZhTw = langCode === "zh-tw";
    const isZhLike = isZhCn || isZhTw;
    let text = state.i18n[key];
    if (isZhLike && (typeof text !== "string" || text.length === 0) && ZH_FALLBACK_I18N[key] !== undefined) {
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

  function readCachedThemeMode() {
    try {
      return normalizeThemeMode(localStorage.getItem("cas_theme") || "auto");
    } catch (e) {
      return "auto";
    }
  }

  function initLanguageIndexFallback() {
    state.languageIndex = [{ code: "zh-CN", name: "简体中文", file: "zh-CN.json" }];
  }

  function findLanguageMeta(code) {
    return state.languageIndex.find((x) => String(x.code).toLowerCase() === String(code).toLowerCase());
  }

  function applyLanguageStrings(code, strings) {
    state.currentLanguage = code || state.currentLanguage || "zh-CN";
    const langCode = String(state.currentLanguage || "").toLowerCase();
    const fallbackBase = (langCode === "zh-cn" || langCode === "zh-tw")
      ? ZH_FALLBACK_I18N
      : DEFAULT_I18N;
    state.i18n = { ...fallbackBase, ...(strings || {}) };
    if (langCode === "zh-cn" || langCode === "zh-tw") {
      state.i18n["tag.current"] = "当前";
      state.i18n["tag.abnormal"] = "异常";
      state.i18n["tag.group_business"] = "企业";
      state.i18n["tag.group_personal"] = "个人";
      state.i18n["settings.enable_auto_refresh_quota_label"] = "开启自动刷新额度";
      state.i18n["settings.enable_auto_refresh_quota_hint"] = "开启后，导入账号时会自动刷新账号类型和额度。";
      state.i18n["settings.auto_mark_abnormal_accounts_label"] = "自动标记异常账号";
      state.i18n["settings.auto_mark_abnormal_accounts_hint"] = "开启后，遇到 token 失效或鉴权失败的账号会被标记为异常，并在自动重试时跳过。";
      state.i18n["status_code.account_abnormal_marked"] = "账号认证已失效，已标记为异常，请重新登录";
      state.i18n["settings.auto_delete_abnormal_accounts_label"] = "自动删除异常账号";
      state.i18n["settings.auto_delete_abnormal_accounts_hint"] = "开启后，异常账号会被自动删除。";
      state.i18n["status_code.account_abnormal_auto_deleted"] = "异常账号已自动删除";
    }
    applyI18n();
    refreshSettingsOptions();
    renderAccounts();
    renderQuickSwitchers();
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

  function getDefaultTabVisibility() {
    return TOP_LEVEL_TABS.reduce((acc, tab) => {
      acc[tab.key] = true;
      return acc;
    }, {});
  }

  function normalizeTabVisibility(value) {
    const normalized = getDefaultTabVisibility();
    const source = value && typeof value === "object" ? value : {};
    TOP_LEVEL_TABS.forEach((tab) => {
      if (tab.key === "settings") {
        normalized[tab.key] = true;
        return;
      }
      if (Object.prototype.hasOwnProperty.call(source, tab.key)) {
        normalized[tab.key] = !(source[tab.key] === false || source[tab.key] === "false");
      }
    });
    normalized.settings = true;
    return normalized;
  }

  function isTabVisible(tabKey) {
    const key = String(tabKey || "");
    if (!TOP_LEVEL_TABS.some((tab) => tab.key === key)) {
      return false;
    }
    return normalizeTabVisibility(state.tabVisibility)[key] !== false;
  }

  function getFirstVisibleTab() {
    return TOP_LEVEL_TABS.find((tab) => isTabVisible(tab.key))?.key || "settings";
  }

  function resolveVisibleTab(tabKey) {
    const key = String(tabKey || "");
    return isTabVisible(key) ? key : getFirstVisibleTab();
  }

  function renderTopLevelTabs() {
    const visibility = normalizeTabVisibility(state.tabVisibility);
    TOP_LEVEL_TABS.forEach((tab) => {
      if (!tab.button) return;
      tab.button.textContent = `${tab.icon} ${t(`tab.${tab.key}`)}`;
      tab.button.hidden = visibility[tab.key] === false;
    });
  }

  function ensureTabVisibilitySettingsRendered() {
    if (!dom.settingsTabVisibilityList || dom.settingsTabVisibilityList.childElementCount > 0) return;
    dom.settingsTabVisibilityList.classList.add("tab-visibility-grid");
    TOP_LEVEL_TABS.forEach((tab) => {
      const button = document.createElement("button");
      button.type = "button";
      button.className = "tab-visibility-card";
      button.setAttribute("data-tab-visibility-card", tab.key);

      const lockBadge = tab.hideable
        ? ""
        : `<span class="tab-visibility-badge">${escapeHtml(t("settings.tab_visibility_badge"))}</span>`;
      button.innerHTML = `
        ${lockBadge}
        <div class="tab-visibility-icon" data-tab-visibility-icon="${tab.key}">${tab.icon}</div>
        <div class="tab-visibility-name" data-tab-visibility-name="${tab.key}"></div>
        <div class="tab-visibility-state" data-tab-visibility-state="${tab.key}"></div>
      `;
      dom.settingsTabVisibilityList.appendChild(button);
    });
  }

  function renderTabVisibilitySettings() {
    if (!dom.settingsTabVisibilityList) return;
    ensureTabVisibilitySettingsRendered();
    TOP_LEVEL_TABS.forEach((tab) => {
      const button = dom.settingsTabVisibilityList.querySelector(`[data-tab-visibility-card="${tab.key}"]`);
      if (!button) return;
      const enabled = isTabVisible(tab.key);
      const locked = !tab.hideable;
      button.classList.toggle("active", enabled);
      button.classList.toggle("locked", locked);
      button.setAttribute("aria-pressed", enabled ? "true" : "false");
      button.disabled = locked;
      const nameEl = button.querySelector(`[data-tab-visibility-name="${tab.key}"]`);
      const stateEl = button.querySelector(`[data-tab-visibility-state="${tab.key}"]`);
      if (nameEl) nameEl.textContent = t(`tab.${tab.key}`);
      if (stateEl) {
        stateEl.textContent = locked
          ? t("settings.tab_visibility_locked")
          : (enabled ? t("settings.auto_update_on") : t("settings.auto_update_off"));
      }
    });
  }

  function switchTab(tab) {
    const nextTab = resolveVisibleTab(tab || state.currentTab || "dashboard");
    state.currentTab = nextTab;
    TOP_LEVEL_TABS.forEach((item) => {
      if (item.button) {
        item.button.classList.toggle("active", item.key === nextTab);
      }
      const panel = document.getElementById(item.panelId);
      if (panel) {
        panel.classList.toggle("active", item.key === nextTab);
      }
    });
    renderTopLevelTabs();
    renderToolbarActionsForTab(nextTab);
    if (nextTab === "dashboard" || nextTab === "accounts") {
      requestAccountsList(true);
    } else if (nextTab === "traffic") {
      requestTrafficLogs(true);
    } else if (nextTab === "token") {
      requestTokenStats(true);
    }
  }

  function renderToolbarActionsForTab(tab) {
    const t = String(tab || "");
    const showOnDashboard = t === "dashboard";
    const showOnAccounts = t === "accounts";
    if (dom.addCurrentBtn) dom.addCurrentBtn.classList.toggle("is-hidden", !(showOnDashboard || showOnAccounts));
    if (dom.refreshBtn) dom.refreshBtn.classList.toggle("is-hidden", !(showOnDashboard || showOnAccounts));
    if (dom.importBtn) dom.importBtn.classList.toggle("is-hidden", !showOnAccounts);
    if (dom.exportBtn) dom.exportBtn.classList.toggle("is-hidden", !showOnAccounts);
    if (dom.cleanAbnormalBtn) dom.cleanAbnormalBtn.classList.toggle("is-hidden", !(showOnDashboard || showOnAccounts));
    if (dom.toolbarActions) {
      const anyVisible = !dom.addCurrentBtn?.classList.contains("is-hidden")
        || !dom.refreshBtn?.classList.contains("is-hidden")
        || !dom.importBtn?.classList.contains("is-hidden")
        || !dom.exportBtn?.classList.contains("is-hidden")
        || !dom.cleanAbnormalBtn?.classList.contains("is-hidden");
      dom.toolbarActions.classList.toggle("is-hidden", !anyVisible);
      document.body.classList.toggle("has-floating-toolbar-actions", anyVisible);
    } else {
      document.body.classList.remove("has-floating-toolbar-actions");
    }
    renderCleanAbnormalButton();
  }

  function switchSettingsSubTab(tab) {
    state.settingsSubTab = (tab === "account" || tab === "cloud") ? tab : "general";
    document.querySelectorAll("[data-settings-tab]").forEach((x) => {
      x.classList.toggle("active", x.getAttribute("data-settings-tab") === state.settingsSubTab);
    });
    dom.settingsPaneGeneral.classList.toggle("active", state.settingsSubTab === "general");
    dom.settingsPaneAccount.classList.toggle("active", state.settingsSubTab === "account");
    dom.settingsPaneCloud.classList.toggle("active", state.settingsSubTab === "cloud");
  }

  function switchAddAccountTab(tab) {
    if (state.importMode === "oauth" && state.oauthFlowActive && tab !== "oauth") {
      return;
    }
    state.addAccountTab = tab;
    document.querySelectorAll("[data-add-tab]").forEach((x) => {
      x.classList.toggle("active", x.getAttribute("data-add-tab") === state.addAccountTab);
    });
    dom.addPaneOAuth.classList.toggle("active", state.addAccountTab === "oauth");
    dom.addPaneManual.classList.toggle("active", state.addAccountTab === "manual");
    dom.addPaneCurrent.classList.toggle("active", state.addAccountTab === "current");
    dom.addPaneFile.classList.toggle("active", state.addAccountTab === "file");
  }

  function openAddAccountModal() {
    if (!state.oauthFlowActive) {
      state.oauthFlowStage = "";
      state.oauthFlowMessage = "";
    }
    dom.addAccountModal.classList.add("show");
    document.body.classList.add("add-account-modal-open");
    renderOAuthFlowPanel();
  }

  function closeAddAccountModal(force = false) {
    if (!force && state.importMode === "oauth" && state.oauthFlowActive) {
      return;
    }
    dom.addAccountModal.classList.remove("show");
    document.body.classList.remove("add-account-modal-open");
    if (!state.oauthFlowActive) {
      dom.oauthCallbackTextarea.value = "";
    }
  }

  function renderOAuthFlowPanel() {
    const stage = String(state.oauthFlowStage || "");
    const messageByStage = {
      listening: t("dialog.add_account.oauth_status_listening", { port: state.oauthPort }),
      browser_opened: t("dialog.add_account.oauth_status_browser_opened"),
      callback_received: t("dialog.add_account.oauth_status_callback_received"),
      completed: t("dialog.add_account.oauth_status_done"),
      browser_open_failed: t("dialog.add_account.oauth_status_browser_failed")
    };
    const fallback = t("dialog.add_account.oauth_status_idle", { port: state.oauthPort });
    const statusText = String(state.oauthFlowMessage || "").trim() || messageByStage[stage] || fallback;

    dom.oauthMonitorPanel.classList.toggle("active", state.oauthFlowActive || !!state.oauthAuthUrl);
    dom.oauthMonitorStatus.textContent = statusText;
    dom.oauthAuthLinkInput.value = state.oauthAuthUrl || "";
    dom.oauthOpenLinkBtn.disabled = !state.oauthAuthUrl;
    dom.oauthSubmitCallbackBtn.disabled = !state.oauthFlowActive || state.importMode !== "oauth";
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
    var eff = resolveEffectiveTheme();
    document.documentElement.setAttribute("data-theme", eff);
    document.documentElement.style.colorScheme = eff;
    renderQuickSwitchers();
  }

  function renderQuickSwitchers() {
    if (dom.quickThemeBtn) {
      const mode = normalizeThemeMode(state.themeMode || "auto");
      if (mode === "auto") {
        dom.quickThemeBtn.textContent = "A";
        dom.quickThemeBtn.title = "主题: Auto（跟随系统）";
      } else if (mode === "dark") {
        dom.quickThemeBtn.textContent = "☾";
        dom.quickThemeBtn.title = "主题: Dark";
      } else {
        dom.quickThemeBtn.textContent = "☼";
        dom.quickThemeBtn.title = "主题: Light";
      }
    }
    if (dom.quickLangBtn) {
      const raw = String(state.currentLanguage || "zh-CN").trim();
      const shortCode = raw.split("-")[0].toUpperCase();
      dom.quickLangBtn.textContent = shortCode || "ZH";
      dom.quickLangBtn.title = `语言: ${raw || "zh-CN"}`;
    }
    renderQuickLangMenu();
  }

  function closeQuickLangMenu() {
    state.quickLangMenuOpen = false;
    if (dom.quickLangMenu) {
      dom.quickLangMenu.classList.remove("show");
    }
    if (dom.quickLangBtn) {
      dom.quickLangBtn.classList.remove("active");
    }
  }

  function renderQuickLangMenu() {
    if (!dom.quickLangMenu) return;
    const langs = Array.isArray(state.languageIndex) ? state.languageIndex : [];
    const current = String(state.currentLanguage || "").toLowerCase();
    dom.quickLangMenu.innerHTML = langs.map((lang) => {
      const code = String(lang?.code || "");
      const name = String(lang?.name || code);
      const isActive = code.toLowerCase() === current;
      const shortCode = code.replace(/-.*/, "").toUpperCase();
      return `
        <button class="quick-menu-item ${isActive ? "active" : ""}" data-quick-lang="${escapeHtml(code)}">
          <span class="quick-menu-code">${escapeHtml(shortCode)}</span>
          <span class="quick-menu-name">${escapeHtml(name)}</span>
          <span class="quick-menu-dot">${isActive ? "•" : ""}</span>
        </button>
      `;
    }).join("");
  }

  function applyLanguageByCode(code) {
    const nextCode = String(code || "").trim();
    if (!nextCode) return;
    state.currentLanguage = nextCode;
    refreshSettingsOptions();
    requestLanguagePack(nextCode);
    queueSaveConfig();
    renderQuickSwitchers();
  }

  function mountCustomSelect(selectEl) {
    if (!(selectEl instanceof HTMLSelectElement)) return;
    if (customSelectRegistry.has(selectEl)) {
      return;
    }

    const wrap = document.createElement("div");
    wrap.className = "custom-select-wrap";
    const computedWidth = window.getComputedStyle(selectEl).width;
    if (computedWidth && computedWidth !== "auto") {
      wrap.style.width = computedWidth;
    }
    selectEl.parentNode.insertBefore(wrap, selectEl);
    wrap.appendChild(selectEl);

    const btn = document.createElement("button");
    btn.type = "button";
    btn.className = "custom-select-btn";
    wrap.appendChild(btn);

    const menu = document.createElement("div");
    menu.className = "custom-select-menu";
    wrap.appendChild(menu);

    const closeMenu = () => {
      menu.classList.remove("show");
      btn.classList.remove("open");
    };

    const syncButtonText = () => {
      const selected = selectEl.options[selectEl.selectedIndex];
      btn.textContent = selected ? selected.textContent : "";
      btn.disabled = !!selectEl.disabled;
    };

    const rebuildMenu = () => {
      const selectedValue = String(selectEl.value ?? "");
      menu.innerHTML = Array.from(selectEl.options).map((opt) => {
        const value = String(opt.value ?? "");
        const active = value === selectedValue;
        return `
          <button type="button" class="custom-select-item ${active ? "active" : ""}" data-value="${escapeHtml(value)}">
            ${escapeHtml(String(opt.textContent || ""))}
          </button>
        `;
      }).join("");
      syncButtonText();
    };

    btn.addEventListener("click", (e) => {
      e.stopPropagation();
      if (btn.disabled) return;
      const opening = !menu.classList.contains("show");
      document.querySelectorAll(".custom-select-menu.show").forEach((x) => x.classList.remove("show"));
      document.querySelectorAll(".custom-select-btn.open").forEach((x) => x.classList.remove("open"));
      if (opening) {
        menu.classList.add("show");
        btn.classList.add("open");
      } else {
        closeMenu();
      }
    });

    menu.addEventListener("click", (e) => {
      const item = e.target.closest(".custom-select-item");
      if (!item) return;
      const next = String(item.getAttribute("data-value") || "");
      if (String(selectEl.value) !== next) {
        selectEl.value = next;
        selectEl.dispatchEvent(new Event("change", { bubbles: true }));
      }
      rebuildMenu();
      closeMenu();
    });

    selectEl.classList.add("custom-select-native");
    selectEl.addEventListener("change", rebuildMenu);
    rebuildMenu();

    const observer = new MutationObserver(() => {
      rebuildMenu();
    });
    observer.observe(selectEl, { childList: true, subtree: true, attributes: true });

    customSelectRegistry.set(selectEl, { rebuildMenu, closeMenu, observer });
  }

  function initCustomSelects() {
    document.querySelectorAll("select.settings-number-input").forEach((el) => mountCustomSelect(el));
  }

  function refreshCustomSelects() {
    customSelectRegistry.forEach((entry, selectEl) => {
      if (!document.body.contains(selectEl)) return;
      entry.rebuildMenu();
    });
  }

  function setThemeWithButtonTransition(buttonEl, nextMode) {
    const applyNext = () => {
      state.themeMode = normalizeThemeMode(nextMode);
      refreshSettingsOptions();
      applyTheme();
      queueSaveConfig();
    };
    if (!buttonEl || typeof document.startViewTransition !== "function") {
      applyNext();
      return;
    }
    const rect = buttonEl.getBoundingClientRect();
    document.documentElement.style.setProperty("--vt-x", `${Math.round(rect.left + rect.width / 2)}px`);
    document.documentElement.style.setProperty("--vt-y", `${Math.round(rect.top + rect.height / 2)}px`);
    document.startViewTransition(() => {
      applyNext();
    });
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
    state.tabVisibility = normalizeTabVisibility(state.tabVisibility);
    renderTopLevelTabs();
    renderTabVisibilitySettings();
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
    dom.disableAutoRefreshQuotaToggle.checked = state.enableAutoRefreshQuota;
    dom.autoMarkAbnormalAccountsToggle.checked = state.autoMarkAbnormalAccounts;
    dom.autoDeleteAbnormalAccountsToggle.checked = state.autoDeleteAbnormalAccounts;
    dom.lowQuotaAutoPromptToggle.checked = state.lowQuotaAutoPrompt;
    dom.settingsAutoRefreshAllHint.textContent = t("settings.auto_refresh_all_hint", { minutes: state.autoRefreshAllMinutes });
    dom.settingsAutoRefreshCurrentHint.textContent = t("settings.auto_refresh_current_hint", { minutes: state.autoRefreshCurrentMinutes });
    dom.autoRefreshAllMinutesInput.value = String(state.autoRefreshAllMinutes);
    dom.autoRefreshCurrentMinutesInput.value = String(state.autoRefreshCurrentMinutes);
    dom.proxyAutoStartToggle.checked = state.proxyAutoStart;
    dom.proxyAllowLanToggle.checked = state.proxyAllowLan;
    dom.proxyStealthModeToggle.checked = state.proxyStealthMode;
    dom.proxyDispatchModeSelect.value = state.proxyDispatchMode || "round_robin";
    renderCloudAccountState();
    renderWebDavState();
    renderProxyStatus();
    renderProxyFixedAccountOptions();
    refreshCustomSelects();
    switchSettingsSubTab(state.settingsSubTab);
  }

  function applyI18n() {
    renderTopLevelTabs();
    dom.quickThemeBtn.title = t("quick.theme_title");
    dom.quickLangBtn.title = t("quick.language_title");
    dom.accountsSectionTitle.textContent = "\ud83d\udc65 " + t("tab.accounts");
    renderPrimaryActionButtons();
    dom.importBtn.textContent = "\ud83d\udce5 " + t("toolbar.import");
    dom.exportBtn.textContent = "\ud83d\udce4 " + t("toolbar.export");
    dom.cleanAbnormalBtn.textContent = "\ud83e\uddf9 " + t("toolbar.clean_abnormal");
    dom.searchInput.placeholder = t("search.placeholder");
    dom.groupAllBtn.textContent = t("group.all");
    dom.groupFreeBtn.textContent = t("group.free");
    dom.groupPlusBtn.textContent = t("group.plus");
    dom.groupTeamBtn.textContent = t("group.team");
    dom.groupProBtn.textContent = t("group.pro");
    dom.thAccount.textContent = t("table.account");
    dom.thQuota.textContent = t("table.quota");
    dom.thRecent.textContent = t("table.recent");
    dom.thAction.textContent = t("table.action");
    dom.multiSelectToggleBtn.textContent = state.multiSelectMode ? "\u2716 " + t("bulk.mode_on") : "\u2610 " + t("bulk.mode_off");
    dom.bulkRefreshBtn.textContent = "\ud83d\udd04 " + t("bulk.refresh");
    dom.bulkDeleteBtn.textContent = "\ud83d\uddd1\ufe0f " + t("bulk.delete");
    dom.selectAllCheckbox.title = t("bulk.select_all");
    dom.aboutTitle.textContent = t("about.title");
    dom.aboutSubtitle.textContent = t("about.subtitle");
    dom.aboutAuthorLabel.textContent = t("about.author_label");
    dom.aboutAuthorValue.textContent = t("about.author_name");
    dom.aboutRepoLabel.textContent = t("about.repo_label");
    dom.aboutRepoLink.textContent = t("about.repo_link");
    dom.checkUpdateBtn.textContent = "\ud83d\ude80 " + t("about.check_update");
    dom.settingsTitle.textContent = t("settings.title");
    dom.settingsSub.textContent = t("settings.subtitle");
    dom.settingsTabGeneralBtn.textContent = t("settings.tab.general");
    dom.settingsTabAccountBtn.textContent = t("settings.tab.account");
    dom.settingsTabCloudBtn.textContent = t("settings.tab.cloud");
    dom.settingsLanguageLabel.textContent = t("settings.language_label");
    dom.settingsIdeLabel.textContent = t("settings.ide_label");
    dom.settingsThemeLabel.textContent = t("settings.theme_label");
    dom.settingsThemeHint.textContent = t("settings.theme_hint");
    dom.themeAutoBtn.textContent = t("settings.theme_auto");
    dom.themeLightBtn.textContent = t("settings.theme_light");
    dom.themeDarkBtn.textContent = t("settings.theme_dark");
    dom.settingsAutoUpdateLabel.textContent = t("settings.auto_update_label");
    dom.settingsAutoUpdateHint.textContent = t("settings.auto_update_hint");
    dom.settingsTabVisibilitySectionTitle.textContent = t("settings.tab_visibility_section");
    dom.settingsTabVisibilitySectionHint.textContent = t("settings.tab_visibility_hint");
    dom.settingsQuotaSectionTitle.textContent = t("settings.quota_refresh_section");
    dom.settingsDisableAutoRefreshQuotaLabel.textContent = t("settings.enable_auto_refresh_quota_label");
    dom.settingsDisableAutoRefreshQuotaHint.textContent = t("settings.enable_auto_refresh_quota_hint");
    dom.settingsAutoMarkAbnormalAccountsLabel.textContent = t("settings.auto_mark_abnormal_accounts_label");
    dom.settingsAutoMarkAbnormalAccountsHint.textContent = t("settings.auto_mark_abnormal_accounts_hint");
    dom.settingsAutoDeleteAbnormalAccountsLabel.textContent = t("settings.auto_delete_abnormal_accounts_label");
    dom.settingsAutoDeleteAbnormalAccountsHint.textContent = t("settings.auto_delete_abnormal_accounts_hint");
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
    dom.settingsWebDavSectionTitle.textContent = t("settings.webdav_section");
    dom.settingsWebDavWarning.textContent = t("settings.webdav_warning");
    dom.settingsWebDavEnabledLabel.textContent = t("settings.webdav_enabled_label");
    dom.settingsWebDavEnabledHint.textContent = t("settings.webdav_enabled_hint");
    dom.settingsWebDavUrlLabel.textContent = t("settings.webdav_url_label");
    dom.settingsWebDavUrlHint.textContent = t("settings.webdav_url_hint");
    dom.settingsWebDavRemotePathLabel.textContent = t("settings.webdav_remote_path_label");
    dom.settingsWebDavRemotePathHint.textContent = t("settings.webdav_remote_path_hint");
    dom.settingsWebDavUsernameLabel.textContent = t("settings.webdav_username_label");
    dom.settingsWebDavUsernameHint.textContent = t("settings.webdav_username_hint");
    dom.settingsWebDavPasswordLabel.textContent = t("settings.webdav_password_label");
    dom.settingsWebDavPasswordHint.textContent = t("settings.webdav_password_hint");
    dom.webdavClearPasswordBtn.textContent = t("settings.webdav_clear_password");
    dom.settingsWebDavAutoSyncLabel.textContent = t("settings.webdav_auto_sync_label");
    dom.settingsWebDavAutoSyncHint.textContent = t("settings.webdav_auto_sync_hint", { minutes: state.webdavSyncIntervalMinutes });
    dom.settingsWebDavIntervalLabel.textContent = t("settings.refresh_minutes_label");
    dom.settingsWebDavStatusLabel.textContent = t("settings.webdav_status_label");
    dom.webdavTestBtn.textContent = t("settings.webdav_test");
    dom.webdavUploadBtn.textContent = t("settings.webdav_upload");
    dom.webdavDownloadBtn.textContent = t("settings.webdav_download");
    dom.webdavResetUploadBtn.textContent = t("settings.webdav_reset_upload");
    dom.webdavSyncBtn.textContent = t("settings.webdav_sync");
    dom.cloudAccountTitle.textContent = t("cloud_account.title");
    dom.cloudAccountSubtitle.textContent = t("cloud_account.subtitle");
    dom.cloudAccountDependencyHint.textContent = t("cloud_account.dependency_hint");
    dom.cloudAccountUrlLabel.textContent = t("cloud_account.url_label");
    dom.cloudAccountUrlHint.textContent = t("cloud_account.url_hint");
    dom.cloudAccountPasswordLabel.textContent = t("cloud_account.password_label");
    dom.cloudAccountPasswordHint.textContent = t("cloud_account.password_hint");
    dom.cloudAccountClearPasswordBtn.textContent = t("cloud_account.clear_password");
    dom.cloudAccountAutoDownloadLabel.textContent = t("cloud_account.auto_download_label");
    dom.cloudAccountIntervalLabel.textContent = t("cloud_account.interval_label");
    dom.cloudAccountStatusLabel.textContent = t("cloud_account.status_label");
    dom.cloudAccountDownloadBtn.textContent = t("cloud_account.download_now");
    dom.webdavConflictTitle.textContent = t("dialog.webdav_conflict.title");
    dom.webdavConflictDesc.textContent = t("dialog.webdav_conflict.desc");
    dom.webdavConflictCancelBtn.textContent = t("dialog.common.cancel");
    dom.webdavConflictConfirmBtn.textContent = t("dialog.webdav_conflict.apply");
    dom.backupTitle.textContent = t("dialog.backup.title");
    dom.backupNameLabel.textContent = t("dialog.backup.name_label");
    dom.backupNameInput.placeholder = t("dialog.backup.name_placeholder");
    dom.renameTitle.textContent = t("dialog.rename.title");
    dom.renameNameLabel.textContent = t("dialog.rename.name_label");
    dom.renameNameInput.placeholder = t("dialog.rename.name_placeholder");
    dom.renameCancelBtn.textContent = t("dialog.common.cancel");
    dom.renameConfirmBtn.textContent = t("dialog.common.confirm");
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
    dom.addTabOAuthBtn.textContent = t("dialog.add_account.tab_oauth");
    dom.addTabManualBtn.textContent = t("dialog.add_account.tab_manual");
    dom.addTabCurrentBtn.textContent = t("dialog.add_account.tab_current");
    dom.addTabFileBtn.textContent = t("dialog.add_account.tab_file");
    dom.addPaneOAuthDesc.textContent = t("dialog.add_account.oauth_desc");
    dom.addPaneManualDesc.textContent = t("dialog.add_account.manual_desc");
    dom.addPaneCurrentDesc.textContent = t("dialog.add_account.current_desc");
    dom.addPaneFileDesc.textContent = t("dialog.add_account.file_desc");
    dom.addAccountOAuthBtn.textContent = t("dialog.add_account.oauth_btn");
    dom.oauthMonitorTitle.textContent = t("dialog.add_account.oauth_monitor_title");
    dom.oauthAuthLinkLabel.textContent = t("dialog.add_account.oauth_link_label");
    dom.oauthOpenLinkBtn.textContent = t("dialog.add_account.oauth_open_link");
    dom.oauthCallbackLabel.textContent = t("dialog.add_account.oauth_callback_label");
    dom.oauthCallbackTextarea.placeholder = t("dialog.add_account.oauth_callback_placeholder");
    dom.oauthSubmitCallbackBtn.textContent = t("dialog.add_account.oauth_submit_callback");
    dom.addAccountManualImportBtn.textContent = t("dialog.add_account.manual_btn");
    dom.addAccountImportCurrentBtn.textContent = t("dialog.add_account.current_btn");
    dom.addAccountImportOAuthBtn.textContent = t("dialog.add_account.file_btn");
    dom.addAccountCancelBtn.textContent = t("dialog.common.cancel");
    switchAddAccountTab(state.addAccountTab);
    renderOAuthFlowPanel();
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
    dom.apiTitle.textContent = t("api.title");
    dom.apiSub.textContent = t("api.subtitle");
    dom.apiModelLabel.textContent = t("api.model_label");
    dom.apiPromptLabel.textContent = t("api.prompt_label");
    dom.apiOutputLabel.textContent = t("api.output_label");
    dom.apiSendBtn.textContent = "\ud83d\ude80 " + t("api.send");
    dom.proxyTitle.textContent = "\ud83d\udd27 " + t("proxy.title");
    dom.proxyPortLabel.textContent = t("proxy.port_label");
    dom.proxyPortHint.textContent = t("proxy.port_hint");
    dom.proxyTimeoutLabel.textContent = t("proxy.timeout_label");
    dom.proxyTimeoutHint.textContent = t("proxy.timeout_hint");
    dom.proxyAutoStartLabel.textContent = t("proxy.auto_start_label");
    dom.proxyAutoStartHint.textContent = t("proxy.auto_start_hint");
    dom.proxyAllowLanLabel.textContent = t("proxy.allow_lan_label");
    dom.proxyApiKeyLabel.textContent = t("proxy.api_key_label");
    dom.proxyApiKeyHint.textContent = t("proxy.api_key_hint");
    dom.proxyStealthModeLabel.textContent = "🔄 " + t("proxy.stealth_mode_label");
    if (dom.proxyStealthModeHint) dom.proxyStealthModeHint.textContent = t("proxy.stealth_mode_hint");
    dom.apiSubTabProxyBtn.textContent = t("api.subtab.proxy");
    dom.apiSubTabTestBtn.textContent = t("api.subtab.test");
    dom.proxyDispatchModeLabel.textContent = t("proxy.dispatch_mode_label");
    dom.proxyDispatchModeHint.textContent = t("proxy.dispatch_mode_hint");
    if (dom.proxyFixedAccountLabel) dom.proxyFixedAccountLabel.textContent = t("proxy.fixed_account_label");
    if (dom.proxyFixedAccountHint) dom.proxyFixedAccountHint.textContent = t("proxy.fixed_account_hint");
    Array.from(dom.proxyDispatchModeSelect.options).forEach((opt) => {
      if (opt.value === "round_robin") opt.textContent = t("proxy.dispatch_mode_round_robin");
      if (opt.value === "random") opt.textContent = t("proxy.dispatch_mode_random");
      if (opt.value === "fixed") opt.textContent = t("proxy.dispatch_mode_fixed");
    });
    dom.proxyDispatchModeSelect.value = state.proxyDispatchMode || "round_robin";
    dom.proxyStartBtn.textContent = "\u25b6\ufe0f " + t("proxy.start");
    dom.proxyStopBtn.textContent = "\u23f9\ufe0f " + t("proxy.stop");
    dom.trafficTitle.textContent = t("traffic.title");
    dom.trafficSubtitle.textContent = t("traffic.subtitle");
    dom.trafficAccountFilterLabel.textContent = t("traffic.label.account");
    dom.trafficQuickFilterLabel.textContent = t("traffic.label.filter");
    if (dom.trafficQuickFilterAllOption) dom.trafficQuickFilterAllOption.textContent = t("traffic.filter.all");
    if (dom.trafficQuickFilterErrorOption) dom.trafficQuickFilterErrorOption.textContent = t("traffic.filter.error");
    dom.trafficSearchInput.placeholder = t("traffic.search_placeholder");
    dom.trafficRefreshBtn.textContent = t("traffic.refresh");
    dom.trafficThStatus.textContent = t("traffic.th.status");
    dom.trafficThMethod.textContent = t("traffic.th.method");
    dom.trafficThModel.textContent = t("traffic.th.model");
    dom.trafficThProtocol.textContent = t("traffic.th.protocol");
    dom.trafficThAccount.textContent = t("traffic.th.account");
    dom.trafficThPath.textContent = t("traffic.th.path");
    dom.trafficThTokens.textContent = t("traffic.th.tokens");
    dom.trafficThElapsed.textContent = t("traffic.th.elapsed");
    dom.trafficThCalledAt.textContent = t("traffic.th.called_at");
    dom.trafficPageSizeLabel.textContent = t("traffic.label.page_size");
    dom.trafficPrevBtn.textContent = t("traffic.prev");
    dom.trafficNextBtn.textContent = t("traffic.next");
    dom.tokenTitle.textContent = t("token.title");
    dom.tokenSubtitle.textContent = t("token.subtitle");
    dom.tokenAccountFilterLabel.textContent = t("token.label.account");
    dom.tokenPeriodSelectLabel.textContent = t("token.label.period");
    if (dom.tokenPeriodHourOption) dom.tokenPeriodHourOption.textContent = t("token.period.hour");
    if (dom.tokenPeriodDayOption) dom.tokenPeriodDayOption.textContent = t("token.period.day");
    if (dom.tokenPeriodWeekOption) dom.tokenPeriodWeekOption.textContent = t("token.period.week");
    dom.tokenRefreshBtn.textContent = t("token.refresh");
    dom.tokenCardInputLabel.textContent = t("token.card.input");
    dom.tokenCardActiveLabel.textContent = t("token.card.active_account");
    dom.tokenCardOutputLabel.textContent = t("token.card.output");
    dom.tokenCardTotalLabel.textContent = t("token.card.total");
    dom.tokenTrendTitle.textContent = t("token.trend.title");
    dom.tokenTrendModelBtn.textContent = t("token.trend.model");
    dom.tokenTrendAccountBtn.textContent = t("token.trend.account");
    dom.tokenAccountStatsTitle.textContent = t("token.account_stats.title");
    dom.tokenAccountThAccount.textContent = t("token.account_stats.th.account");
    dom.tokenAccountThCalls.textContent = t("token.account_stats.th.calls");
    dom.tokenAccountThOutput.textContent = t("token.account_stats.th.output");
    dom.tokenAccountThTotal.textContent = t("token.account_stats.th.total");
    dom.tokenModelThModel.textContent = t("token.model_stats.th.model");
    dom.tokenModelThOutput.textContent = t("token.model_stats.th.output");
    dom.tokenModelThTotal.textContent = t("token.model_stats.th.total");
    dom.apiPromptTextarea.placeholder = t("api.prompt_placeholder");
    dom.versionText.textContent = t("about.version_prefix", { version: state.appVersion });
    renderRefreshCountdowns();
    renderIdeOptions();
    switchSettingsSubTab(state.settingsSubTab);
    renderDashboard();
    applyCountText();
    switchApiSubTab(state.apiSubTab);
    renderApiModelOptions();
    renderApiState();
    renderProxyStatus();
    renderProxyFixedAccountOptions();
    renderTrafficAccountOptions();
    renderTrafficLogs();
    renderTokenStats();
    renderImportBusy();
    renderCloudAccountState();
    refreshCustomSelects();
    renderBulkControls();
    renderTabVisibilitySettings();
    switchTab(state.currentTab || document.querySelector(".tab-btn.active")?.getAttribute("data-tab") || "dashboard");
  }

  function formatTokenNumber(v) {
    const n = Number(v);
    if (!Number.isFinite(n) || n < 0) return "-";
    return n.toLocaleString();
  }

  function formatTrendDateTime(sec, withDate = true) {
    const n = Number(sec);
    if (!Number.isFinite(n) || n <= 0) return "-";
    const d = new Date(n * 1000);
    const mm = String(d.getMonth() + 1).padStart(2, "0");
    const dd = String(d.getDate()).padStart(2, "0");
    const hh = String(d.getHours()).padStart(2, "0");
    const mi = String(d.getMinutes()).padStart(2, "0");
    return withDate ? `${mm}-${dd} ${hh}:${mi}` : `${hh}:${mi}`;
  }

  function renderTrafficAccountOptions() {
    if (!dom.trafficAccountFilter || !dom.tokenAccountFilter) return;
    const currentTraffic = String(state.trafficAccountFilter || "all");
    const currentToken = String(state.tokenAccountFilter || "all");
    const allAccounts = Array.isArray(state.accounts) ? state.accounts : [];
    const names = Array.from(new Set(allAccounts.map((x) => String(x?.name || "").trim()).filter(Boolean))).sort((a, b) => a.localeCompare(b));

    dom.trafficAccountFilter.innerHTML = "";
    dom.tokenAccountFilter.innerHTML = "";
    const addOption = (selectEl, value, text) => {
      const opt = document.createElement("option");
      opt.value = value;
      opt.textContent = text;
      selectEl.appendChild(opt);
    };
    addOption(dom.trafficAccountFilter, "all", t("group.all"));
    addOption(dom.tokenAccountFilter, "all", t("group.all"));
    names.forEach((name) => {
      addOption(dom.trafficAccountFilter, name, name);
      addOption(dom.tokenAccountFilter, name, name);
    });
    dom.trafficAccountFilter.value = names.includes(currentTraffic) ? currentTraffic : "all";
    dom.tokenAccountFilter.value = names.includes(currentToken) ? currentToken : "all";
    state.trafficAccountFilter = dom.trafficAccountFilter.value;
    state.tokenAccountFilter = dom.tokenAccountFilter.value;
    refreshCustomSelects();
  }

  function renderTrafficLogs() {
    if (!dom.trafficBody) return;
    const allItems = Array.isArray(state.trafficLogs) ? state.trafficLogs : [];
    const keyword = String(state.trafficKeyword || "").trim().toLowerCase();
    let filtered = allItems;
    if (state.trafficQuickFilter === "error") {
      filtered = filtered.filter((it) => Number(it?.status) >= 400);
    }
    if (keyword) {
      filtered = filtered.filter((it) => {
        const text = [
          it?.account,
          it?.model,
          it?.path,
          it?.method,
          it?.protocol,
          it?.status
        ].map((x) => String(x || "").toLowerCase()).join(" ");
        return text.includes(keyword);
      });
    }

    const pageSize = Math.max(1, Number(state.trafficPageSize || 50));
    const total = filtered.length;
    const totalPages = Math.max(1, Math.ceil(total / pageSize));
    state.trafficPage = Math.max(1, Math.min(totalPages, Number(state.trafficPage || 1)));
    const start = (state.trafficPage - 1) * pageSize;
    const items = filtered.slice(start, start + pageSize);

    if (total <= 0) {
      dom.trafficBody.innerHTML = `<tr><td colspan="9" class="table-empty-cell">${escapeHtml(t("traffic.empty"))}</td></tr>`;
      if (dom.trafficPageInfo) dom.trafficPageInfo.textContent = t("traffic.page_info_empty");
      if (dom.trafficPrevBtn) dom.trafficPrevBtn.disabled = true;
      if (dom.trafficNextBtn) dom.trafficNextBtn.disabled = true;
      return;
    }
    dom.trafficBody.innerHTML = items.map((it) => {
      const status = Number(it?.status);
      const statusText = Number.isFinite(status) && status > 0 ? String(status) : "-";
      const method = String(it?.method || "-").toUpperCase();
      const model = String(it?.model || "-");
      const protocol = String(it?.protocol || "-");
      const account = String(it?.account || "-");
      const path = String(it?.path || "-");
      const tokenValue = Number(it?.totalTokens);
      const tokenText = Number.isFinite(tokenValue) && tokenValue >= 0
        ? formatTokenNumber(tokenValue)
        : "-";
      const elapsed = Number(it?.elapsedMs);
      const elapsedText = Number.isFinite(elapsed) && elapsed >= 0 ? `${elapsed} ms` : "-";
      const calledAtText = String(it?.calledAtText || "-");
      return `
        <tr>
          <td>${escapeHtml(statusText)}</td>
          <td>${escapeHtml(method)}</td>
          <td>${escapeHtml(model)}</td>
          <td>${escapeHtml(protocol)}</td>
          <td title="${escapeHtml(account)}">${escapeHtml(account)}</td>
          <td title="${escapeHtml(path)}">${escapeHtml(path)}</td>
          <td>${escapeHtml(tokenText)}</td>
          <td>${escapeHtml(elapsedText)}</td>
          <td>${escapeHtml(calledAtText)}</td>
        </tr>
      `;
    }).join("");
    if (dom.trafficPageInfo) dom.trafficPageInfo.textContent =
      t("traffic.page_info", { page: state.trafficPage, totalPages, total });
    if (dom.trafficPrevBtn) dom.trafficPrevBtn.disabled = state.trafficPage <= 1;
    if (dom.trafficNextBtn) dom.trafficNextBtn.disabled = state.trafficPage >= totalPages;
  }

  function renderTokenStats() {
    const stats = state.tokenStats || {};
    if (dom.tokenInputValue) dom.tokenInputValue.textContent = formatTokenNumber(stats.inputTokens);
    if (dom.tokenOutputValue) dom.tokenOutputValue.textContent = formatTokenNumber(stats.outputTokens);
    if (dom.tokenTotalValue) dom.tokenTotalValue.textContent = formatTokenNumber(stats.totalTokens);
    if (dom.tokenActiveAccount) {
      const name = String(stats.activeAccount || "").trim();
      dom.tokenActiveAccount.textContent = name || "-";
    }
    if (!dom.tokenModelBody) return;
    const models = Array.isArray(stats.models) ? stats.models : [];
    if (models.length <= 0) {
      dom.tokenModelBody.innerHTML = `<tr><td colspan="3" class="table-empty-cell">${escapeHtml(t("token.model_stats.empty"))}</td></tr>`;
      return;
    }
    dom.tokenModelBody.innerHTML = models.map((it) => `
      <tr>
        <td title="${escapeHtml(String(it?.name || "-"))}">${escapeHtml(String(it?.name || "-"))}</td>
        <td>${escapeHtml(formatTokenNumber(it?.outputTokens))}</td>
        <td>${escapeHtml(formatTokenNumber(it?.totalTokens))}</td>
      </tr>
    `).join("");

    if (dom.tokenTrendModelBtn && dom.tokenTrendAccountBtn) {
      dom.tokenTrendModelBtn.classList.toggle("active", state.tokenTrendMode === "model");
      dom.tokenTrendAccountBtn.classList.toggle("active", state.tokenTrendMode === "account");
    }
    if (dom.tokenTrendList) {
      const trend = stats.trend || {};
      const labels = Array.isArray(trend.labels) ? trend.labels.map((x) => String(x || "")) : [];
      const list = state.tokenTrendMode === "account"
        ? (Array.isArray(trend.byAccount) ? trend.byAccount : [])
        : (Array.isArray(trend.byModel) ? trend.byModel : []);
      if (!list.length) {
        dom.tokenTrendList.innerHTML = `<div class="table-empty-cell">${escapeHtml(t("token.trend.empty"))}</div>`;
      } else {
        const periodText = state.tokenPeriod === "hour"
          ? t("token.trend.period.hour")
          : (state.tokenPeriod === "week" ? t("token.trend.period.week") : t("token.trend.period.day"));
        const rangeStart = Number(trend.rangeStartSec);
        const rangeEnd = Number(trend.rangeEndSec);
        const hasRange = Number.isFinite(rangeStart) && Number.isFinite(rangeEnd) && rangeStart > 0 && rangeEnd > 0;
        const axisStart = hasRange
          ? formatTrendDateTime(rangeStart, state.tokenPeriod !== "hour")
          : (labels.length > 0 ? labels[0] : "-");
        const axisEnd = hasRange
          ? formatTrendDateTime(rangeEnd, state.tokenPeriod !== "hour")
          : (labels.length > 0 ? labels[labels.length - 1] : "-");
        dom.tokenTrendList.innerHTML = list.map((row) => {
          const vals = Array.isArray(row?.values) ? row.values.map((x) => Number(x) || 0) : [];
          const maxVal = vals.reduce((a, b) => Math.max(a, b), 0);
          const bars = vals.map((v, idx) => {
            const h = maxVal > 0 ? Math.max(2, Math.round((v / maxVal) * 28)) : 2;
            const label = labels[idx] || `#${idx + 1}`;
            return `<span style="height:${h}px" title="${escapeHtml(label)} | ${escapeHtml(formatTokenNumber(v))} token"></span>`;
          }).join("");
          return `
            <div class="token-trend-row">
              <div class="token-trend-head">
                <span>${escapeHtml(String(row?.name || "-"))}</span>
                <strong>${escapeHtml(formatTokenNumber(row?.totalTokens))}</strong>
              </div>
              <div class="token-trend-axis">
                <span>${escapeHtml(periodText)}</span>
                <span>${escapeHtml(axisStart)} -> ${escapeHtml(axisEnd)}</span>
              </div>
              <div class="token-spark" style="--col-count:${Math.max(1, vals.length)}">${bars}</div>
              <div class="token-trend-ticks">
                <span>${escapeHtml(axisStart)}</span>
                <span>${escapeHtml(axisEnd)}</span>
              </div>
            </div>
          `;
        }).join("");
      }
    }
    if (dom.tokenAccountBody) {
      const accounts = Array.isArray(stats.accounts) ? stats.accounts : [];
      if (!accounts.length) {
        dom.tokenAccountBody.innerHTML = `<tr><td colspan="4" class="table-empty-cell">${escapeHtml(t("token.account_stats.empty"))}</td></tr>`;
      } else {
        dom.tokenAccountBody.innerHTML = accounts.map((it) => `
          <tr>
            <td title="${escapeHtml(String(it?.name || "-"))}">${escapeHtml(String(it?.name || "-"))}</td>
            <td>${escapeHtml(formatTokenNumber(it?.calls))}</td>
            <td>${escapeHtml(formatTokenNumber(it?.outputTokens))}</td>
            <td>${escapeHtml(formatTokenNumber(it?.totalTokens))}</td>
          </tr>
        `).join("");
      }
    }
  }

  function requestTrafficLogs(force = false) {
    const now = Date.now();
    if (!force && (now - Number(state.lastTrafficFetchAt || 0)) < 1200) {
      return;
    }
    state.lastTrafficFetchAt = now;
    post("get_traffic_logs", {
      account: String(state.trafficAccountFilter || "all"),
      limit: 1000
    });
  }

  function requestAccountsList(force = false) {
    const now = Date.now();
    if (!force && (now - Number(state.lastAccountsFetchAt || 0)) < 1200) {
      return;
    }
    state.lastAccountsFetchAt = now;
    post("list_accounts");
  }

  function requestTokenStats(force = false) {
    const now = Date.now();
    if (!force && (now - Number(state.lastTokenFetchAt || 0)) < 1200) {
      return;
    }
    state.lastTokenFetchAt = now;
    post("get_token_stats", {
      account: String(state.tokenAccountFilter || "all"),
      period: String(state.tokenPeriod || "day")
    });
  }

  function startAutoRefreshLoop() {
    if (state.autoRefreshLoopTimer) {
      clearInterval(state.autoRefreshLoopTimer);
      state.autoRefreshLoopTimer = null;
    }
    state.autoRefreshLoopTimer = setInterval(() => {
      if (document.visibilityState !== "visible") return;
      const activeTab = document.querySelector(".tab-btn.active")?.getAttribute("data-tab") || "";
      const now = Date.now();
      if (activeTab === "dashboard" || activeTab === "accounts") {
        if ((now - Number(state.lastAccountsFetchAt || 0)) >= Number(state.accountsAutoRefreshMs || 8000)) {
          requestAccountsList();
        }
      } else if (activeTab === "traffic") {
        if ((now - Number(state.lastTrafficFetchAt || 0)) >= Number(state.trafficAutoRefreshMs || 6000)) {
          requestTrafficLogs();
        }
      } else if (activeTab === "token") {
        if ((now - Number(state.lastTokenFetchAt || 0)) >= Number(state.tokenAutoRefreshMs || 8000)) {
          requestTokenStats();
        }
      }
    }, 1000);
  }

  function applyCountText() {
    const total = state.filteredAccounts.length;
    dom.countText.textContent = total > 0 ? t("count.format", { total }) : t("count.empty");
  }

  function renderApiState() {
    const hasAnyAccount = Array.isArray(state.accounts) && state.accounts.length > 0;
    dom.apiSendBtn.disabled = state.apiRequestBusy || !hasAnyAccount;
    dom.apiSendBtn.classList.toggle("loading", state.apiRequestBusy);
  }

  function renderProxyStatus() {
    const running = !!state.proxyRunning;
    const portText = String(dom.proxyPortInput.value || "8045");
    dom.proxyStatusText.textContent = running
      ? t("proxy.status_running", { port: portText })
      : t("proxy.status_stopped");
    dom.proxyStatusDot.classList.toggle("running", running);
    dom.proxyStartBtn.disabled = running;
    dom.proxyStopBtn.disabled = !running;
    if (!state.proxyApiKeyEditing) {
      dom.proxyApiKeyInput.value = state.proxyApiKey || "";
    }
    dom.proxyAllowLanHint.textContent = state.proxyAllowLan
      ? t("proxy.allow_lan_hint_on")
      : t("proxy.allow_lan_hint_off");
  }

  function renderProxyFixedAccountOptions() {
    if (!dom.proxyFixedAccountSelect) return;
    dom.proxyFixedAccountSelect.innerHTML = "";
    const currentOpt = document.createElement("option");
    currentOpt.value = "current::";
    currentOpt.textContent = t("proxy.fixed_account_current");
    dom.proxyFixedAccountSelect.appendChild(currentOpt);
    dom.proxyFixedAccountSelect.value = "current::";
    dom.proxyFixedAccountSelect.disabled = true;
  }

  function renderApiModelOptions() {
    if (!dom.apiModelList) return;
    const models = state.apiModels.length > 0 ? state.apiModels : FALLBACK_API_MODELS;
    dom.apiModelList.innerHTML = "";
    if (dom.apiModelSelect) {
      dom.apiModelSelect.innerHTML = "";
    }
    for (const id of models) {
      const option = document.createElement("option");
      option.value = id;
      dom.apiModelList.appendChild(option);
      if (dom.apiModelSelect) {
        const selectOption = document.createElement("option");
        selectOption.value = id;
        selectOption.textContent = id;
        dom.apiModelSelect.appendChild(selectOption);
      }
    }
    if (dom.apiModelSelect && models.length > 0 && !dom.apiModelSelect.value) {
      const preferred = models.find((x) => x === "gpt-5.3-codex") || models[0];
      dom.apiModelSelect.value = preferred;
    }
  }

  function switchApiSubTab(tab) {
    state.apiSubTab = tab === "proxy" ? "proxy" : "test";
    dom.apiSubTabTestBtn.classList.toggle("active", state.apiSubTab === "test");
    dom.apiSubTabProxyBtn.classList.toggle("active", state.apiSubTab === "proxy");
    dom.apiPaneTest.classList.toggle("active", state.apiSubTab === "test");
    dom.apiPaneProxy.classList.toggle("active", state.apiSubTab === "proxy");
  }

  function mapStatusMessage(msg) {
    const code = String(msg?.code || "");
    if (!code) return String(msg?.message || "");
    const key = `status_code.${code}`;
    if (state.i18n[key]) {
      if (code === "restart_failed") {
        return t(key, { ide: getIdeDisplayName(state.currentIdeExe) });
      }
      if ([
        "import_auth_batch_done",
        "import_auth_batch_partial",
        "import_auth_batch_failed",
        "cloud_account_batch_done",
        "cloud_account_batch_partial",
        "cloud_account_batch_failed"
      ].includes(code)) {
        const successRaw = msg?.success;
        const totalRaw = msg?.total;
        const failedRaw = msg?.failed;
        const skippedRaw = msg?.skipped;
        const toNumber = (val) => {
          const n = Number(val);
          return Number.isFinite(n) ? n : null;
        };
        let successNum = toNumber(successRaw);
        let totalNum = toNumber(totalRaw);
        let failedNum = toNumber(failedRaw);
        let skippedNum = toNumber(skippedRaw);

        if (successNum === null || totalNum === null) {
          const raw = String(msg?.message || "");
          const countMatch = raw.match(/(\d+)\s*\/\s*(\d+)/);
          if (countMatch) {
            if (successNum === null) successNum = Number(countMatch[1]);
            if (totalNum === null) totalNum = Number(countMatch[2]);
          }
        }
        if (failedNum === null && successNum !== null && totalNum !== null) {
          failedNum = Math.max(0, totalNum - successNum);
        }
        if (skippedNum === null) {
          skippedNum = 0;
        }

        const success = successNum === null ? "0" : String(successNum);
        const total = totalNum === null ? "0" : String(totalNum);
        const failed = failedNum === null ? "0" : String(failedNum);
        const skipped = skippedNum === null ? "0" : String(skippedNum);
        const lastError = String(msg?.lastError || "").trim();
        const text = t(key, { success, failed, total, skipped });
        if (lastError && !["import_auth_batch_done", "cloud_account_batch_done"].includes(code)) {
          return `${text} · ${t("status_code.last_error_prefix")}: ${lastError}`;
        }
        return text;
      }
      if (["cloud_account_progress", "import_auth_batch_progress", "quota_refresh_progress"].includes(code)) {
        const vars = getProgressVars(msg?.current, msg?.total);
        return vars ? t(key, vars) : t(key);
      }
      if (["batch_delete_done", "batch_delete_partial", "batch_delete_failed"].includes(code)) {
        const successRaw = msg?.success;
        const totalRaw = msg?.total;
        const success = Number.isFinite(Number(successRaw)) ? String(Number(successRaw)) : "";
        const total = Number.isFinite(Number(totalRaw)) ? String(Number(totalRaw)) : "";
        const lastError = String(msg?.lastError || "").trim();
        let text = t(key);
        if (success && total) {
          text = `${text} ${success}/${total}`;
        }
        if (lastError && code !== "batch_delete_done") {
          text = `${text} · ${t("status_code.last_error_prefix")}: ${lastError}`;
        }
        return text;
      }
      return t(key);
    }
    return String(msg?.message || code);
  }

  function getProgressVars(current, total) {
    const currentNum = Math.max(0, Math.trunc(Number(current)));
    const totalNum = Math.max(0, Math.trunc(Number(total)));
    if (!Number.isFinite(currentNum) || !Number.isFinite(totalNum) || currentNum <= 0 || totalNum <= 0) {
      return null;
    }
    return {
      current: String(currentNum),
      total: String(totalNum),
      progress: t("progress.count", { current: String(currentNum), total: String(totalNum) })
    };
  }

  function getRefreshProgressText(scope = state.refreshProgressScope || "") {
    const vars = getProgressVars(state.refreshProgressCurrent, state.refreshProgressTotal);
    if (scope === "batch") {
      return vars ? t("bulk.refresh_progress", vars) : t("bulk.refresh_running");
    }
    return vars ? t("toolbar.refresh_progress", vars) : t("toolbar.refresh_running");
  }

  function isCloudQuotaRefreshRunning() {
    return !!state.enableAutoRefreshQuota
      && !!state.cloudAccountDownloadRunning
      && state.refreshProgressScope === "cloud";
  }

  function getImportProgressText() {
    const vars = getProgressVars(state.importProgressCurrent, state.importProgressTotal);
    return vars ? t("toolbar.import_progress", vars) : t("toolbar.import_running");
  }

  function getCloudProgressStatusText() {
    const vars = getProgressVars(state.cloudAccountProgressCurrent, state.cloudAccountProgressTotal);
    return vars ? t("cloud_account.status_running_progress", vars) : t("cloud_account.status_running");
  }

  function getCloudProgressButtonText() {
    const vars = getProgressVars(state.cloudAccountProgressCurrent, state.cloudAccountProgressTotal);
    return vars ? t("cloud_account.download_now_progress", vars) : t("cloud_account.download_now_running");
  }

  function renderPrimaryActionButtons() {
    const showRefreshProgress = state.refreshMode === "all" || isCloudQuotaRefreshRunning();
    if (dom.addCurrentBtn) {
      dom.addCurrentBtn.textContent = state.importMode
        ? "⏳ " + getImportProgressText()
        : "➕ " + t("toolbar.add_current");
    }
    if (dom.refreshBtn) {
      dom.refreshBtn.textContent = "🔄 " + (showRefreshProgress
        ? getRefreshProgressText(state.refreshProgressScope || "all")
        : t("toolbar.refresh"));
    }
  }

  function isOAuthTerminalCode(code) {
    return [
      "oauth_success",
      "oauth_timeout",
      "oauth_error",
      "browser_open_failed",
      "no_auth_code",
      "token_request_failed",
      "invalid_token_response"
    ].includes(String(code || ""));
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
    if (!isBusy) {
      state.refreshProgressCurrent = 0;
      state.refreshProgressTotal = 0;
      state.refreshProgressScope = "";
    }
    const isImportBusy = !!state.importMode;
    const isBlocked = isImportBusy || state.cleanAbnormalBusy;
    dom.refreshBtn.disabled = isBusy || isBlocked;
    dom.refreshBtn.classList.toggle("loading", mode === "all" || isImportBusy);

    if (isBusy) {
      const timeoutMs = mode === "account" ? 120000 : 0;
      if (timeoutMs > 0) {
        state.refreshBusyTimer = setTimeout(() => {
          setRefreshBusy("", "");
        }, timeoutMs);
      }
    }

    renderPrimaryActionButtons();
    renderAccounts();
    renderImportBusy();
    renderBulkControls();
  }

  function setImportBusy(mode = "") {
    state.importMode = mode;
    if (!mode) {
      state.importProgressCurrent = 0;
      state.importProgressTotal = 0;
    }
    renderImportBusy();
    setRefreshBusy(state.refreshMode, state.refreshTargetKey);
  }

  function normalizeSelectedKeys() {
    const validSet = new Set(state.accounts.map((x) => makeAccountKey(x.name, x.group)));
    state.selectedAccountKeys = state.selectedAccountKeys.filter((key) => validSet.has(key));
  }

  function getSelectedItems() {
    const selectedSet = new Set(state.selectedAccountKeys);
    return state.accounts.filter((item) => selectedSet.has(makeAccountKey(item.name, item.group)));
  }

  function getAbnormalAccounts() {
    const list = Array.isArray(state.accounts) ? state.accounts : [];
    return list.filter((item) => item && item.abnormal);
  }

  function renderCleanAbnormalButton() {
    if (!dom.cleanAbnormalBtn) return;
    const busy = state.cleanAbnormalBusy;
    const blocked = !!state.importMode
      || !!state.refreshMode
      || state.bulkMode === "refresh"
      || state.bulkMode === "delete";
    dom.cleanAbnormalBtn.textContent = "\ud83e\uddf9 " + t("toolbar.clean_abnormal");
    dom.cleanAbnormalBtn.disabled = busy || blocked;
    dom.cleanAbnormalBtn.classList.toggle("loading", busy);
    dom.cleanAbnormalBtn.dataset.abnormalCount = String(getAbnormalAccounts().length);
  }

  function renderBulkControls() {
    const selectedCount = state.selectedAccountKeys.length;
    const busy = state.bulkMode === "refresh" || state.bulkMode === "delete" || state.cleanAbnormalBusy;
    const cloudQuotaRefreshBusy = isCloudQuotaRefreshRunning();
    dom.bulkSelectedText.textContent = t("bulk.selected", { count: selectedCount });
    dom.multiSelectToggleBtn.textContent = state.multiSelectMode ? t("bulk.mode_on") : t("bulk.mode_off");
    dom.bulkRefreshBtn.textContent = "🔄 " + ((state.bulkMode === "refresh" || cloudQuotaRefreshBusy)
      ? getRefreshProgressText(cloudQuotaRefreshBusy ? "cloud" : "batch")
      : t("bulk.refresh"));
    dom.bulkDeleteBtn.textContent = "🗑️ " + t("bulk.delete");

    const hasSelection = selectedCount > 0;
    dom.bulkRefreshBtn.disabled = !state.multiSelectMode || !hasSelection || busy || !!state.importMode || !!state.refreshMode || cloudQuotaRefreshBusy;
    dom.bulkDeleteBtn.disabled = !state.multiSelectMode || !hasSelection || busy || !!state.importMode;
    dom.bulkRefreshBtn.classList.toggle("loading", state.bulkMode === "refresh" || cloudQuotaRefreshBusy);
    dom.bulkDeleteBtn.classList.toggle("loading", state.bulkMode === "delete");

    const visibleKeys = state.filteredAccounts.map((x) => makeAccountKey(x.name, x.group));
    const visibleSelected = visibleKeys.filter((x) => state.selectedAccountKeys.includes(x)).length;
    dom.selectAllCheckbox.checked = state.multiSelectMode && visibleKeys.length > 0 && visibleSelected === visibleKeys.length;
    dom.selectAllCheckbox.indeterminate = state.multiSelectMode && visibleSelected > 0 && visibleSelected < visibleKeys.length;
    dom.selectAllCheckbox.disabled = !state.multiSelectMode || visibleKeys.length === 0 || busy;
    dom.selectAllCheckbox.title = t("bulk.select_all");
    dom.selectAllCheckbox.style.visibility = state.multiSelectMode ? "visible" : "hidden";
    dom.bulkRow.classList.toggle("disabled", !state.multiSelectMode);
    renderCleanAbnormalButton();
  }

  function setBulkBusy(mode = "") {
    state.bulkMode = mode;
    if (mode !== "refresh" && state.refreshProgressScope === "batch") {
      state.refreshProgressCurrent = 0;
      state.refreshProgressTotal = 0;
      state.refreshProgressScope = "";
    }
    renderBulkControls();
    renderImportBusy();
    renderAccounts();
  }

  function renderImportBusy() {
    const isCurrentBusy = state.importMode === "current";
    const isOAuthBusy = state.importMode === "oauth";
    const isManualBusy = state.importMode === "manual";
    const disableAny = isCurrentBusy || isOAuthBusy || isManualBusy;
    const cloudQuotaRefreshBusy = isCloudQuotaRefreshRunning();

    renderPrimaryActionButtons();

    dom.addCurrentBtn.disabled = disableAny;
    dom.addCurrentBtn.classList.toggle("loading", disableAny);

    dom.refreshBtn.disabled = (state.refreshMode === "all" || state.refreshMode === "account") || disableAny || cloudQuotaRefreshBusy;
    dom.refreshBtn.classList.toggle("loading", (state.refreshMode === "all") || disableAny || cloudQuotaRefreshBusy);

    dom.addAccountImportCurrentBtn.disabled = disableAny;
    dom.addAccountImportCurrentBtn.classList.toggle("loading", isCurrentBusy);

    dom.addAccountImportOAuthBtn.disabled = disableAny;
    dom.addAccountImportOAuthBtn.classList.toggle("loading", isOAuthBusy);

    dom.addAccountOAuthBtn.disabled = disableAny;
    dom.addAccountOAuthBtn.classList.toggle("loading", isOAuthBusy);

    dom.addAccountManualImportBtn.disabled = disableAny;
    dom.addAccountManualImportBtn.classList.toggle("loading", isManualBusy);
    dom.addAccountCancelBtn.disabled = isOAuthBusy;
    dom.addTabOAuthBtn.disabled = isOAuthBusy;
    dom.addTabManualBtn.disabled = isOAuthBusy;
    dom.addTabCurrentBtn.disabled = isOAuthBusy;
    dom.addTabFileBtn.disabled = isOAuthBusy;
    renderOAuthFlowPanel();
  }

  function formatRemain(v) {
    return formatPercentValue(v);
  }

  function normalizePlanType(planType) {
    const value = String(planType || "").toLowerCase();
    if (value === "free" || value === "plus" || value === "team" || value === "pro") {
      return value;
    }
    return "";
  }

  function normalizeGroupValue(group) {
    const value = String(group || "").toLowerCase();
    if (value === "personal" || value === "business" ||
      value === "free" || value === "plus" || value === "team" || value === "pro") {
      return value;
    }
    return "personal";
  }

  function formatPlanTypeLabel(planType) {
    const normalized = normalizePlanType(planType);
    if (normalized) {
      if (normalized === "free") return "Free";
      if (normalized === "plus") return "Plus";
      if (normalized === "team") return "Team";
      if (normalized === "pro") return "Pro";
    }
    const raw = String(planType || "");
    return raw ? `${t("tag.plan_unknown")}: ${raw}` : "";
  }

  function formatGroupLabel(group) {
    const normalized = normalizeGroupValue(group);
    if (normalized === "personal") return t("tag.group_personal");
    if (normalized === "business") return t("tag.group_business");
    if (normalized === "free" || normalized === "plus" || normalized === "team" || normalized === "pro") {
      return formatPlanTypeLabel(normalized);
    }
    return t("tag.group_personal");
  }

  function formatUsageErrorMessage(usageError) {
    const raw = String(usageError || "");
    if (raw.startsWith("unknown_plan_type:")) {
      const value = raw.slice("unknown_plan_type:".length) || "(empty)";
      return `${t("tag.plan_unknown")}：${value}`;
    }
    return "";
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
    if (n === null) return "-";
    if (n > 0 && n < 0.1) return "<0.1%";
    const rounded = Math.round(n * 10) / 10;
    return `${rounded.toFixed(1).replace(/\.0$/, "")}%`;
  }

  function getPlanMetrics(item) {
    const plan = normalizePlanType(item?.planType);
    const q5 = toPercentNumber(item?.quota5hRemainingPercent);
    const q7Raw = toPercentNumber(item?.quota7dRemainingPercent);
    const q7 = plan === "free" ? (q7Raw ?? q5) : q7Raw;
    return { plan, q5, q7 };
  }

  function renderDashboard() {
    const accounts = Array.isArray(state.accounts) ? state.accounts : [];
    const usable = accounts.filter((x) => x && x.usageOk);
    const metrics = usable.map((x) => ({ name: String(x.name || ""), ...getPlanMetrics(x) }));
    const fiveHourMetrics = metrics.filter((x) => x.plan !== "free" && x.q5 !== null);
    const sevenDayMetrics = metrics.filter((x) => x.q7 !== null);
    const sum5 = fiveHourMetrics.reduce((acc, x) => acc + x.q5, 0);
    const sum7 = sevenDayMetrics.reduce((acc, x) => acc + x.q7, 0);
    const avg5 = fiveHourMetrics.length > 0 ? sum5 / fiveHourMetrics.length : null;
    const avg7 = sevenDayMetrics.length > 0 ? sum7 / sevenDayMetrics.length : null;
    const lowCount = usable.filter((x) => {
      const m = getPlanMetrics(x);
      const value = m.plan === "free" ? m.q7 : m.q5;
      return value !== null && value < 20;
    }).length;
    const lowAccounts = usable
      .map((x) => {
        const m = getPlanMetrics(x);
        return { name: String(x.name || ""), lowValue: m.plan === "free" ? m.q7 : m.q5 };
      })
      .filter((x) => x.lowValue !== null && x.lowValue < 20)
      .sort((a, b) => a.lowValue - b.lowValue);

    dom.dashTotalValue.textContent = String(accounts.length);
    dom.dashAvg5Value.textContent = formatPercentValue(avg5);
    dom.dashAvg7Value.textContent = formatPercentValue(avg7);
    dom.dashLowValue.textContent = String(lowCount);
    if (lowAccounts.length > 0) {
      dom.dashLowList.innerHTML = lowAccounts.slice(0, 8).map((x) => `
        <div class="dashboard-low-item">
          <span class="dashboard-low-name">${escapeHtml(x.name || "-")}</span>
          <span class="dashboard-low-quota">${escapeHtml(formatPercentValue(x.lowValue))}</span>
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
      dom.dashCurrent5Row.style.display = "";
      dom.dashCurrent5Progress.style.display = "";
      return;
    }

    const currentMetrics = getPlanMetrics(current);
    const q5 = currentMetrics.q5;
    const q7 = currentMetrics.q7;
    const isCurrentFree = currentMetrics.plan === "free";
    dom.dashCurrentName.textContent = current.name || t("dashboard.current_name_empty");
    dom.dashCurrent5Row.style.display = isCurrentFree ? "none" : "";
    dom.dashCurrent5Progress.style.display = isCurrentFree ? "none" : "";
    dom.dashCurrent5Value.textContent = isCurrentFree ? "-" : formatPercentValue(q5);
    dom.dashCurrent7Value.textContent = formatPercentValue(q7);
    dom.dashCurrent5Bar.style.width = `${isCurrentFree || q5 === null ? 0 : q5}%`;
    dom.dashCurrent7Bar.style.width = `${q7 === null ? 0 : q7}%`;
  }

  function formatHoursFromSeconds(v) {
    if (!Number.isFinite(Number(v)) || Number(v) < 0) return "-";
    return `${(Number(v) / 3600).toFixed(1)}h`;
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

  function clampWebDavSyncMinutes(v, fallback = 15) {
    const n = Number(v);
    if (!Number.isFinite(n)) return fallback;
    if (n < 1) return 1;
    if (n > 1440) return 1440;
    return Math.round(n);
  }

  function formatSyncDateTime(value) {
    const text = String(value || "").trim();
    return text || "-";
  }

  function closeWebDavConflictModal() {
    if (!dom.webdavConflictModal) return;
    dom.webdavConflictModal.classList.remove("show");
  }

  function openWebDavConflictModal() {
    if (!dom.webdavConflictModal) return;
    dom.webdavConflictModal.classList.add("show");
  }

  function renderWebDavConflictModal() {
    if (!dom.webdavConflictList) return;
    const conflicts = Array.isArray(state.webdavConflicts) ? state.webdavConflicts : [];
    dom.webdavConflictList.innerHTML = conflicts.map((item, index) => {
      const winner = item.winner === "local" ? "local" : "remote";
      return `
        <div class="webdav-conflict-item" data-webdav-conflict-index="${index}">
          <div class="webdav-conflict-head">
            <span>${escapeHtml(item.account || "-")}</span>
            <span class="webdav-conflict-meta">${escapeHtml((item.localGroup || "personal") + " ↔ " + (item.remoteGroup || "personal"))}</span>
          </div>
          <div class="webdav-conflict-options">
            <button class="webdav-choice ${winner === "local" ? "active" : ""}" data-webdav-choice="local" data-webdav-conflict-index="${index}">
              <div class="webdav-choice-title">${escapeHtml(t("dialog.webdav_conflict.keep_local"))}</div>
              <div class="webdav-choice-note">${escapeHtml(t("dialog.webdav_conflict.local_note"))}</div>
              <div class="webdav-choice-note">${escapeHtml(item.localUpdatedAt || "-")}</div>
            </button>
            <button class="webdav-choice ${winner === "remote" ? "active" : ""}" data-webdav-choice="remote" data-webdav-conflict-index="${index}">
              <div class="webdav-choice-title">${escapeHtml(t("dialog.webdav_conflict.keep_remote"))}</div>
              <div class="webdav-choice-note">${escapeHtml(t("dialog.webdav_conflict.remote_note"))}</div>
              <div class="webdav-choice-note">${escapeHtml(item.remoteUpdatedAt || "-")}</div>
            </button>
          </div>
        </div>
      `;
    }).join("");
  }

  function syncInputValue(input, value) {
    if (!input) return;
    const next = String(value ?? "");
    if (document.activeElement === input) return;
    if (String(input.value || "") !== next) {
      input.value = next;
    }
  }

  function renderWebDavState() {
    if (!dom.webdavEnabledToggle) return;
    dom.webdavEnabledToggle.checked = !!state.webdavEnabled;
    dom.webdavAutoSyncToggle.checked = !!state.webdavAutoSync;
    syncInputValue(dom.webdavUrlInput, state.webdavUrl || "");
    syncInputValue(dom.webdavRemotePathInput, state.webdavRemotePath || "/CodexAccountSwitch");
    syncInputValue(dom.webdavUsernameInput, state.webdavUsername || "");
    syncInputValue(dom.webdavSyncIntervalInput, clampWebDavSyncMinutes(state.webdavSyncIntervalMinutes, 15));
    dom.webdavPasswordState.textContent = state.webdavPasswordConfigured
      ? t("settings.webdav_password_saved")
      : t("settings.webdav_password_missing");
    dom.webdavStatusDot.classList.toggle("running", !!state.webdavSyncRunning);
    const statusText = state.webdavSyncRunning
      ? t("settings.webdav_running")
      : (String(state.webdavLastSyncStatus || "").trim() || t("settings.webdav_idle"));
    dom.webdavStatusText.textContent = statusText;
    dom.webdavLastSyncText.textContent = state.webdavLastSyncAt
      ? t("settings.webdav_last_sync", { time: formatSyncDateTime(state.webdavLastSyncAt) })
      : t("settings.webdav_last_sync_empty");
    const nextSyncText = state.webdavEnabled && state.webdavAutoSync
      ? t("settings.webdav_next_sync", { time: formatCountdown(state.webdavSyncRemainingSec) })
      : t("settings.webdav_next_sync_disabled");
    dom.webdavNextSyncText.textContent = nextSyncText;
    dom.webdavPasswordInput.placeholder = state.webdavPasswordConfigured
      ? t("settings.webdav_password_saved")
      : t("settings.webdav_password_missing");
    dom.webdavSyncIntervalInput.placeholder = t("settings.webdav_interval_hint");
    const disableSyncFields = !state.webdavEnabled;
    [dom.webdavUrlInput, dom.webdavRemotePathInput, dom.webdavUsernameInput, dom.webdavPasswordInput,
      dom.webdavAutoSyncToggle, dom.webdavSyncIntervalInput, dom.webdavClearPasswordBtn,
      dom.webdavTestBtn, dom.webdavUploadBtn, dom.webdavDownloadBtn, dom.webdavResetUploadBtn,
      dom.webdavSyncBtn].forEach((el) => {
      if (!el) return;
      if (el === dom.webdavClearPasswordBtn) {
        el.disabled = disableSyncFields || !state.webdavPasswordConfigured || state.webdavSyncRunning;
      } else if (el === dom.webdavAutoSyncToggle || el === dom.webdavSyncIntervalInput || el === dom.webdavUrlInput || el === dom.webdavRemotePathInput || el === dom.webdavUsernameInput || el === dom.webdavPasswordInput) {
        el.disabled = disableSyncFields || state.webdavSyncRunning;
      } else {
        el.disabled = disableSyncFields || state.webdavSyncRunning;
      }
    });
  }

  function renderCloudAccountState() {
    if (!dom.cloudAccountAutoDownloadToggle) return;
    dom.cloudAccountAutoDownloadToggle.checked = !!state.cloudAccountAutoDownload;
    syncInputValue(dom.cloudAccountUrlInput, state.cloudAccountUrl || "");
    syncInputValue(dom.cloudAccountIntervalInput, clampWebDavSyncMinutes(state.cloudAccountIntervalMinutes, 60));
    dom.cloudAccountPasswordState.textContent = state.cloudAccountPasswordConfigured
      ? t("cloud_account.password_saved")
      : t("cloud_account.password_missing");
    dom.cloudAccountStatusDot.classList.toggle("running", !!state.cloudAccountDownloadRunning);
    dom.cloudAccountStatusText.textContent = state.cloudAccountDownloadRunning
      ? getCloudProgressStatusText()
      : (String(state.cloudAccountLastDownloadStatus || "").trim() || t("cloud_account.status_idle"));
    dom.cloudAccountLastDownloadText.textContent = state.cloudAccountLastDownloadAt
      ? t("cloud_account.last_download", { time: formatSyncDateTime(state.cloudAccountLastDownloadAt) })
      : t("cloud_account.last_download_empty");
    dom.cloudAccountNextDownloadText.textContent = state.cloudAccountAutoDownload
      ? t("cloud_account.next_download", { time: formatCountdown(state.cloudAccountRemainingSec) })
      : t("cloud_account.next_download_disabled");
    dom.cloudAccountAutoDownloadHint.textContent = t("cloud_account.auto_download_hint", {
      minutes: clampWebDavSyncMinutes(state.cloudAccountIntervalMinutes, 60)
    });
    dom.cloudAccountPasswordInput.placeholder = state.cloudAccountPasswordConfigured
      ? t("cloud_account.password_saved")
      : t("cloud_account.password_missing");
    dom.cloudAccountIntervalInput.placeholder = t("cloud_account.interval_hint");
    dom.cloudAccountDownloadBtn.textContent = state.cloudAccountDownloadRunning
      ? getCloudProgressButtonText()
      : t("cloud_account.download_now");
    [
      dom.cloudAccountUrlInput,
      dom.cloudAccountPasswordInput,
      dom.cloudAccountAutoDownloadToggle,
      dom.cloudAccountIntervalInput,
      dom.cloudAccountDownloadBtn
    ].forEach((el) => {
      if (!el) return;
      el.disabled = !!state.cloudAccountDownloadRunning;
    });
    if (dom.cloudAccountClearPasswordBtn) {
      dom.cloudAccountClearPasswordBtn.disabled = !state.cloudAccountPasswordConfigured || !!state.cloudAccountDownloadRunning;
    }
  }

  function parseEnableAutoRefreshQuota(msg, fallback = true) {
    const hasEnable = !!(msg && Object.prototype.hasOwnProperty.call(msg, "enableAutoRefreshQuota"));
    if (hasEnable) {
      return msg.enableAutoRefreshQuota === true || msg.enableAutoRefreshQuota === "true";
    }
    const hasDisable = !!(msg && Object.prototype.hasOwnProperty.call(msg, "disableAutoRefreshQuota"));
    if (hasDisable) {
      return !(msg.disableAutoRefreshQuota === true || msg.disableAutoRefreshQuota === "true");
    }
    return fallback;
  }

  function buildConfigPayload() {
    const cloudAccountPassword = String(dom.cloudAccountPasswordInput?.value || "").trim();
    const webdavPassword = String(dom.webdavPasswordInput?.value || "").trim();
    return {
      language: state.currentLanguage,
      ideExe: state.currentIdeExe,
      tabVisibility: normalizeTabVisibility(state.tabVisibility),
      autoUpdate: state.autoUpdate,
      enableAutoRefreshQuota: state.enableAutoRefreshQuota,
      autoMarkAbnormalAccounts: state.autoMarkAbnormalAccounts,
      autoDeleteAbnormalAccounts: state.autoDeleteAbnormalAccounts,
      autoRefreshCurrent: state.autoRefreshCurrent,
      lowQuotaAutoPrompt: state.lowQuotaAutoPrompt,
      autoRefreshAllMinutes: state.autoRefreshAllMinutes,
      autoRefreshCurrentMinutes: state.autoRefreshCurrentMinutes,
      theme: state.themeMode,
      proxyPort: Math.max(1, Math.min(65535, Number(dom.proxyPortInput.value || 8045))),
      proxyTimeoutSec: Math.max(30, Math.min(7200, Number(dom.proxyTimeoutInput.value || 120))),
      proxyAllowLan: !!state.proxyAllowLan,
      proxyAutoStart: !!state.proxyAutoStart,
      proxyApiKey: String(dom.proxyApiKeyInput.value || "").trim(),
      proxyStealthMode: !!state.proxyStealthMode,
      proxyDispatchMode: String(state.proxyDispatchMode || "round_robin"),
      proxyFixedAccount: "",
      proxyFixedGroup: "personal",
      cloudAccountUrl: String(dom.cloudAccountUrlInput?.value || "").trim(),
      cloudAccountPassword,
      cloudAccountPasswordClear: !cloudAccountPassword && !!state.cloudAccountPasswordClear,
      cloudAccountAutoDownload: !!state.cloudAccountAutoDownload,
      cloudAccountIntervalMinutes: clampWebDavSyncMinutes(state.cloudAccountIntervalMinutes, 60),
      webdavEnabled: !!state.webdavEnabled,
      webdavAutoSync: !!state.webdavAutoSync,
      webdavSyncIntervalMinutes: clampWebDavSyncMinutes(state.webdavSyncIntervalMinutes, 15),
      webdavUrl: String(dom.webdavUrlInput?.value || "").trim(),
      webdavRemotePath: String(dom.webdavRemotePathInput?.value || "/CodexAccountSwitch").trim(),
      webdavUsername: String(dom.webdavUsernameInput?.value || "").trim(),
      webdavPassword,
      webdavPasswordClear: !webdavPassword && !!state.webdavPasswordClear
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
    const msgTabVisibility = normalizeTabVisibility(msg.tabVisibility);
    const pendingTabVisibility = normalizeTabVisibility(pending.tabVisibility);
    const msgLanguage = msg.language || "zh-CN";
    const msgIdeExe = msg.ideExe || "Code.exe";
    const msgAutoUpdate = msg.autoUpdate !== false && msg.autoUpdate !== "false";
    const msgEnableAutoRefreshQuota = parseEnableAutoRefreshQuota(msg, true);
    const msgAutoMarkAbnormalAccounts = msg.autoMarkAbnormalAccounts !== false && msg.autoMarkAbnormalAccounts !== "false";
    const msgAutoDeleteAbnormalAccounts = msg.autoDeleteAbnormalAccounts === true || msg.autoDeleteAbnormalAccounts === "true";
    const msgAutoRefreshCurrent = msg.autoRefreshCurrent !== false && msg.autoRefreshCurrent !== "false";
    const msgLowQuotaAutoPrompt = msg.lowQuotaAutoPrompt !== false && msg.lowQuotaAutoPrompt !== "false";
    const msgAutoRefreshAllMinutes = clampRefreshMinutes(msg.autoRefreshAllMinutes, 15);
    const msgAutoRefreshCurrentMinutes = clampRefreshMinutes(msg.autoRefreshCurrentMinutes, 5);
    const msgTheme = normalizeThemeMode(msg.theme || "auto");
    const msgProxyAllowLan = msg.proxyAllowLan === true || msg.proxyAllowLan === "true";
    const msgProxyAutoStart = msg.proxyAutoStart === true || msg.proxyAutoStart === "true";
    const msgProxyApiKey = String(msg.proxyApiKey || "").trim();
    const msgProxyStealthMode = msg.proxyStealthMode === true || msg.proxyStealthMode === "true";
    const msgProxyDispatchMode = String(msg.proxyDispatchMode || "round_robin");
    const msgProxyFixedAccount = String(msg.proxyFixedAccount || "");
    const msgProxyFixedGroup = String(msg.proxyFixedGroup || "personal");
    const msgCloudAccountUrl = String(msg.cloudAccountUrl || "").trim();
    const msgCloudAccountAutoDownload = msg.cloudAccountAutoDownload === true || msg.cloudAccountAutoDownload === "true";
    const msgCloudAccountIntervalMinutes = clampWebDavSyncMinutes(msg.cloudAccountIntervalMinutes, 60);
    const msgCloudAccountPasswordConfigured = msg.cloudAccountPasswordConfigured === true || msg.cloudAccountPasswordConfigured === "true";
    const msgWebdavEnabled = msg.webdavEnabled === true || msg.webdavEnabled === "true";
    const msgWebdavAutoSync = msg.webdavAutoSync !== false && msg.webdavAutoSync !== "false";
    const msgWebdavSyncIntervalMinutes = clampWebDavSyncMinutes(msg.webdavSyncIntervalMinutes, 15);
    const msgWebdavUrl = String(msg.webdavUrl || "").trim();
    const msgWebdavRemotePath = String(msg.webdavRemotePath || "/CodexAccountSwitch").trim();
    const msgWebdavUsername = String(msg.webdavUsername || "").trim();
    const msgWebdavPasswordConfigured = msg.webdavPasswordConfigured === true || msg.webdavPasswordConfigured === "true";
    return msgLanguage === pending.language
      && msgIdeExe === pending.ideExe
      && TOP_LEVEL_TABS.every((tab) => msgTabVisibility[tab.key] === pendingTabVisibility[tab.key])
      && msgAutoUpdate === pending.autoUpdate
      && msgEnableAutoRefreshQuota === !!pending.enableAutoRefreshQuota
      && msgAutoMarkAbnormalAccounts === !!pending.autoMarkAbnormalAccounts
      && msgAutoDeleteAbnormalAccounts === !!pending.autoDeleteAbnormalAccounts
      && msgAutoRefreshCurrent === pending.autoRefreshCurrent
      && msgLowQuotaAutoPrompt === pending.lowQuotaAutoPrompt
      && msgAutoRefreshAllMinutes === pending.autoRefreshAllMinutes
      && msgAutoRefreshCurrentMinutes === pending.autoRefreshCurrentMinutes
      && msgTheme === pending.theme
      && msgProxyAllowLan === pending.proxyAllowLan
      && msgProxyAutoStart === pending.proxyAutoStart
      && msgProxyApiKey === String(pending.proxyApiKey || "").trim()
      && msgProxyStealthMode === !!pending.proxyStealthMode
      && msgProxyDispatchMode === String(pending.proxyDispatchMode || "round_robin")
      && msgProxyFixedAccount === String(pending.proxyFixedAccount || "")
      && msgProxyFixedGroup === String(pending.proxyFixedGroup || "personal")
      && msgCloudAccountUrl === String(pending.cloudAccountUrl || "").trim()
      && msgCloudAccountAutoDownload === !!pending.cloudAccountAutoDownload
      && msgCloudAccountIntervalMinutes === clampWebDavSyncMinutes(pending.cloudAccountIntervalMinutes, 60)
      && msgCloudAccountPasswordConfigured === (String(pending.cloudAccountPassword || "").trim() ? true : (!!state.cloudAccountPasswordConfigured && !pending.cloudAccountPasswordClear))
      && msgWebdavEnabled === !!pending.webdavEnabled
      && msgWebdavAutoSync === !!pending.webdavAutoSync
      && msgWebdavSyncIntervalMinutes === clampWebDavSyncMinutes(pending.webdavSyncIntervalMinutes, 15)
      && msgWebdavUrl === String(pending.webdavUrl || "").trim()
      && msgWebdavRemotePath === String(pending.webdavRemotePath || "/CodexAccountSwitch").trim()
      && msgWebdavUsername === String(pending.webdavUsername || "").trim()
      && msgWebdavPasswordConfigured === (String(pending.webdavPassword || "").trim() ? true : (!!state.webdavPasswordConfigured && !pending.webdavPasswordClear));
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
    try { localStorage.setItem('cas_theme', state.themeMode || 'auto'); } catch(e) {}
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
    if (!state.enableAutoRefreshQuota) {
      dom.settingsAllRefreshCountdown.textContent = `${t("settings.countdown_prefix")}${t("refresh.disabled")}`;
      dom.settingsCurrentRefreshCountdown.textContent = `${t("settings.countdown_prefix")}${t("refresh.disabled")}`;
      return;
    }
    dom.settingsAllRefreshCountdown.textContent = `${t("settings.countdown_prefix")}${formatCountdown(state.allRefreshRemainSec)}`;
    dom.settingsCurrentRefreshCountdown.textContent = state.autoRefreshCurrent
      ? `${t("settings.countdown_prefix")}${formatCountdown(state.currentRefreshRemainSec)}`
      : `${t("settings.countdown_prefix")}${t("refresh.disabled")}`;
  }

  function renderAccounts() {
    normalizeSelectedKeys();

    if (!state.filteredAccounts.length) {
      dom.accountsBody.innerHTML = `<tr><td colspan="5" class="table-empty-cell">${escapeHtml(t("accounts.empty"))}</td></tr>`;
      renderBulkControls();
      return;
    }

    const shortName = (name) => {
      const v = String(name || "");
      return v.length > 14 ? `${v.slice(0, 14)}...` : v;
    };

    const bulkBusy = state.bulkMode === "refresh" || state.bulkMode === "delete";
    const cloudQuotaRefreshBusy = isCloudQuotaRefreshRunning();

    dom.accountsBody.innerHTML = state.filteredAccounts.map((item) => {
      const accountKey = makeAccountKey(item.name, item.group);
      const isThisRefreshing = state.refreshMode === "account" && state.refreshTargetKey === accountKey;
      const disableRefreshAction = state.refreshMode === "all" || isThisRefreshing || !!state.importMode || bulkBusy || cloudQuotaRefreshBusy;
      const disableRowAction = bulkBusy;
      const normalizedPlanType = normalizePlanType(item.planType);
      const normalizedGroup = normalizeGroupValue(item.group);
      const planLabel = formatPlanTypeLabel(item.planType);
      const planClass = normalizedPlanType || "unknown";
      const showGroupTag = item.abnormal
        || normalizedGroup === "personal"
        || normalizedGroup === "business"
        || !normalizedPlanType
        || normalizedGroup !== normalizedPlanType;
      const groupTagLabel = item.abnormal ? t("tag.abnormal") : formatGroupLabel(normalizedGroup);
      const groupTagClass = item.abnormal ? "abnormal" : `group ${normalizedGroup}`;
      const isFreePlan = normalizedPlanType === "free";
      const freeQ7Value = Number(item.quota7dRemainingPercent) >= 0
        ? item.quota7dRemainingPercent
        : item.quota5hRemainingPercent;
      const freeR7Value = Number(item.quota7dResetAfterSeconds) >= 0
        ? item.quota7dResetAfterSeconds
        : item.quota5hResetAfterSeconds;
      const usageErrorText = formatUsageErrorMessage(item.usageError);
      const quotaErrorText = usageErrorText
        ? usageErrorText
        : t("quota.placeholder");
      const checked = state.selectedAccountKeys.includes(accountKey);
      return `
      <tr>
        <td class="select-col">
          <input class="row-select ${state.multiSelectMode ? "is-visible" : "is-hidden"}" type="checkbox" data-select-account="1" data-name="${escapeHtml(item.name)}" data-group="${escapeHtml(item.group || "personal")}" ${checked ? "checked" : ""} ${!state.multiSelectMode || bulkBusy ? "disabled" : ""} />
        </td>
        <td>
          <div class="account-cell" title="${escapeHtml(item.name)}">
            <span class="account-name">${escapeHtml(shortName(item.name))}</span>
            ${item.isCurrent ? `<span class="tag current">${escapeHtml(t("tag.current"))}</span>` : ""}
            ${showGroupTag ? `<span class="tag ${escapeHtml(groupTagClass)}" title="${escapeHtml(item.abnormalReason || formatGroupLabel(normalizedGroup))}">${escapeHtml(groupTagLabel)}</span>` : ""}
            ${planLabel ? `<span class="tag plan ${escapeHtml(planClass)}">${escapeHtml(planLabel)}</span>` : ""}
          </div>
        </td>
        <td>
          <div class="quota-box">
            <span class="quota-name">Codex</span>
            ${item.usageOk
          ? escapeHtml(
            isFreePlan
              ? t("quota.format_free", {
                q7: formatRemain(freeQ7Value),
                r7: formatHoursFromSeconds(freeR7Value)
              })
              : t("quota.format", {
                q5: formatRemain(item.quota5hRemainingPercent),
                q7: formatRemain(item.quota7dRemainingPercent),
                r5: formatHoursFromSeconds(item.quota5hResetAfterSeconds),
                r7: formatHoursFromSeconds(item.quota7dResetAfterSeconds)
              })
          )
          : escapeHtml(quotaErrorText)}
          </div>
        </td>
        <td>${escapeHtml(item.updatedAt || "-")}</td>
        <td class="actions-col">
          <div class="actions">
            <button class="btn-action switch" data-action="switch" data-name="${escapeHtml(item.name)}" data-group="${escapeHtml(item.group || "personal")}" title="${escapeHtml(t("action.switch_title"))}" ${disableRowAction ? "disabled" : ""}>${escapeHtml(t("action.switch"))}</button>
            <button class="btn-action rename" data-action="rename" data-name="${escapeHtml(item.name)}" data-group="${escapeHtml(item.group || "personal")}" title="${escapeHtml(t("action.rename_title"))}" ${disableRowAction ? "disabled" : ""}>${escapeHtml(t("action.rename"))}</button>
            <button class="btn-action refresh ${isThisRefreshing ? "loading" : ""}" data-action="refresh" data-name="${escapeHtml(item.name)}" data-group="${escapeHtml(item.group || "personal")}" title="${escapeHtml(t("action.refresh_title"))}" ${disableRefreshAction ? "disabled" : ""}>${escapeHtml(t("action.refresh"))}</button>
            <button class="btn-action delete" data-action="delete" data-name="${escapeHtml(item.name)}" data-group="${escapeHtml(item.group || "personal")}" title="${escapeHtml(t("action.delete_title"))}" ${disableRowAction ? "disabled" : ""}>${escapeHtml(t("action.delete"))}</button>
          </div>
        </td>
      </tr>
    `;
    }).join("");

    renderBulkControls();

    if (dom.accountsWrap) {
      dom.accountsWrap.classList.toggle("mode-multiselect", state.multiSelectMode);
    }
  }

  function applySearch() {
    const q = dom.searchInput.value.trim().toLowerCase();
    let list = [...state.accounts];
    if (state.groupFilter !== "all") list = list.filter((x) => normalizePlanType(x.planType) === state.groupFilter);
    state.filteredAccounts = !q ? list : list.filter((x) => String(x.name || "").toLowerCase().includes(q));
    applyCountText();
    renderAccounts();
  }

  function bindEvents() {
    document.addEventListener("contextmenu", (e) => e.preventDefault());
    document.addEventListener("dragstart", (e) => e.preventDefault());
    document.addEventListener("selectstart", (e) => {
      const target = e.target;
      const allow =
        target instanceof HTMLInputElement ||
        target instanceof HTMLTextAreaElement ||
        (target instanceof HTMLElement && target.isContentEditable) ||
        target === dom.logEl;
      if (!allow) {
        e.preventDefault();
      }
    });
    document.addEventListener("keydown", (e) => {
      if (!(e.ctrlKey || e.metaKey) || e.altKey || e.shiftKey) return;
      if (String(e.key).toLowerCase() !== "a") return;
      const active = document.activeElement;
      const inInput =
        active instanceof HTMLInputElement ||
        active instanceof HTMLTextAreaElement ||
        (active instanceof HTMLElement && active.isContentEditable);
      if (inInput) {
        if (active instanceof HTMLInputElement || active instanceof HTMLTextAreaElement) {
          active.select();
        } else {
          const selection = window.getSelection();
          const range = document.createRange();
          range.selectNodeContents(active);
          selection.removeAllRanges();
          selection.addRange(range);
        }
        e.preventDefault();
        e.stopPropagation();
        return;
      }
      e.preventDefault();
    }, true);
    document.addEventListener("focusout", (e) => {
      const target = e.target;
      const editable =
        target instanceof HTMLInputElement ||
        target instanceof HTMLTextAreaElement ||
        (target instanceof HTMLElement && target.isContentEditable);
      if (!editable) return;
      if (target instanceof HTMLInputElement || target instanceof HTMLTextAreaElement) {
        try {
          target.setSelectionRange(0, 0);
        } catch (_) {
        }
      }
      const selection = window.getSelection();
      if (selection && selection.rangeCount > 0) {
        selection.removeAllRanges();
      }
    }, true);

    document.querySelectorAll(".tab-btn").forEach((btn) => {
      btn.addEventListener("click", () => switchTab(btn.getAttribute("data-tab") || "accounts"));
    });
    document.querySelectorAll("[data-settings-tab]").forEach((btn) => {
      btn.addEventListener("click", () => {
        switchSettingsSubTab(btn.getAttribute("data-settings-tab") || "general");
      });
    });
    document.querySelectorAll("[data-api-subtab]").forEach((btn) => {
      btn.addEventListener("click", () => {
        switchApiSubTab(btn.getAttribute("data-api-subtab") || "test");
      });
    });

    dom.searchInput.addEventListener("input", applySearch);
    dom.dashboardSwitchBtn.addEventListener("click", () => switchTab("accounts"));
    dom.apiSendBtn.addEventListener("click", () => {
      if (state.apiRequestBusy) return;
      const model = String(dom.apiModelInput.value || "").trim();
      const content = String(dom.apiPromptTextarea.value || "").trim();
      const hasAnyAccount = Array.isArray(state.accounts) && state.accounts.length > 0;
      if (!hasAnyAccount) {
        showToast(t("status_code.api_no_selected_account"), "warning");
        return;
      }
      if (!model || !content) {
        showToast(t("status_code.api_invalid_input"), "warning");
        return;
      }
      state.apiRequestBusy = true;
      renderApiState();
      dom.apiOutputTextarea.value = "";
      post("send_api_request", {
        model,
        content
      });
    });
    dom.proxyStartBtn.addEventListener("click", () => {
      const port = Math.max(1, Math.min(65535, Number(dom.proxyPortInput.value || 8045)));
      const timeoutSec = Math.max(30, Math.min(7200, Number(dom.proxyTimeoutInput.value || 120)));
      post("start_proxy_service", {
        port,
        timeoutSec,
        allowLan: !!state.proxyAllowLan,
        dispatchMode: String(state.proxyDispatchMode || "round_robin"),
        fixedAccount: "",
        fixedGroup: "personal"
      });
    });
    dom.proxyStopBtn.addEventListener("click", () => {
      state.proxyAutoStart = false;
      dom.proxyAutoStartToggle.checked = false;
      post("stop_proxy_service");
    });
    if (dom.trafficRefreshBtn) {
      dom.trafficRefreshBtn.addEventListener("click", () => requestTrafficLogs(true));
    }
    if (dom.trafficAccountFilter) {
      dom.trafficAccountFilter.addEventListener("change", () => {
        state.trafficAccountFilter = String(dom.trafficAccountFilter.value || "all");
        requestTrafficLogs(true);
      });
    }
    if (dom.trafficQuickFilter) {
      dom.trafficQuickFilter.addEventListener("change", () => {
        state.trafficQuickFilter = String(dom.trafficQuickFilter.value || "all");
        state.trafficPage = 1;
        renderTrafficLogs();
      });
    }
    if (dom.trafficSearchInput) {
      dom.trafficSearchInput.addEventListener("input", () => {
        state.trafficKeyword = String(dom.trafficSearchInput.value || "");
        state.trafficPage = 1;
        renderTrafficLogs();
      });
    }
    if (dom.trafficPageSizeSelect) {
      dom.trafficPageSizeSelect.addEventListener("change", () => {
        state.trafficPageSize = Number(dom.trafficPageSizeSelect.value || 50);
        state.trafficPage = 1;
        renderTrafficLogs();
      });
    }
    if (dom.trafficPrevBtn) {
      dom.trafficPrevBtn.addEventListener("click", () => {
        state.trafficPage = Math.max(1, Number(state.trafficPage || 1) - 1);
        renderTrafficLogs();
      });
    }
    if (dom.trafficNextBtn) {
      dom.trafficNextBtn.addEventListener("click", () => {
        state.trafficPage = Number(state.trafficPage || 1) + 1;
        renderTrafficLogs();
      });
    }
    if (dom.tokenRefreshBtn) {
      dom.tokenRefreshBtn.addEventListener("click", () => requestTokenStats(true));
    }
    if (dom.tokenAccountFilter) {
      dom.tokenAccountFilter.addEventListener("change", () => {
        state.tokenAccountFilter = String(dom.tokenAccountFilter.value || "all");
        requestTokenStats(true);
      });
    }
    if (dom.tokenPeriodSelect) {
      dom.tokenPeriodSelect.addEventListener("change", () => {
        state.tokenPeriod = String(dom.tokenPeriodSelect.value || "day");
        requestTokenStats(true);
      });
    }
    if (dom.tokenTrendModelBtn) {
      dom.tokenTrendModelBtn.addEventListener("click", () => {
        state.tokenTrendMode = "model";
        renderTokenStats();
      });
    }
    if (dom.tokenTrendAccountBtn) {
      dom.tokenTrendAccountBtn.addEventListener("click", () => {
        state.tokenTrendMode = "account";
        renderTokenStats();
      });
    }
    dom.proxyPortInput.addEventListener("change", () => {
      renderProxyStatus();
      queueSaveConfig();
    });
    dom.proxyTimeoutInput.addEventListener("change", queueSaveConfig);
    dom.proxyDispatchModeSelect.addEventListener("change", () => {
      state.proxyDispatchMode = String(dom.proxyDispatchModeSelect.value || "round_robin");
      renderProxyFixedAccountOptions();
      queueSaveConfig();
    });
    dom.proxyStealthModeToggle.addEventListener("change", () => {
      state.proxyStealthMode = dom.proxyStealthModeToggle.checked;
      queueSaveConfig();
    });
    dom.proxyApiKeyInput.addEventListener("focus", () => {
      state.proxyApiKeyEditing = true;
    });
    dom.proxyApiKeyInput.addEventListener("change", () => {
      dom.proxyApiKeyInput.value = String(dom.proxyApiKeyInput.value || "").trim();
      state.proxyApiKey = dom.proxyApiKeyInput.value;
      state.proxyApiKeyEditing = false;
      queueSaveConfig();
    });
    dom.proxyApiKeyInput.addEventListener("blur", () => {
      state.proxyApiKeyEditing = false;
      state.proxyApiKey = String(dom.proxyApiKeyInput.value || "").trim();
      renderProxyStatus();
    });
    dom.proxyAutoStartToggle.addEventListener("change", () => {
      state.proxyAutoStart = dom.proxyAutoStartToggle.checked;
      queueSaveConfig();
    });
    dom.proxyAllowLanToggle.addEventListener("change", () => {
      state.proxyAllowLan = dom.proxyAllowLanToggle.checked;
      renderProxyStatus();
      queueSaveConfig();
    });
    [dom.cloudAccountUrlInput].forEach((el) => {
      el.addEventListener("change", queueSaveConfig);
      el.addEventListener("blur", queueSaveConfig);
    });
    dom.cloudAccountPasswordInput.addEventListener("change", () => {
      state.cloudAccountPasswordClear = false;
      queueSaveConfig();
    });
    dom.cloudAccountClearPasswordBtn.addEventListener("click", () => {
      state.cloudAccountPasswordClear = true;
      dom.cloudAccountPasswordInput.value = "";
      state.cloudAccountPasswordConfigured = false;
      renderCloudAccountState();
      queueSaveConfig();
    });
    dom.cloudAccountAutoDownloadToggle.addEventListener("change", () => {
      state.cloudAccountAutoDownload = dom.cloudAccountAutoDownloadToggle.checked;
      renderCloudAccountState();
      queueSaveConfig();
    });
    const handleCloudAccountMinutesChanged = () => {
      state.cloudAccountIntervalMinutes = clampWebDavSyncMinutes(dom.cloudAccountIntervalInput.value, state.cloudAccountIntervalMinutes || 60);
      renderCloudAccountState();
      queueSaveConfig();
    };
    dom.cloudAccountIntervalInput.addEventListener("input", handleCloudAccountMinutesChanged);
    dom.cloudAccountIntervalInput.addEventListener("change", handleCloudAccountMinutesChanged);
    dom.cloudAccountDownloadBtn.addEventListener("click", () => {
      if (state.cloudAccountDownloadRunning) return;
      state.cloudAccountDownloadRunning = true;
      state.cloudAccountProgressCurrent = 0;
      state.cloudAccountProgressTotal = 0;
      if (state.enableAutoRefreshQuota) {
        state.refreshProgressCurrent = 0;
        state.refreshProgressTotal = 0;
        state.refreshProgressScope = "cloud";
        renderPrimaryActionButtons();
        renderBulkControls();
        renderAccounts();
      }
      renderCloudAccountState();
      post("download_latest_cloud_account");
    });
    dom.webdavEnabledToggle.addEventListener("change", () => {
      state.webdavEnabled = dom.webdavEnabledToggle.checked;
      refreshSettingsOptions();
      queueSaveConfig();
    });
    dom.webdavAutoSyncToggle.addEventListener("change", () => {
      state.webdavAutoSync = dom.webdavAutoSyncToggle.checked;
      refreshSettingsOptions();
      queueSaveConfig();
    });
    const handleWebDavMinutesChanged = () => {
      state.webdavSyncIntervalMinutes = clampWebDavSyncMinutes(dom.webdavSyncIntervalInput.value, state.webdavSyncIntervalMinutes || 15);
      refreshSettingsOptions();
      queueSaveConfig();
    };
    dom.webdavSyncIntervalInput.addEventListener("input", handleWebDavMinutesChanged);
    dom.webdavSyncIntervalInput.addEventListener("change", handleWebDavMinutesChanged);
    [dom.webdavUrlInput, dom.webdavRemotePathInput, dom.webdavUsernameInput].forEach((el) => {
      el.addEventListener("change", queueSaveConfig);
      el.addEventListener("blur", queueSaveConfig);
    });
    dom.webdavPasswordInput.addEventListener("change", () => {
      state.webdavPasswordClear = false;
      queueSaveConfig();
    });
    dom.webdavClearPasswordBtn.addEventListener("click", () => {
      state.webdavPasswordClear = true;
      dom.webdavPasswordInput.value = "";
      state.webdavPasswordConfigured = false;
      renderWebDavState();
      queueSaveConfig();
    });
    dom.webdavTestBtn.addEventListener("click", () => post("test_webdav_connection"));
    dom.webdavUploadBtn.addEventListener("click", () => post("run_webdav_sync", { mode: "upload" }));
    dom.webdavDownloadBtn.addEventListener("click", () => post("run_webdav_sync", { mode: "download" }));
    dom.webdavResetUploadBtn.addEventListener("click", () => post("run_webdav_sync", { mode: "reset_upload" }));
    dom.webdavSyncBtn.addEventListener("click", () => post("run_webdav_sync", { mode: "bidirectional" }));
    dom.webdavConflictList.addEventListener("click", (e) => {
      const btn = e.target.closest("[data-webdav-choice]");
      if (!btn) return;
      const index = Number(btn.getAttribute("data-webdav-conflict-index"));
      const winner = String(btn.getAttribute("data-webdav-choice") || "remote");
      if (!Number.isInteger(index) || !state.webdavConflicts[index]) return;
      state.webdavConflicts[index].winner = winner === "local" ? "local" : "remote";
      renderWebDavConflictModal();
    });
    dom.webdavConflictCancelBtn.addEventListener("click", () => {
      closeWebDavConflictModal();
      post("resolve_webdav_conflicts", { cancel: true });
    });
    dom.webdavConflictConfirmBtn.addEventListener("click", () => {
      const decisions = (state.webdavConflicts || []).map((item) => ({
        account: item.account,
        winner: item.winner === "local" ? "local" : "remote"
      }));
      post("resolve_webdav_conflicts", { decisions });
      closeWebDavConflictModal();
    });
    dom.webdavConflictModal.addEventListener("click", (e) => {
      if (e.target === dom.webdavConflictModal) {
        closeWebDavConflictModal();
      }
    });
    dom.addCurrentBtn.addEventListener("click", () => {
      openAddAccountModal();
    });

    dom.multiSelectToggleBtn.addEventListener("click", () => {
      if (state.bulkMode || state.importMode || state.refreshMode) return;
      state.multiSelectMode = !state.multiSelectMode;
      if (!state.multiSelectMode) {
        state.selectedAccountKeys = [];
      }
      renderAccounts();
    });

    dom.selectAllCheckbox.addEventListener("change", () => {
      if (!state.multiSelectMode || state.bulkMode) return;
      const visibleKeys = state.filteredAccounts.map((x) => makeAccountKey(x.name, x.group));
      const selectedSet = new Set(state.selectedAccountKeys);
      if (dom.selectAllCheckbox.checked) {
        visibleKeys.forEach((key) => selectedSet.add(key));
      } else {
        visibleKeys.forEach((key) => selectedSet.delete(key));
      }
      state.selectedAccountKeys = Array.from(selectedSet);
      renderAccounts();
    });

    dom.bulkRefreshBtn.addEventListener("click", () => {
      if (state.bulkMode || state.importMode || state.refreshMode) return;
      const items = getSelectedItems().map((x) => ({ account: x.name, group: x.group || "personal" }));
      if (!items.length) {
        showToast(t("status_code.batch_empty"), "warning");
        return;
      }
      setRefreshBusy("all");
      setBulkBusy("refresh");
      post("refresh_accounts_batch", { items });
    });

    dom.bulkDeleteBtn.addEventListener("click", () => {
      if (state.bulkMode || state.importMode) return;
      const items = getSelectedItems().map((x) => ({ account: x.name, group: x.group || "personal" }));
      if (!items.length) {
        showToast(t("status_code.batch_empty"), "warning");
        return;
      }
      openConfirm({
        title: t("bulk.delete_confirm_title"),
        message: t("bulk.delete_confirm_message", { count: items.length }),
        onConfirm: () => {
          setBulkBusy("delete");
          post("delete_accounts_batch", { items });
        }
      });
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
        const ide = getIdeDisplayName(state.currentIdeExe);
        const message = state.proxyStealthMode
          ? t("confirm.switch_proxy_mode", { name, ide })
          : t("confirm.switch_restart_ide", { name, ide });
        openConfirm({
          title: t("dialog.confirm.title"),
          message,
          onConfirm: () => {
            post("switch_account", {
              account: name,
              group,
              language: state.currentLanguage,
              ideExe: state.currentIdeExe
            });
          }
        });
      } else if (action === "rename") {
        state.renameTargetName = name;
        state.renameTargetGroup = group;
        dom.renameNameInput.value = name;
        dom.renameModal.classList.add("show");
        setTimeout(() => {
          dom.renameNameInput.focus();
          dom.renameNameInput.select();
        }, 10);
      } else if (action === "refresh") {
        if (state.refreshMode || state.bulkMode) return;
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

    dom.accountsBody.addEventListener("change", (e) => {
      const target = e.target;
      if (!(target instanceof HTMLInputElement)) return;
      if (target.getAttribute("data-select-account") !== "1") return;
      if (!state.multiSelectMode || state.bulkMode) return;
      const name = target.getAttribute("data-name") || "";
      const group = target.getAttribute("data-group") || "personal";
      if (!name) return;
      const key = makeAccountKey(name, group);
      const selectedSet = new Set(state.selectedAccountKeys);
      if (target.checked) {
        selectedSet.add(key);
      } else {
        selectedSet.delete(key);
      }
      state.selectedAccountKeys = Array.from(selectedSet);
      renderBulkControls();
    });

    dom.autoUpdateToggle.addEventListener("change", () => {
      state.autoUpdate = dom.autoUpdateToggle.checked;
      refreshSettingsOptions();
      queueSaveConfig();
    });

    dom.settingsTabVisibilityList.addEventListener("click", (e) => {
      const target = e.target instanceof HTMLElement
        ? e.target.closest("[data-tab-visibility-card]")
        : null;
      if (!(target instanceof HTMLButtonElement)) return;
      const key = String(target.getAttribute("data-tab-visibility-card") || "");
      if (!key) return;
      if (key === "settings") {
        return;
      }
      state.tabVisibility = normalizeTabVisibility({
        ...state.tabVisibility,
        [key]: !isTabVisible(key)
      });
      refreshSettingsOptions();
      if (!isTabVisible(state.currentTab)) {
        switchTab(state.currentTab || "dashboard");
      } else {
        renderToolbarActionsForTab(state.currentTab);
      }
      queueSaveConfig();
    });

    dom.disableAutoRefreshQuotaToggle.addEventListener("change", () => {
      state.enableAutoRefreshQuota = dom.disableAutoRefreshQuotaToggle.checked;
      refreshSettingsOptions();
      renderRefreshCountdowns();
      queueSaveConfig();
    });
    dom.autoMarkAbnormalAccountsToggle.addEventListener("change", () => {
      state.autoMarkAbnormalAccounts = dom.autoMarkAbnormalAccountsToggle.checked;
      refreshSettingsOptions();
      queueSaveConfig();
    });
    dom.autoDeleteAbnormalAccountsToggle.addEventListener("change", () => {
      state.autoDeleteAbnormalAccounts = dom.autoDeleteAbnormalAccountsToggle.checked;
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

    dom.renameCancelBtn.addEventListener("click", () => {
      dom.renameModal.classList.remove("show");
      state.renameTargetName = "";
      state.renameTargetGroup = "personal";
    });
    dom.renameModal.addEventListener("click", (e) => {
      if (e.target === dom.renameModal) {
        dom.renameModal.classList.remove("show");
        state.renameTargetName = "";
        state.renameTargetGroup = "personal";
      }
    });
    dom.renameConfirmBtn.addEventListener("click", () => {
      const newName = dom.renameNameInput.value.trim();
      if (!newName) {
        dom.renameNameInput.focus();
        return;
      }
      if (newName.length > 32) {
        showToast(t("status_code.name_too_long"), "warning");
        return;
      }
      if (!state.renameTargetName) {
        dom.renameModal.classList.remove("show");
        return;
      }
      post("rename_account", {
        account: state.renameTargetName,
        group: state.renameTargetGroup || "personal",
        newName
      });
      dom.renameModal.classList.remove("show");
      state.renameTargetName = "";
      state.renameTargetGroup = "personal";
    });

    dom.addAccountCancelBtn.addEventListener("click", () => closeAddAccountModal(true));

    dom.addAccountModal.addEventListener("click", (e) => {
      if (e.target === dom.addAccountModal) return;
    });

    document.querySelectorAll("[data-add-tab]").forEach((btn) => {
      btn.addEventListener("click", () => {
        switchAddAccountTab(btn.getAttribute("data-add-tab") || "oauth");
      });
    });

    dom.addAccountOAuthBtn.addEventListener("click", () => {
      if (state.importMode) return;
      state.oauthFlowActive = true;
      state.oauthFlowStage = "listening";
      state.oauthFlowMessage = "";
      setImportBusy("oauth");
      openAddAccountModal();
      switchAddAccountTab("oauth");
      renderOAuthFlowPanel();
      post("start_oauth_login");
    });

    dom.oauthOpenLinkBtn.addEventListener("click", () => {
      const url = String(dom.oauthAuthLinkInput.value || "").trim();
      if (!url) return;
      post("open_external_url", { url });
    });

    dom.oauthSubmitCallbackBtn.addEventListener("click", () => {
      if (!state.oauthFlowActive || state.importMode !== "oauth") return;
      const callbackUrl = String(dom.oauthCallbackTextarea.value || "").trim();
      if (!callbackUrl) {
        dom.oauthCallbackTextarea.focus();
        return;
      }
      post("submit_oauth_callback", { callbackUrl });
    });

    dom.addAccountManualImportBtn.addEventListener("click", () => {
      if (state.importMode) return;
      const tokenText = dom.manualTokenTextarea.value.trim();
      if (!tokenText) {
        dom.manualTokenTextarea.focus();
        return;
      }
      setImportBusy("manual");
      post("import_manual_token", { tokenData: tokenText });
      dom.manualTokenTextarea.value = "";
      closeAddAccountModal(true);
    });

    dom.addAccountImportCurrentBtn.addEventListener("click", () => {
      if (state.importMode) return;
      closeAddAccountModal(true);
      setImportBusy("current");
      post("backup_current_auto");
    });
    dom.addAccountImportOAuthBtn.addEventListener("click", () => {
      if (state.importMode) return;
      closeAddAccountModal(true);
      setImportBusy("oauth");
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
      if (state.refreshMode || state.bulkMode) return;
      setRefreshBusy("all");
      post("refresh_accounts");
    });
    dom.importBtn.addEventListener("click", () => post("import_accounts"));
    dom.exportBtn.addEventListener("click", () => post("export_accounts"));
    dom.cleanAbnormalBtn.addEventListener("click", () => {
      if (state.cleanAbnormalBusy) return;
      const targets = getAbnormalAccounts().map((x) => ({ account: x.name, group: x.group || "personal" }));
      if (!targets.length) {
        showToast(t("clean_abnormal.empty"), "warning");
        return;
      }
      openConfirm({
        title: t("dialog.confirm.title"),
        message: t("clean_abnormal.confirm", { count: targets.length }),
        onConfirm: () => {
          state.cleanAbnormalBusy = true;
          renderCleanAbnormalButton();
          post("delete_accounts_batch", { items: targets });
        }
      });
    });
    dom.checkUpdateBtn.addEventListener("click", () => requestUpdateCheck("manual"));
    dom.aboutRepoLink.addEventListener("click", (e) => {
      e.preventDefault();
      const url = state.repoUrl || "https://github.com/isxlan0/Codex_AccountSwitch";
      post("open_external_url", { url });
    });

    dom.debugNotifyBtn.addEventListener("click", () => post("debug_test_low_quota_notify"));
    if (dom.quickLangBtn) {
      dom.quickLangBtn.addEventListener("click", () => {
        state.quickLangMenuOpen = !state.quickLangMenuOpen;
        if (dom.quickLangMenu) dom.quickLangMenu.classList.toggle("show", state.quickLangMenuOpen);
        dom.quickLangBtn.classList.toggle("active", state.quickLangMenuOpen);
      });
    }
    if (dom.quickThemeBtn) {
      dom.quickThemeBtn.addEventListener("click", () => {
        const current = normalizeThemeMode(state.themeMode || "auto");
        const next = current === "auto" ? "light" : (current === "light" ? "dark" : "auto");
        setThemeWithButtonTransition(dom.quickThemeBtn, next);
      });
    }

    if (dom.quickLangMenu) {
      dom.quickLangMenu.addEventListener("click", (e) => {
        const btn = e.target.closest("[data-quick-lang]");
        if (!btn) return;
        const code = String(btn.getAttribute("data-quick-lang") || "");
        applyLanguageByCode(code);
        closeQuickLangMenu();
      });
    }

    document.addEventListener("click", (e) => {
      if (dom.toolbarQuick && !dom.toolbarQuick.contains(e.target)) {
        closeQuickLangMenu();
      }
      const targetEl = e.target instanceof Element ? e.target : null;
      if (!targetEl || !targetEl.closest(".custom-select-wrap")) {
        customSelectRegistry.forEach((entry) => entry.closeMenu());
      }
    });

    document.addEventListener("keydown", (e) => {
      if (e.key === "Escape") {
        closeQuickLangMenu();
        customSelectRegistry.forEach((entry) => entry.closeMenu());
      }
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
            group: normalizeGroupValue(x.group),
            planType: String(x.planType || ""),
            usageError: String(x.usageError || ""),
            isCurrent: x.isCurrent === true || x.isCurrent === "true",
            usageOk: x.usageOk === true || x.usageOk === "true",
            abnormal: x.abnormal === true || x.abnormal === "true",
            abnormalReason: String(x.abnormalReason || ""),
            abnormalAt: String(x.abnormalAt || "")
          }))
          : [];
        applySearch();
        renderDashboard();
        renderProxyFixedAccountOptions();
        renderTrafficAccountOptions();
        renderTrafficLogs();
        renderTokenStats();
        renderApiState();
        post("get_api_models");
        log(`host: accounts loaded (${state.accounts.length})`);
        return;
      }

      if (msg && typeof msg === "object" && msg.type === "api_models") {
        const models = Array.isArray(msg.models) ? msg.models.map((x) => String(x || "").trim()).filter(Boolean) : [];
        state.apiModels = models;
        renderApiModelOptions();
        return;
      }

      if (msg && typeof msg === "object" && msg.type === "proxy_status") {
        state.proxyRunning = msg.running === true || msg.running === "true";
        const port = Number(msg.port);
        const timeoutSec = Number(msg.timeoutSec);
        if (Number.isFinite(port) && port > 0) {
          dom.proxyPortInput.value = String(port);
        }
        if (Number.isFinite(timeoutSec) && timeoutSec > 0) {
          dom.proxyTimeoutInput.value = String(timeoutSec);
        }
        if (msg.allowLan === true || msg.allowLan === "true") {
          state.proxyAllowLan = true;
        } else if (msg.allowLan === false || msg.allowLan === "false") {
          state.proxyAllowLan = false;
        }
        if (typeof msg.apiKey === "string") {
          state.proxyApiKey = msg.apiKey;
        }
        if (typeof msg.dispatchMode === "string") {
          state.proxyDispatchMode = msg.dispatchMode;
          dom.proxyDispatchModeSelect.value = state.proxyDispatchMode;
        }
        if (typeof msg.fixedAccount === "string") {
          state.proxyFixedAccount = msg.fixedAccount;
        }
        if (typeof msg.fixedGroup === "string") {
          state.proxyFixedGroup = msg.fixedGroup || "personal";
        }
        renderProxyFixedAccountOptions();
        renderProxyStatus();
        return;
      }

      if (msg && typeof msg === "object" && msg.type === "webdav_sync_status") {
        state.webdavEnabled = msg.enabled === true || msg.enabled === "true";
        state.webdavAutoSync = msg.autoSync !== false && msg.autoSync !== "false";
        state.webdavSyncIntervalMinutes = clampWebDavSyncMinutes(msg.intervalMinutes, state.webdavSyncIntervalMinutes || 15);
        state.webdavSyncRemainingSec = Number.isFinite(Number(msg.remainingSec)) ? Number(msg.remainingSec) : state.webdavSyncRemainingSec;
        state.webdavSyncRunning = msg.running === true || msg.running === "true";
        state.webdavLastSyncAt = String(msg.lastSyncAt || state.webdavLastSyncAt || "");
        state.webdavLastSyncStatus = String(msg.lastSyncStatus || state.webdavLastSyncStatus || "");
        state.webdavPasswordConfigured = msg.passwordConfigured === true || msg.passwordConfigured === "true";
        renderWebDavState();
        return;
      }

      if (msg && typeof msg === "object" && msg.type === "cloud_account_status") {
        const wasCloudQuotaRefreshRunning = isCloudQuotaRefreshRunning();
        state.cloudAccountAutoDownload = msg.autoDownload === true || msg.autoDownload === "true";
        state.cloudAccountIntervalMinutes = clampWebDavSyncMinutes(msg.intervalMinutes, state.cloudAccountIntervalMinutes || 60);
        state.cloudAccountRemainingSec = Number.isFinite(Number(msg.remainingSec)) ? Number(msg.remainingSec) : state.cloudAccountRemainingSec;
        state.cloudAccountDownloadRunning = msg.running === true || msg.running === "true";
        state.cloudAccountLastDownloadAt = String(msg.lastDownloadAt || state.cloudAccountLastDownloadAt || "");
        state.cloudAccountLastDownloadStatus = String(msg.lastDownloadStatus || state.cloudAccountLastDownloadStatus || "");
        state.cloudAccountPasswordConfigured = msg.passwordConfigured === true || msg.passwordConfigured === "true";
        if (state.cloudAccountDownloadRunning && state.enableAutoRefreshQuota && !state.refreshMode && !state.refreshProgressScope) {
          state.refreshProgressCurrent = 0;
          state.refreshProgressTotal = 0;
          state.refreshProgressScope = "cloud";
          renderPrimaryActionButtons();
          renderBulkControls();
          renderAccounts();
        }
        if (!state.cloudAccountDownloadRunning) {
          state.cloudAccountProgressCurrent = 0;
          state.cloudAccountProgressTotal = 0;
          if (wasCloudQuotaRefreshRunning || state.refreshProgressScope === "cloud") {
            state.refreshProgressCurrent = 0;
            state.refreshProgressTotal = 0;
            state.refreshProgressScope = "";
            clearProgressToast("quota_refresh");
            renderPrimaryActionButtons();
            renderBulkControls();
            renderAccounts();
          }
          clearProgressToast("cloud_account");
        }
        renderCloudAccountState();
        return;
      }

      if (msg && typeof msg === "object" && msg.type === "webdav_sync_conflicts") {
        state.webdavConflicts = Array.isArray(msg.conflicts)
          ? msg.conflicts.map((item) => ({ ...item, winner: "remote" }))
          : [];
        renderWebDavConflictModal();
        openWebDavConflictModal();
        return;
      }

      if (msg && typeof msg === "object" && msg.type === "traffic_logs") {
        state.trafficLogs = Array.isArray(msg.items) ? msg.items : [];
        state.trafficPage = 1;
        renderTrafficLogs();
        return;
      }

      if (msg && typeof msg === "object" && msg.type === "token_stats") {
        state.tokenStats = {
          inputTokens: Number(msg.inputTokens),
          outputTokens: Number(msg.outputTokens),
          totalTokens: Number(msg.totalTokens),
          activeAccount: String(msg.activeAccount || ""),
          models: Array.isArray(msg.models) ? msg.models : [],
          accounts: Array.isArray(msg.accounts) ? msg.accounts : [],
          trend: (msg.trend && typeof msg.trend === "object") ? msg.trend : {}
        };
        renderTokenStats();
        return;
      }

      if (msg && typeof msg === "object" && msg.type === "debug_log") {
        const scope = String(msg.scope || "host");
        const text = String(msg.message || "");
        log(`host.${scope}: ${text}`);
        return;
      }

      if (msg && typeof msg === "object" && msg.type === "oauth_flow") {
        const stage = String(msg.stage || "");
        const authUrl = String(msg.authUrl || "").trim();
        const message = String(msg.message || "").trim();
        const flowCode = String(msg.code || "").trim();
        const portNum = Number(msg.port);
        if (Number.isFinite(portNum) && portNum > 0) {
          state.oauthPort = Math.floor(portNum);
        }
        if (authUrl) {
          state.oauthAuthUrl = authUrl;
        }
        if (message) {
          let flowMessage = message;
          if (flowCode) {
            const key = `status_code.${flowCode}`;
            if (state.i18n[key]) {
              flowMessage = t(key);
              const detail = message.replace(/^[^:：]+[:：]\s*/, "").trim();
              if (detail && detail !== message) {
                flowMessage += `: ${detail}`;
              }
            }
          }
          state.oauthFlowMessage = flowMessage;
        } else if (stage !== "completed") {
          state.oauthFlowMessage = "";
        }
        if (stage) {
          state.oauthFlowStage = stage;
        }
        if (["listening", "browser_opened", "callback_received"].includes(stage)) {
          state.oauthFlowActive = true;
          setImportBusy("oauth");
          openAddAccountModal();
          switchAddAccountTab("oauth");
        } else if (["completed", "browser_open_failed", "timeout", "error"].includes(stage)) {
          state.oauthFlowActive = false;
          if (state.importMode === "oauth") {
            setImportBusy("");
          }
          renderOAuthFlowPanel();
        } else {
          renderOAuthFlowPanel();
        }
        return;
      }

      if (msg && typeof msg === "object" && msg.type === "api_result") {
        state.apiRequestBusy = false;
        renderApiState();
        if (msg.ok) {
          dom.apiOutputTextarea.value = String(msg.content || "");
        } else {
          const err = String(msg.error || "api_request_failed");
          const status = Number(msg.status);
          dom.apiOutputTextarea.value = `ERROR: ${err}${Number.isFinite(status) && status > 0 ? ` (HTTP ${status})` : ""}\n\n${String(msg.raw || "")}`;
        }
        return;
      }

      if (msg && typeof msg === "object" && msg.type === "status") {
        log(`host status: level=${msg.level || "info"} code=${msg.code || ""}`);
        const statusCode = String(msg.code || "");
        if (statusCode === "config_saved") {
          return;
        }
        if (["api_request_success", "api_request_failed", "api_invalid_input", "api_no_selected_account", "api_account_auth_missing"].includes(String(msg.code || ""))) {
          state.apiRequestBusy = false;
          renderApiState();
        }
        const switchCode = String(msg.code || "");
        const isManualSwitchSuccess = switchCode === "switch_success" || switchCode === "switch_success_proxy_mode";
        const isAutoSwitch = switchCode === "proxy_auto_switched" || switchCode === "proxy_fixed_auto_switched" || switchCode === "proxy_abnormal_auto_switched";
        if (isManualSwitchSuccess || isAutoSwitch) {
          requestAccountsList(true);
        }
        if (isManualSwitchSuccess) {
          switchTab("accounts");
        }
        if (statusCode === "cloud_account_progress") {
          state.cloudAccountDownloadRunning = true;
          state.cloudAccountProgressCurrent = Math.max(0, Math.trunc(Number(msg.current) || 0));
          state.cloudAccountProgressTotal = Math.max(0, Math.trunc(Number(msg.total) || 0));
          renderCloudAccountState();
          showProgressToast("cloud_account", getCloudProgressStatusText(), msg.level || "info");
          return;
        }
        if (statusCode === "import_auth_batch_progress") {
          if (!state.importMode) {
            setImportBusy("oauth");
          }
          state.importProgressCurrent = Math.max(0, Math.trunc(Number(msg.current) || 0));
          state.importProgressTotal = Math.max(0, Math.trunc(Number(msg.total) || 0));
          renderImportBusy();
          showProgressToast("import_auth", getImportProgressText(), msg.level || "info");
          return;
        }
        if (statusCode === "quota_refresh_progress") {
          const scope = String(msg.scope || "all").toLowerCase() === "batch" ? "batch" : "all";
          const isCloudScope = String(msg.scope || "").toLowerCase() === "cloud";
          const effectiveScope = isCloudScope ? "cloud" : scope;
          if (!isCloudScope && !state.refreshMode) {
            setRefreshBusy("all");
          }
          state.refreshProgressCurrent = Math.max(0, Math.trunc(Number(msg.current) || 0));
          state.refreshProgressTotal = Math.max(0, Math.trunc(Number(msg.total) || 0));
          state.refreshProgressScope = effectiveScope;
          if (effectiveScope === "batch" && state.bulkMode !== "refresh") {
            setBulkBusy("refresh");
          } else {
            renderPrimaryActionButtons();
            renderBulkControls();
            renderAccounts();
          }
          showProgressToast("quota_refresh", getRefreshProgressText(effectiveScope), msg.level || "info");
          return;
        }
        if (["quota_refreshed", "account_quota_refreshed", "account_abnormal_marked"].includes(String(msg.code || ""))) {
          clearProgressToast("quota_refresh");
          setRefreshBusy("", "");
          if (state.importMode === "current") {
            setImportBusy("");
          }
          if (state.bulkMode === "refresh") {
            setBulkBusy("");
          }
        } else if (msg.code === "batch_refresh_done") {
          clearProgressToast("quota_refresh");
          setRefreshBusy("", "");
          if (state.bulkMode === "refresh") {
            setBulkBusy("");
          }
        } else if (["batch_delete_done", "batch_delete_partial", "batch_delete_failed"].includes(String(msg.code || ""))) {
          if (state.bulkMode === "delete") {
            setBulkBusy("");
          }
          if (state.cleanAbnormalBusy) {
            state.cleanAbnormalBusy = false;
            renderCleanAbnormalButton();
          }
        } else if (
          state.refreshMode &&
          [
            "account_abnormal_marked",
            "account_quota_refresh_failed",
            "account_quota_refresh_skipped",
            "quota_refresh_failed",
            "quota_refresh_running"
          ].includes(statusCode)
        ) {
          clearProgressToast("quota_refresh");
          setRefreshBusy("", "");
          if (state.bulkMode === "refresh") {
            setBulkBusy("");
          }
        }

        if (state.importMode === "oauth" && ([
          "import_cancelled",
          "import_auth_batch_done",
          "import_auth_batch_partial",
          "import_auth_batch_failed"
        ].includes(String(msg.code || "")) || isOAuthTerminalCode(msg.code) || msg.level === "error")) {
          clearProgressToast("import_auth");
          setImportBusy("");
          state.oauthFlowActive = false;
          renderOAuthFlowPanel();
        }
        if (state.importMode === "manual") {
          setImportBusy("");
        }
        if (state.importMode === "current" && msg.level === "error") {
          setImportBusy("");
        }
        if ([
          "cloud_account_batch_done",
          "cloud_account_batch_partial",
          "cloud_account_batch_failed",
          "cloud_account_invalid_config",
          "cloud_account_provider_empty",
          "cloud_account_provider_request_failed",
          "cloud_account_provider_invalid_index",
          "cloud_account_provider_item_failed",
          "cloud_account_invalid_payload"
        ].includes(statusCode)) {
          clearProgressToast("cloud_account");
        }

        if (!(msg.silent === true || msg.silent === "true")) {
          showToast(mapStatusMessage(msg), msg.level || "info");
        }
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
          state.enableAutoRefreshQuota = parseEnableAutoRefreshQuota(msg, true);
          state.autoMarkAbnormalAccounts = msg.autoMarkAbnormalAccounts !== false && msg.autoMarkAbnormalAccounts !== "false";
          state.autoDeleteAbnormalAccounts = msg.autoDeleteAbnormalAccounts === true || msg.autoDeleteAbnormalAccounts === "true";
          state.autoRefreshCurrent = msg.autoRefreshCurrent !== false && msg.autoRefreshCurrent !== "false";
          state.lowQuotaAutoPrompt = msg.lowQuotaAutoPrompt !== false && msg.lowQuotaAutoPrompt !== "false";
          state.autoRefreshAllMinutes = clampRefreshMinutes(msg.autoRefreshAllMinutes, 15);
          state.autoRefreshCurrentMinutes = clampRefreshMinutes(msg.autoRefreshCurrentMinutes, 5);
          state.tabVisibility = normalizeTabVisibility(msg.tabVisibility);
          state.themeMode = normalizeThemeMode(msg.theme || "auto");
          try { localStorage.setItem("cas_theme", state.themeMode || "auto"); } catch (e) {}
          state.proxyAllowLan = msg.proxyAllowLan === true || msg.proxyAllowLan === "true";
          state.proxyAutoStart = msg.proxyAutoStart === true || msg.proxyAutoStart === "true";
          state.proxyApiKey = typeof msg.proxyApiKey === "string" ? msg.proxyApiKey : "";
          state.proxyStealthMode = msg.proxyStealthMode === true || msg.proxyStealthMode === "true";
          state.proxyDispatchMode = String(msg.proxyDispatchMode || "round_robin");
          state.proxyFixedAccount = String(msg.proxyFixedAccount || "");
          state.proxyFixedGroup = String(msg.proxyFixedGroup || "personal");
          state.cloudAccountUrl = String(msg.cloudAccountUrl || "");
          state.cloudAccountPasswordConfigured = msg.cloudAccountPasswordConfigured === true || msg.cloudAccountPasswordConfigured === "true";
          state.cloudAccountPasswordClear = false;
          state.cloudAccountAutoDownload = msg.cloudAccountAutoDownload === true || msg.cloudAccountAutoDownload === "true";
          state.cloudAccountIntervalMinutes = clampWebDavSyncMinutes(msg.cloudAccountIntervalMinutes, 60);
          state.cloudAccountLastDownloadAt = String(msg.cloudAccountLastDownloadAt || "");
          state.cloudAccountLastDownloadStatus = String(msg.cloudAccountLastDownloadStatus || "");
          state.webdavEnabled = msg.webdavEnabled === true || msg.webdavEnabled === "true";
          state.webdavAutoSync = msg.webdavAutoSync !== false && msg.webdavAutoSync !== "false";
          state.webdavSyncIntervalMinutes = clampWebDavSyncMinutes(msg.webdavSyncIntervalMinutes, 15);
          state.webdavUrl = String(msg.webdavUrl || "");
          state.webdavRemotePath = String(msg.webdavRemotePath || "/CodexAccountSwitch");
          state.webdavUsername = String(msg.webdavUsername || "");
          state.webdavPasswordConfigured = msg.webdavPasswordConfigured === true || msg.webdavPasswordConfigured === "true";
          state.webdavPasswordClear = false;
          state.webdavLastSyncAt = String(msg.webdavLastSyncAt || "");
          state.webdavLastSyncStatus = String(msg.webdavLastSyncStatus || "");
          if (dom.cloudAccountPasswordInput) dom.cloudAccountPasswordInput.value = "";
          if (dom.webdavPasswordInput) dom.webdavPasswordInput.value = "";
          if (Number.isFinite(Number(msg.proxyPort))) dom.proxyPortInput.value = String(Number(msg.proxyPort));
          if (Number.isFinite(Number(msg.proxyTimeoutSec))) dom.proxyTimeoutInput.value = String(Number(msg.proxyTimeoutSec));
          if (document.documentElement.getAttribute("data-theme") !== resolveEffectiveTheme()) applyTheme();
          state.firstRun = msg.firstRun === true || msg.firstRun === "true";
          renderTopLevelTabs();
          post("get_languages", { code: state.currentLanguage });
          if (state.autoUpdate && !state.didAutoCheckUpdate) {
            state.didAutoCheckUpdate = true;
            requestUpdateCheck("auto");
          }
          if (state.firstRun) {
            switchTab("settings");
            showToast(t("settings.first_run_toast"), "warning");
          } else {
            switchTab(state.currentTab || "dashboard");
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
          state.enableAutoRefreshQuota = parseEnableAutoRefreshQuota(msg, state.enableAutoRefreshQuota);
          state.autoMarkAbnormalAccounts = msg.autoMarkAbnormalAccounts !== false && msg.autoMarkAbnormalAccounts !== "false";
          state.autoDeleteAbnormalAccounts = msg.autoDeleteAbnormalAccounts === true || msg.autoDeleteAbnormalAccounts === "true";
          state.autoRefreshAllMinutes = clampRefreshMinutes(msg.autoRefreshAllMinutes, state.autoRefreshAllMinutes || 15);
          state.autoRefreshCurrentMinutes = clampRefreshMinutes(msg.autoRefreshCurrentMinutes, state.autoRefreshCurrentMinutes || 5);
          state.autoRefreshCurrent = msg.autoRefreshCurrent !== false && msg.autoRefreshCurrent !== "false";
          state.lowQuotaAutoPrompt = msg.lowQuotaAutoPrompt !== false && msg.lowQuotaAutoPrompt !== "false";
          state.tabVisibility = normalizeTabVisibility(msg.tabVisibility);
          state.themeMode = normalizeThemeMode(msg.theme || state.themeMode || "auto");
          try { localStorage.setItem("cas_theme", state.themeMode || "auto"); } catch (e) {}
          state.proxyAllowLan = msg.proxyAllowLan === true || msg.proxyAllowLan === "true";
          state.proxyAutoStart = msg.proxyAutoStart === true || msg.proxyAutoStart === "true";
          state.proxyApiKey = typeof msg.proxyApiKey === "string" ? msg.proxyApiKey : state.proxyApiKey;
          state.proxyStealthMode = msg.proxyStealthMode === true || msg.proxyStealthMode === "true";
          state.proxyDispatchMode = String(msg.proxyDispatchMode || state.proxyDispatchMode || "round_robin");
          state.proxyFixedAccount = String(msg.proxyFixedAccount || state.proxyFixedAccount || "");
          state.proxyFixedGroup = String(msg.proxyFixedGroup || state.proxyFixedGroup || "personal");
          state.cloudAccountUrl = String(msg.cloudAccountUrl || state.cloudAccountUrl || "");
          state.cloudAccountPasswordConfigured = msg.cloudAccountPasswordConfigured === true || msg.cloudAccountPasswordConfigured === "true";
          state.cloudAccountPasswordClear = false;
          state.cloudAccountAutoDownload = msg.cloudAccountAutoDownload === true || msg.cloudAccountAutoDownload === "true";
          state.cloudAccountIntervalMinutes = clampWebDavSyncMinutes(msg.cloudAccountIntervalMinutes, state.cloudAccountIntervalMinutes || 60);
          state.cloudAccountLastDownloadAt = String(msg.cloudAccountLastDownloadAt || state.cloudAccountLastDownloadAt || "");
          state.cloudAccountLastDownloadStatus = String(msg.cloudAccountLastDownloadStatus || state.cloudAccountLastDownloadStatus || "");
          state.webdavEnabled = msg.webdavEnabled === true || msg.webdavEnabled === "true";
          state.webdavAutoSync = msg.webdavAutoSync !== false && msg.webdavAutoSync !== "false";
          state.webdavSyncIntervalMinutes = clampWebDavSyncMinutes(msg.webdavSyncIntervalMinutes, state.webdavSyncIntervalMinutes || 15);
          state.webdavUrl = String(msg.webdavUrl || state.webdavUrl || "");
          state.webdavRemotePath = String(msg.webdavRemotePath || state.webdavRemotePath || "/CodexAccountSwitch");
          state.webdavUsername = String(msg.webdavUsername || state.webdavUsername || "");
          state.webdavPasswordConfigured = msg.webdavPasswordConfigured === true || msg.webdavPasswordConfigured === "true";
          state.webdavPasswordClear = false;
          state.webdavLastSyncAt = String(msg.webdavLastSyncAt || state.webdavLastSyncAt || "");
          state.webdavLastSyncStatus = String(msg.webdavLastSyncStatus || state.webdavLastSyncStatus || "");
          if (dom.cloudAccountPasswordInput) dom.cloudAccountPasswordInput.value = "";
          if (dom.webdavPasswordInput) dom.webdavPasswordInput.value = "";
          if (Number.isFinite(Number(msg.proxyPort))) dom.proxyPortInput.value = String(Number(msg.proxyPort));
          if (Number.isFinite(Number(msg.proxyTimeoutSec))) dom.proxyTimeoutInput.value = String(Number(msg.proxyTimeoutSec));
          if (document.documentElement.getAttribute("data-theme") !== resolveEffectiveTheme()) applyTheme();
          renderTopLevelTabs();
          if (!isTabVisible(state.currentTab)) {
            switchTab(state.currentTab || "dashboard");
          }
          refreshSettingsOptions();
          renderRefreshCountdowns();
        }
        return;
      }

      if (msg && typeof msg === "object" && msg.type === "refresh_timers") {
        if (!state.hasPendingConfigWrite) {
          if (msg.enableAutoRefreshQuota === false || msg.enableAutoRefreshQuota === "false") state.enableAutoRefreshQuota = false;
          if (msg.enableAutoRefreshQuota === true || msg.enableAutoRefreshQuota === "true") state.enableAutoRefreshQuota = true;
          if (msg.autoRefreshQuotaDisabled === false || msg.autoRefreshQuotaDisabled === "false") state.enableAutoRefreshQuota = true;
          if (msg.autoRefreshQuotaDisabled === true || msg.autoRefreshQuotaDisabled === "true") state.enableAutoRefreshQuota = false;
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
        const winRaw = String(msg.currentWindow || "").toLowerCase();
        const winText = winRaw.includes("7") ? t("low_quota.window_7d") : t("low_quota.window_5h");
        openConfirm({
          title: t("low_quota.prompt_title"),
          message: t("low_quota.prompt_message_dynamic", {
            current: msg.currentName || "-",
            currentQuota: msg.currentQuota ?? "-",
            window: winText,
            best: msg.bestName || "-",
            bestQuota: msg.bestQuota ?? "-"
          }),
          persistent: true,
          onConfirm: () => post("confirm_low_quota_switch")
        });
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
          applyLanguageStrings("zh-CN", ZH_FALLBACK_I18N);
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
    state.i18n = { ...ZH_FALLBACK_I18N };
    applyI18n();
    initLanguageIndexFallback();
    renderLanguageOptions();
    bindEvents();
    initCustomSelects();
    bindWebViewMessages();
    startAutoRefreshLoop();
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
    applyLanguageStrings("zh-CN", ZH_FALLBACK_I18N);
    dom.logEl.style.display = state.debug ? "block" : "none";
    dom.debugPanel.style.display = state.debug ? "block" : "none";
    renderRefreshCountdowns();
    renderTrafficAccountOptions();
    renderTrafficLogs();
    renderTokenStats();
    renderQuickSwitchers();

    post("get_app_info");
    post("get_config");
    post("get_languages", { code: "zh-CN" });
    requestAccountsList(true);
    post("get_proxy_status");
  }

  bootstrap();
})();
