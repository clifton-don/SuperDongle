(function() {
  // ========== 多语言配置 ==========
  var i18n = {
    zh: {
      ssidPlaceholder: '网络名称',
      passPlaceholder: '密码',
      scanning: '正在扫描...',
      scanFailed: '扫描失败，点击重试',
      save: '保存并连接',
      rescan: '重新扫描',
      verifying: '正在连接...',
      success: '连接成功',
      failed: '连接失败',
      retry: '重试',
      statusTitle: 'WiFi状态',
      ssid: '网络名称',
      ip: 'IP 地址',
      mode: '模式',
      signal: '信号强度',
      themeLabel: '主题模式',
      langLabel: '语言设置',
      ledLabel: 'LED 指示灯',
      dark: '深色模式',
      light: '浅色模式',
      langName: '简体中文',
      noSavedWifi: '暂无已保存的WiFi',
      operation: '操作',
      connect: '连接',
      delete: '删除',
      disconnected: '未连接',
      connecting: '连接中',
      connected: '已连接',
      unknown: '未知',
      confirmTitle: '确认操作',
      confirmMessage: '当前处于STA模式，如果连接新WiFi会导致断开当前管理页面，是否确定继续？',
      confirmDeleteMessage: '确定删除该WiFi？',
      passMinLen: '密码至少需要8位',
      wifiNotFound: 'WiFi未扫描到',
      savedTitle: '已保存WiFi',
      scanTitle: '扫描WiFi',
      connectTitle: '连接WiFi',
      currentStatus: '当前状态',
      mdnsAddress: '浏览器访问地址',
      networkConfig: '网络管理',
      otherSettings: '其他设置',
      systemFunctions: '系统功能',
      generalSettings: '通用设置',
      versionInfo: '系统管理',
      cancel: '取消',
      confirm: '确定',
      successTitle: '连接成功',
      successHint: '{seconds}秒后AP热点将自动关闭，请切换到 {ssid} 后访问：',
      successIpLabel: '设备 IP 地址',
      successMdnsLabel: '浏览器访问地址',
      successOk: '知道了',
      offlineTitle: '连接断开',
      offlineHint: '网络连接已断开，请连接同一网络后刷新页面',
      mdnsDialogTitle: '修改浏览器访问地址',
      mdnsDialogHint: '仅支持字母、数字和连字符 (-)，长度 1-31 个字符。点击保存并重启后会自动跳转新页面。',
      mdnsPlaceholder: '例如：mybox',
      mdnsSave: '保存并重启',
      firmwareVersion: '固件版本',
      otaTitle: '固件更新',
      otaLabelCurVer: '当前版本',
      otaLabelNewVer: '最新版本',
      otaLabelDesc: '更新说明',
      otaLabelContent: '更新内容',
      otaCheckBtn: '检查更新',
      otaChecking: '检查中...',
      otaUpdateBtn: '立即更新',
      otaCloseBtn: '关闭',
      otaDisconnected: '网络未连接',
      otaError: '更新出错',
      otaUpdating: '更新中',
      otaFirmwarePhase: '正在更新系统固件...',
      otaSpiffsPhase: '正在更新网页固件...',
      otaContentFirmware: '系统固件',
      otaContentSpiffs: '网页固件',
      otaContentBoth: '系统固件 + 网页固件',
      otaWaiting: '等待...',
      otaSuccessTitle: '更新成功',
      otaSuccessHint: '设备即将重启...<br>网页将在 <span id="ota-countdown" style="color: var(--primary); font-weight: 600;">3</span> 秒后自动刷新',
      otaFailed: '更新失败',
      otaCancelBtn: '取消更新',
      usbSwitchTitle: '切换USB模式',
      usbSwitchToMsc: '切换到「网盘模式」将断开键鼠控制功能，确定切换？',
      usbSwitchToHid: '切换到「键鼠模式」将断开文件管理功能，确定切换？',
      usbSwitchToNcm: '切换到「无线网卡」将<span style="color:red">断开网页功能</span>。\n\n按住按钮 3 秒可切回网盘模式。\n\n确定切换？',
      systemOperation: '系统操作',
      factoryReset: '恢复出厂设置',
      factoryResetTitle: '恢复出厂设置',
      factoryResetMessage: '此操作将清除所有已保存配置并重启设备，重启后需重新配网。',
      factoryResetCancel: '取消',
      factoryResetConfirm: '确认重置',
      factoryResetConfirmCountdown: '确认重置 ({count}s)',
      factoryResetting: '正在重置...',
      factoryResetDone: '设备正在恢复出厂设置并重启。重启后请连接 SuPerDongle 热点重新配网。',
      factoryResetFailed: '恢复出厂设置失败，请重试。'
    },
    en: {
      ssidPlaceholder: 'SSID',
      passPlaceholder: 'Password',
      scanning: 'Scanning...',
      scanFailed: 'Scan failed, tap to retry',
      save: 'Save & Connect',
      rescan: 'Rescan',
      verifying: 'Connecting...',
      success: 'Connected',
      failed: 'Failed',
      retry: 'Retry',
      statusTitle: 'WiFi Status',
      ssid: 'SSID',
      ip: 'IP Address',
      mode: 'Mode',
      signal: 'Signal',
      themeLabel: 'Theme',
      langLabel: 'Language',
      ledLabel: 'Status LED',
      dark: 'Dark Mode',
      light: 'Light Mode',
      langName: 'English',
      noSavedWifi: 'No saved WiFi',
      operation: 'Operation',
      connect: 'Connect',
      delete: 'Delete',
      disconnected: 'Disconnected',
      connecting: 'Connecting',
      connected: 'Connected',
      unknown: 'Unknown',
      confirmTitle: 'Confirm',
      confirmMessage: 'You are in STA mode. Connecting to a new WiFi will disconnect the current management page. Continue?',
      confirmDeleteMessage: 'Are you sure you want to delete this WiFi?',
      passMinLen: 'Password must be at least 8 characters',
      wifiNotFound: 'WiFi not found',
      savedTitle: 'Saved WiFi',
      scanTitle: 'Scan WiFi',
      connectTitle: 'Connect WiFi',
      currentStatus: 'Current Status',
      mdnsAddress: 'Browser Address',
      networkConfig: 'Network Manage',
      otherSettings: 'Other Settings',
      systemFunctions: 'System Functions',
      generalSettings: 'General Settings',
      versionInfo: 'System Admin',
      cancel: 'Cancel',
      confirm: 'Confirm',
      successTitle: 'Connected',
      successHint: 'AP will close in {seconds}s. Switch to {ssid} and access:',
      successIpLabel: 'Device IP Address',
      successMdnsLabel: 'Browser Address',
      successOk: 'Got it',
      offlineTitle: 'Disconnected',
      offlineHint: 'Connection lost. Please join the same network and refresh.',
      mdnsDialogTitle: 'Edit Browser Address',
      mdnsDialogHint: 'Only letters, digits and hyphens (-), 1-31 chars. Click save & reboot, will auto-redirect to new address.',
      mdnsPlaceholder: 'e.g. mybox',
      mdnsSave: 'Save & Reboot',
      firmwareVersion: 'Firmware',
      otaTitle: 'Firmware Update',
      otaLabelCurVer: 'Current',
      otaLabelNewVer: 'Latest',
      otaLabelDesc: 'Description',
      otaLabelContent: 'Content',
      otaCheckBtn: 'Check',
      otaChecking: 'Checking...',
      otaUpdateBtn: 'Update Now',
      otaCloseBtn: 'Close',
      otaDisconnected: 'Network unavailable',
      otaError: 'Update failed',
      otaUpdating: 'Updating',
      otaFirmwarePhase: 'Updating firmware...',
      otaSpiffsPhase: 'Updating filesystem...',
      otaContentFirmware: 'Firmware',
      otaContentSpiffs: 'Filesystem',
      otaContentBoth: 'Firmware + Filesystem',
      otaWaiting: 'Waiting...',
      otaSuccessTitle: 'Update successful',
      otaSuccessHint: 'Device is rebooting...<br>Page will refresh in <span id="ota-countdown" style="color: var(--primary); font-weight: 600;">3</span> seconds',
      otaFailed: 'Update failed',
      otaCancelBtn: 'Cancel',
      usbSwitchTitle: 'Switch USB Mode',
      usbSwitchToMsc: 'Switching to "Storage" will disconnect HID control function. Continue?',
      usbSwitchToHid: 'Switching to "HID Mode" will disconnect file management function. Continue?',
      usbSwitchToNcm: 'Switching to "Wireless NIC" will <span style="color:red">disconnect the web page</span>.\n\nLong-press the button for 3s to return to storage mode.\n\nContinue?',
      systemOperation: 'System Operations',
      factoryReset: 'Factory Reset',
      factoryResetTitle: 'Factory Reset',
      factoryResetMessage: 'This will clear all saved settings and reboot. You will need to reconfigure WiFi.',
      factoryResetCancel: 'Cancel',
      factoryResetConfirm: 'Confirm Reset',
      factoryResetConfirmCountdown: 'Confirm Reset ({count}s)',
      factoryResetting: 'Resetting...',
      factoryResetDone: 'Device is resetting and rebooting. After reboot, connect to SuPerDongle hotspot for setup.',
      factoryResetFailed: 'Factory reset failed, please retry.'
    }
  };

  // ========== 应用状态 ==========
  var state = {
    lang: localStorage.getItem('lang') || (navigator.language.slice(0, 2) === 'zh' ? 'zh' : 'en'),
    theme: localStorage.getItem('theme') || (window.matchMedia('(prefers-color-scheme: light)').matches ? 'light' : 'dark'),
    status: 0,
    countdown: 5,
    hostname: 'esp32',
    isScanning: false,  // 防止重复扫描
    selectedNetwork: null,  // 保存选中的网络信息
    originalNetworks: [],  // WiFi列表
    wifiStatus: null,  // 保存WiFi状态数据
    savedWifiList: null,  // 保存已保存WiFi列表数据
    otaStatus: null,  // OTA 状态
    isConnecting: false,  // 是否正在尝试连接（防止定时器干扰）
    targetSsid: null,  // 用户意图连接的目标SSID（避免重连原WiFi时误弹窗）
    fileBusy: false,  // 文件页面上传/下载进行中，阻止离线模块干扰
    otaBusy: false,  // OTA 固件更新进行中，阻止离线模块干扰
    usbMode: 0,  // USB 当前模式：0=MSC, 1=HID
    currentPage: '',  // 当前页面ID
    devMode: localStorage.getItem('devMode') === '1',  // 开发者模式
    offline: false,  // 是否断网
    failCount: 0,  // 连续请求失败次数
  };

  // ========== 页面入口路径映射（来自 pages.js） ==========
  var PAGE_ENTRY = {};

  // 动态发现页面的 USB 模式映射 { pageId: usbModeNumber }
  // 由 pages.js 的响应填充
  var _DYNAMIC_PAGE_MODES = {};

  // pages.js 中注册的所有页面 ID（无论是否有 usbMode）
  // 用于判断"当前页是否属于 pages.js 注册页"，决定是否自动跳转
  // 以及锁定模式下哪些按钮需要禁用
  var _ALL_REGISTERED_PAGE_IDS = {};

  // 锁定模式页 ID 集合：值来自 PAGES_CONFIG.lockingPages（pages.js 中定义）
  // 用于判断"当前页是否是锁定模式页"
  var _LOCKING_PAGE_IDS = {};
  // 物理按钮切换到锁定模式的暂存（pending_mode→断网后立即进入该模式）
  var _pendingLockingMode = -1;
  // 用户刚通过 _enterLockingMode 点击了锁定模式，轮询同步时跳过 auto-switch
  var _lockTransitionPending = false;

  // 判断某个 usbMode 是否是锁定模式（切换后会导致网络断开）
  var _isLockingMode = function(mode) {
    if (typeof mode !== 'number') return false;
    var pageId = _findPageByUsbMode(mode);
    return pageId && _LOCKING_PAGE_IDS[pageId] === true;
  };

  // 通过 usbMode 反向查找 pageId
  var _findPageByUsbMode = function(mode) {
    if (!_DYNAMIC_PAGE_MODES) return null;
    for (var id in _DYNAMIC_PAGE_MODES) {
      if (_DYNAMIC_PAGE_MODES[id] === mode) return id;
    }
    return null;
  };

  // 解析 data-usb-mode 属性值（支持 "0" / "0,1"）返回数字数组
  var _parseModeList = function(attrValue) {
    if (!attrValue) return null;
    var parts = String(attrValue).split(',');
    var result = [];
    for (var i = 0; i < parts.length; i++) {
      var trimmed = parts[i].trim();
      if (trimmed !== '' && !isNaN(Number(trimmed))) {
        result.push(Number(trimmed));
      }
    }
    return result.length > 0 ? result : null;
  };

  // ========== 全局变量和工具函数 ==========
  var $ = function(id) { return document.getElementById(id); };

  var escapeHtml = function(str) {
    var div = document.createElement('div');
    div.textContent = str;
    return div.innerHTML;
  };

  // 确认对话框相关变量（提前声明到全局作用域）
  var confirmDialog = null;
  var confirmTitle = null;
  var confirmMessage = null;
  var confirmOk = null;
  var confirmCancel = null;
  var confirmCallback = null;

  // 连接成功通知弹窗相关变量
  var successDialog = null;
  var successTitle = null;
  var successHint = null;
  var successTimer = null;
  var successIpLabel = null;
  var successIpValue = null;
  var successMdnsLabel = null;
  var successMdnsValue = null;
  var successOk = null;

  // mDNS 编辑弹窗相关变量
   var mdnsDialog = null;
   var mdnsDialogTitle = null;
   var mdnsDialogHint = null;
   var mdnsInput = null;
   var mdnsDialogOk = null;
   var mdnsDialogCancel = null;

  var getSignalLevel = function(rssi) {
    if (rssi >= -55) return 4;
    if (rssi >= -70) return 3;
    if (rssi >= -85) return 2;
    return 1;
  };

  // ========== 确认对话框函数（全局作用域） ==========
  var showConfirmDialog = function(callback, titleKey, messageKey) {
    confirmCallback = callback;
    var t = i18n[state.lang];
    if (confirmTitle) {
      confirmTitle.textContent = t[titleKey || 'confirmTitle'];
    }
    if (confirmMessage) {
      confirmMessage.innerHTML = t[messageKey || 'confirmMessage'];
    }
    if (confirmOk) {
      confirmOk.textContent = t.confirm;
    }
    if (confirmCancel) {
      confirmCancel.textContent = t.cancel;
    }
    confirmDialog.classList.add('show');
  };

  var hideConfirmDialog = function() {
    confirmDialog.classList.remove('show');
    confirmCallback = null;
  };

  var showSuccessDialog = function(ssid, ip, hostname) {
    if (!successDialog) return;
    var t = i18n[state.lang];
    var mdnsAddr = (hostname && hostname !== '-') ? hostname + '.local' : '-';
    var seconds = 20;
    var updateHint = function(s) {
      successHint.textContent = t.successHint.replace('{seconds}', s).replace('{ssid}', ssid || '-');
    };
    successTitle.textContent = t.successTitle;
    updateHint(seconds);
    successIpLabel.textContent = t.successIpLabel;
    successIpValue.textContent = ip || '-';
    successMdnsLabel.textContent = t.successMdnsLabel;
    successMdnsValue.textContent = mdnsAddr;
    successOk.textContent = t.successOk;
    successDialog.classList.add('show');

    if (successTimer) clearInterval(successTimer);
    successTimer = setInterval(function() {
      seconds--;
      if (seconds <= 0) {
        clearInterval(successTimer);
        successTimer = null;
        seconds = 0;
      }
      updateHint(seconds);
    }, 1000);
  };

  var hideSuccessDialog = function() {
    if (successTimer) {
      clearInterval(successTimer);
      successTimer = null;
    }
    if (successDialog) successDialog.classList.remove('show');
  };

  var renderSignalBars = function(level) {
    var html = '';
    for (var i = 1; i <= 4; i++) {
      html += '<span class="' + (i <= level ? 'active' : '') + '" style="height:' + (i * 25) + '%"></span>';
    }
    return html;
  };

  // ========== UI更新 ==========
  var updateUI = function() {
    var t = i18n[state.lang];

    document.body.dataset.theme = state.theme;

    // 侧边栏
    $('label-theme').textContent = t.themeLabel;
    $('label-lang').textContent = t.langLabel;
    $('label-led').textContent = t.ledLabel;
    $('label-mdns').textContent = t.mdnsAddress;
    $('label-firmware').textContent = t.firmwareVersion;
    var labelCurVer = document.getElementById('label-cur-ver');
    if (labelCurVer) labelCurVer.textContent = t.otaLabelCurVer;
    var labelNewVer = document.getElementById('label-new-ver');
    if (labelNewVer) labelNewVer.textContent = t.otaLabelNewVer;
    var labelOtaDesc = document.getElementById('label-ota-desc');
    if (labelOtaDesc) labelOtaDesc.textContent = t.otaLabelDesc;
    var progressLabel = document.getElementById('ota-progress-label');
    if (progressLabel) progressLabel.textContent = t.otaUpdating;
    $('btn-theme').textContent = state.theme === 'dark' ? t.dark : t.light;
    $('btn-lang').textContent = t.langName;
    
    // 侧边栏按钮文本：所有带 data-page 的按钮统一更新
    var allPageBtns = document.querySelectorAll('.sidebar-btn[data-page]');
    allPageBtns.forEach(function(btn) {
      var pid = btn.dataset.page;
      var labelEl = btn.querySelector('.sidebar-label') || btn.querySelector('span');
      if (pid === 'network' && labelEl) labelEl.textContent = t.networkConfig;
      else if (pid === 'other' && labelEl) labelEl.textContent = t.otherSettings;
      // labelI18n 行内翻译（无需改 index.js）
      else if (labelEl && labelEl.dataset && labelEl.dataset.labelI18n) {
        try {
          var i18nMap = JSON.parse(labelEl.dataset.labelI18n);
          labelEl.textContent = i18nMap[state.lang] || i18nMap['en'] || labelEl.textContent;
        } catch(e) {}
      }
      else if (labelEl && labelEl.dataset && labelEl.dataset.labelKey && t[labelEl.dataset.labelKey]) {
        labelEl.textContent = t[labelEl.dataset.labelKey];
      } else if (labelEl && labelEl.dataset && labelEl.dataset.labelFallback) {
        if (state.lang === 'en') {
          // 英文下优先使用 dataset.labelKey 指向的 i18n 条目（已优先），否则使用 labelFallback
          if (!(labelEl.dataset.labelKey && t[labelEl.dataset.labelKey])) {
            labelEl.textContent = labelEl.dataset.labelFallback;
          }
        }
      }
    });
    
    // USB 模式按钮
    updateUsbModeUI();

    // 侧边栏分组标题（系统功能 / USB功能 / 其他功能）
    // 统一支持两种命名方式：data-label-zh / data-label-en（静态分组标题和 JS 生成的分组标题通用）
    var sectionTitles = document.querySelectorAll('.sidebar-section-title');
    sectionTitles.forEach(function(titleEl) {
      // 1) 优先走字典 key（data-label-key），与其他按钮逻辑一致
      if (titleEl.dataset && titleEl.dataset.labelKey && t[titleEl.dataset.labelKey]) {
        titleEl.textContent = t[titleEl.dataset.labelKey];
        return;
      }
      // 2) 兜底：使用 data-label-zh / data-label-en 提供的静态文本
      if (state.lang === 'en' && titleEl.dataset.labelEn) {
        titleEl.textContent = titleEl.dataset.labelEn;
      } else if (titleEl.dataset.labelZh) {
        titleEl.textContent = titleEl.dataset.labelZh;
      }
    });

    // 其他设置页面标题
    var generalSettingTitle = document.querySelector('#other-page .accordion-title');
    if (generalSettingTitle) generalSettingTitle.textContent = t.generalSettings;
    var versionInfoTitle = document.getElementById('title-version-info');
    if (versionInfoTitle) versionInfoTitle.textContent = t.versionInfo;
    
    // 系统操作
    var labelSystemOperation = document.getElementById('label-system-operation');
    if (labelSystemOperation) labelSystemOperation.textContent = t.systemOperation;
    var btnFactoryResetText = document.getElementById('btn-factory-reset-text');
    if (btnFactoryResetText) btnFactoryResetText.textContent = t.factoryReset;

    // 模块标题
    $('title-status').textContent = t.statusTitle;
    $('title-saved').textContent = t.savedTitle;
    $('title-scan').textContent = t.scanTitle;
    $('title-connect').textContent = t.connectTitle;

    // 表单
    $('input-ssid').placeholder = t.ssidPlaceholder;
    $('input-pass').placeholder = t.passPlaceholder;
    $('btn-rescan').textContent = t.rescan;

    // WiFi列表扫描中状态
    var wifiList = $('wifi-list');
    var loadingEl = wifiList.querySelector('.loading-text');
    if (loadingEl) {
      if (state.status === 4) {
        // 扫描中：保留旋转圆圈，只更新文本
        var spinner = loadingEl.querySelector('.loading-spinner');
        if (spinner) {
          loadingEl.innerHTML = '<div class="loading-spinner"></div>' + t.scanning;
        } else {
          loadingEl.textContent = t.scanning;
        }
      } else if (loadingEl.style.cursor === 'pointer') {
        // 扫描失败：更新失败提示文本
        loadingEl.textContent = t.scanFailed;
      }
    }

    // 保存按钮状态
    var saveBtn = $('btn-save');
    saveBtn.className = 'btn';
    saveBtn.disabled = false;

    if (state.status === 1) {
      saveBtn.textContent = t.verifying;
      saveBtn.className = 'btn info';
      saveBtn.disabled = true;
    } else if (state.status === 2) {
      saveBtn.textContent = t.success;
      saveBtn.className = 'btn success';
      saveBtn.disabled = true;
    } else if (state.status === 3) {
      saveBtn.textContent = t.retry;
      saveBtn.className = 'btn error';
    } else {
      saveBtn.textContent = t.save;
    }

  };

  // ========== WiFi扫描 ==========
  var scanWiFi = async function() {
    if (state.isScanning) return;
    state.isScanning = true;

    var wifiList = $('wifi-list');
    var rescanBtn = $('btn-rescan');
    var t = i18n[state.lang];

    state.status = 4;
    rescanBtn.disabled = true;
    wifiList.innerHTML = '<div class="loading-text"><div class="loading-spinner"></div>' + t.scanning + '</div>';

    try {
      var ctrl = new AbortController();
      var timer = setTimeout(function() { ctrl.abort(); }, 10000);
      var response = await fetch('/api/wifi/scan', { signal: ctrl.signal });
      clearTimeout(timer);
      var data = await response.json();

      // 数据有效性检查
      if (!data || !Array.isArray(data.items)) {
        throw new Error('Invalid response');
      }

      var networks = data.items;
      state.status = 0;
      state.isScanning = false;
      rescanBtn.disabled = false;

      if (!networks.length) {
        wifiList.innerHTML = '<div class="loading-text" style="cursor:pointer">' + t.scanFailed + '</div>';
        wifiList.onclick = scanWiFi;
        return;
      }

      // 按信号强度排序
      networks.sort(function(a, b) { return b.rssi - a.rssi; });

      // 保存原始网络列表
      state.originalNetworks = networks;

      // 渲染WiFi列表
      renderWifiList();
      wifiList.onclick = null;

    } catch (e) {
      state.status = 0;
      state.isScanning = false;
      rescanBtn.disabled = false;
      state.originalNetworks = [];
      var t = i18n[state.lang];
      wifiList.innerHTML = '<div class="loading-text" style="cursor:pointer">' + t.scanFailed + '</div>';
      wifiList.onclick = scanWiFi;
    }
  };

  // ========== 渲染WiFi列表 ==========
  var renderWifiList = function() {
    var wifiList = $('wifi-list');
    var networks = state.originalNetworks.slice();

    // 同名WiFi只保留一个，优先保留需要密码的
    var seenSsid = {};
    var result = [];
    for (var i = 0; i < networks.length; i++) {
      var net = networks[i];
      if (seenSsid[net.ssid] === undefined) {
        seenSsid[net.ssid] = net.auth;
        result.push(net);
      } else if (net.auth > 0 && seenSsid[net.ssid] === 0) {
        for (var j = 0; j < result.length; j++) {
          if (result[j].ssid === net.ssid) {
            result[j] = net;
            break;
          }
        }
        seenSsid[net.ssid] = net.auth;
      }
    }
    networks = result;

    var html = '';
    for (var i = 0; i < networks.length; i++) {
      var net = networks[i];
      var level = getSignalLevel(net.rssi);
      var lockIcon = net.auth > 0 ? '<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><rect x="3" y="11" width="18" height="11" rx="2" ry="2"></rect><path d="M7 11V7a5 5 0 0 1 10 0v4"></path></svg>' : '';
      html += '<div class="wifi-item" data-ssid="' + escapeHtml(net.ssid) + '" data-encrypted="' + (net.auth > 0 ? 'true' : 'false') + '">' +
        '<span class="wifi-name">' + escapeHtml(net.ssid) + '</span>' +
        '<div style="display: flex; align-items: center; gap: 12px;">' +
        lockIcon +
        '<div class="signal">' + renderSignalBars(level) + '</div>' +
        '</div></div>';
    }
    wifiList.innerHTML = html;
  };

  // ========== 连接状态轮询 ==========
  var pollConnectionStatus = function() {
    var checkInterval = setInterval(async function() {
      try {
        var data = await getWifiStatus();

        if (data.state === 2) {
          clearInterval(checkInterval);
          state.status = 2;
          state.isConnecting = false;
          var connectedSsid = data.ssid;
          var targetSsid = state.targetSsid;
          state.targetSsid = null;
          updateUI();
          renderWifiStatus();

          // 只在真正连上目标SSID（且为AP+STA模式）时弹窗
          if (data.mode === 'AP+STA' && targetSsid && connectedSsid === targetSsid) {
            showSuccessDialog(data.ssid, data.ip, data.hostname);
          }

          // 2秒后恢复默认状态
          setTimeout(function() {
            state.status = 0;
            updateUI();
          }, 2000);

        } else if (data.state === 3) {
          clearInterval(checkInterval);
          state.status = 3;
          state.isConnecting = false;
          state.targetSsid = null;
          updateUI();

          // 2秒后恢复默认状态
          setTimeout(function() {
            state.status = 0;
            updateUI();
          }, 2000);
        }
      } catch (e) { /* 继续轮询 */ }
    }, 1000);
  };

  // ========== 保存配置 ==========
  var saveConfig = async function(ssid, pass) {
    state.status = 1;
    state.isConnecting = true;
    state.targetSsid = ssid;
    updateUI();

    var url = '/api/wifi/connect?ssid=' + encodeURIComponent(ssid);
    if (pass) {
      url += '&password=' + encodeURIComponent(pass);
    }

    try {
      var ctrl = new AbortController();
      var timer = setTimeout(function() { ctrl.abort(); }, 15000);
      await fetch(url, { method: 'POST', signal: ctrl.signal });
      clearTimeout(timer);
    } catch (e) {
      // POST可能超时但ESP32已收到请求并开始连接，不直接标记失败
    }

    // 无论POST结果如何，始终通过轮询获取真实连接状态
    pollConnectionStatus();
  };

  // ========== 获取WiFi状态数据 ==========
  var getWifiStatus = async function() {
    try {
      var ctrl = new AbortController();
      var timer = setTimeout(function() { ctrl.abort(); }, 2000);
      var response = await fetch('/api/wifi/status', { signal: ctrl.signal });
      clearTimeout(timer);
      var data = await response.json();

      if (!data) {
        throw new Error('Invalid response');
      }

      state.wifiStatus = data;
      return data;
    } catch (e) {
      console.error('Failed to get WiFi status:', e);
      state.wifiStatus = null;
      throw e;
    }
  };

  // ========== 渲染WiFi状态 ==========
  var renderWifiStatus = function() {
    var t = i18n[state.lang];
    var content = $('wifi-status-content');

    if (!state.wifiStatus) {
      // 当没有数据时，保持初始结构，不显示错误信息
      return;
    }

    var data = state.wifiStatus;
    var modeText = data.mode || '-';
    var signalPercent = 0;
    var rssiText = '-';
    if (data.connected && data.rssi) {
      signalPercent = Math.min(100, Math.max(0, 2 * (data.rssi + 100)));
      rssiText = data.rssi + ' dBm';
    }

    var stateText = '';
    var stateClass = '';
    switch(data.state) {
      case 0: stateText = t.disconnected; stateClass = 'idle'; break;
      case 1: stateText = t.connecting; stateClass = 'connecting'; break;
      case 2: stateText = t.connected; stateClass = 'connected'; break;
      case 3: stateText = t.failed; stateClass = 'failed'; break;
      default: stateText = t.unknown; stateClass = 'idle';
    }

    var hostname = data.hostname || '-';
    var mdnsAddress = hostname === '-' ? '-' : hostname + '.local';

    // 直接设置content容器的innerHTML
    content.innerHTML =
      '<div style="display: flex; gap: 12px; margin-top: 0;">' +
      '<div class="result-card" style="flex: 1;"><small class="result-label">' + t.currentStatus + '</small><div style="display: flex; align-items: center; gap: 8px;"><span class="status-indicator ' + stateClass + '"></span><span class="result-value">' + stateText + '</span></div></div>' +
      '<div class="result-card" style="flex: 1;"><small class="result-label">' + t.mdnsAddress + '</small><span class="result-value">' + mdnsAddress + '</span></div>' +
      '</div>' +
      '<div style="display: flex; gap: 12px; margin-top: 12px;">' +
      '<div class="result-card" style="flex: 1;"><small class="result-label">' + t.ssid + '</small><span class="result-value">' + (data.ssid || '-') + '</span></div>' +
      '<div class="result-card" style="flex: 1;"><small class="result-label">' + t.ip + '</small><span class="result-value">' + (data.ip || '-') + '</span></div>' +
      '</div>' +
      '<div style="display: flex; gap: 12px; margin-top: 12px;">' +
      '<div class="result-card" style="flex: 1;"><small class="result-label">' + t.mode + '</small><span class="result-value">' + modeText + '</span></div>' +
      '<div class="result-card" style="flex: 1;"><small class="result-label">' + t.signal + '</small><span class="result-value">' + (data.connected ? signalPercent + '% (' + rssiText + ')' : '-') + '</span></div>' +
      '</div>';
  };

  // ========== 刷新WiFi状态 ==========
  var fetchWifiStatus = async function() {
    if (state.fileBusy || state.otaBusy) return;
    try {
      await getWifiStatus();
      state.failCount = 0;
      exitOffline();
      renderWifiStatus();
      initMdnsSettings();
    } catch (e) {
      state.failCount++;
      // 连续失败且不在连接/扫描中时触发离线模式
      // 但需跳过锁定模式切换场景：
      // - UI 按钮点击：state.usbMode 已设，_isLockingMode 返回 true
      // - 物理按钮：_pendingLockingMode 已缓存 pending_mode
      if (state.failCount >= 2 && !state.isConnecting && !state.isScanning) {
        if (_pendingLockingMode !== -1 || _isLockingMode(state.usbMode)) {
          state.failCount = 0;
          return;
        }
        enterOffline();
      }
    }
  };

  // ========== 断网模式 ==========
  var enterOffline = function() {
    if (state.offline) return;
    // 锁定模式下不进入离线模式（NCM 本身就会断网，优先级高于断网模式）
    if (_isLockingMode(state.usbMode)) return;
    state.offline = true;
    // 切换到网络管理页面并显示断网内容
    switchPage('network', true);
    _applyNetworkPage();
    // 锁定所有 pages.js 页按钮 + 隐藏系统管理 + 隐藏开发者模式 UI
    _updateButtonStates();
    applyDevMode();
    $('version-view').style.display = 'none';
  };

  var exitOffline = function() {
    if (!state.offline) return;
    state.offline = false;
    state.failCount = 0;
    // 恢复网络管理页面正常内容
    _applyNetworkPage();
    // 恢复 pages.js 页按钮 + 恢复系统管理 + 恢复开发者模式
    _updateButtonStates();
    applyDevMode();
    if (state.currentPage === 'other') {
      $('version-view').style.display = '';
    }
  };

  // 切换网络管理页面的正常/断网内容
  // 断网模式（非锁定）或锁定模式（NCM）下均显示断网内容
  var _applyNetworkPage = function() {
    var isOffline = state.offline || _isLockingMode(state.usbMode);
    var normalContent = document.getElementById('network-normal-content');
    var offlineContent = document.getElementById('network-offline-content');
    if (!normalContent || !offlineContent) return;
    if (isOffline) {
      var t = i18n[state.lang];
      var hintEl = document.getElementById('network-offline-hint');
      if (hintEl) hintEl.textContent = t.offlineHint;
      normalContent.classList.add('hidden');
      offlineContent.classList.remove('hidden');
    } else {
      normalContent.classList.remove('hidden');
      offlineContent.classList.add('hidden');
    }
  };

  var updateUsbModeUI = function() {
    _syncModeIndicators();
    _updateButtonStates();
    _applyNetworkPage();  // 锁定模式下网络管理页显示断网内容
    applyDevMode();  // 锁定模式下强制隐藏开发者模式 UI
  };

  // 同步侧边栏按钮的模式指示器：data-usb-mode 匹配 state.usbMode 的按钮显示圆点
  var _syncModeIndicators = function() {
    var buttons = document.querySelectorAll('.sidebar-btn');
    buttons.forEach(function(btn) {
      var modes = _parseModeList(btn.getAttribute('data-usb-mode'));
      if (modes) {
        var isActive = modes.indexOf(state.usbMode) !== -1;
        btn.classList.toggle('mode-active', isActive);
      } else {
        btn.classList.remove('mode-active');
      }
    });
  };

  // 锁定模式下禁用 pages.js 注册的所有其他页面按钮，系统功能按钮不受影响
  var _updateButtonStates = function() {
    var isLocking = _isLockingMode(state.usbMode);
    var buttons = document.querySelectorAll('.sidebar-btn');
    buttons.forEach(function(btn) {
      var pageId = btn.getAttribute('data-page');
      // 非 pages.js 注册的按钮（系统功能：网络管理、其他设置）不受影响
      if (!pageId || !(pageId in _ALL_REGISTERED_PAGE_IDS)) return;
      if (isLocking) {
        // 锁定模式（NCM）：仅锁定其他 pages.js 页，保留当前锁定页
        var currentPageId = _findPageByUsbMode(state.usbMode);
        btn.classList.toggle('page-locked', pageId !== currentPageId);
      } else if (state.offline) {
        // 断网模式（非锁定）：锁定所有 pages.js 页
        btn.classList.add('page-locked');
      } else {
        btn.classList.remove('page-locked');
      }
    });
  };

  // 统一协调页面与 USB 模式：确保当前页面与 usbMode 对齐
  // 幂等、与轮询顺序无关，可安全地在多处调用
  var reconcilePageWithMode = function() {
    var targetPage = _findPageByUsbMode(state.usbMode);
    if (targetPage && state.currentPage !== targetPage) {
      switchPage(targetPage, true);
    }
  };

  // 点击 UI 按钮进入锁定模式（如 NCM）：确认弹窗 → 立即切页 + 后台 POST
  var _enterLockingMode = function(targetMode, pageId) {
    var t = i18n[state.lang];
    var msgKey = targetMode === 0 ? 'usbSwitchToMsc' : targetMode === 1 ? 'usbSwitchToHid' : 'usbSwitchToNcm';
    showConfirmDialog(function(confirmed) {
      if (!confirmed) return;
      _lockTransitionPending = true;
      state.usbMode = targetMode;
      updateUsbModeUI();
      switchPage(pageId, true);
      fetch('/api/usb/mode', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ mode: targetMode })
      }).catch(function() {});
    }, 'usbSwitchTitle', msgKey);
  };

  var switchUsbMode = function(targetMode, callback) {
    var t = i18n[state.lang];
    var msgKey;
    if (targetMode === 0) msgKey = 'usbSwitchToMsc';
    else if (targetMode === 1) msgKey = 'usbSwitchToHid';
    else msgKey = 'usbSwitchToNcm';
    state._switchId = (state._switchId || 0) + 1;
    var mySwitchId = state._switchId;
    showConfirmDialog(function(confirmed) {
      if (!confirmed) return;
      if (state._switchId !== mySwitchId) return;
      // POST + 轮询确认完成后切页
      var done = false;
      var pollTimer;
      var tryCallback = function() {
        if (done) return;
        done = true;
        if (pollTimer) clearInterval(pollTimer);
        if (state._switchId !== mySwitchId) return;
        state.usbMode = targetMode;
        updateUsbModeUI();
        if (callback) callback();
      };
      fetch('/api/usb/mode', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ mode: targetMode })
      }).then(function(r) { return r.json(); })
        .then(function(data) {
          if (data && data.ok) tryCallback();
        })
        .catch(function() {});
      // 快速轮询确认（每 500ms，最多 10 次 = 5s）
      var attempts = 0;
      pollTimer = setInterval(function() {
        attempts++;
        fetch('/api/usb/status')
          .then(function(r) { return r.json(); })
          .then(function(data) {
            if (data && data.mode === targetMode) {
              tryCallback();
            }
          })
          .catch(function() {});
        if (attempts >= 10) tryCallback();
      }, 500);
    }, 'usbSwitchTitle', msgKey);
  };

  // ========== 展开/折叠功能 ==========
  window.toggleSection = function(sectionId) {
    var section = document.getElementById(sectionId);
    var content = section.querySelector('.accordion-content');
    var arrow = section.querySelector('.accordion-arrow');

    if (content.classList.contains('collapsed')) {
      content.classList.remove('collapsed');
      arrow.classList.remove('collapsed');
    } else {
      content.classList.add('collapsed');
      arrow.classList.add('collapsed');
    }
  }

  // ========== 渲染已保存WiFi列表 ==========
  var renderSavedWifi = function() {
    var t = i18n[state.lang];
    var savedList = $('saved-list');
    var savedEmpty = $('saved-empty');

    if (!state.savedWifiList) {
      // 当没有数据时，保持初始结构，显示"暂无已保存的WiFi"
      savedEmpty.textContent = t.noSavedWifi;
      savedEmpty.style.display = 'block';
      return;
    }

    var wifiList = state.savedWifiList;
    if (wifiList.length === 0) {
      savedList.innerHTML = '';
      savedEmpty.textContent = t.noSavedWifi;
      savedEmpty.style.display = 'block';
      return;
    }

    savedEmpty.style.display = 'none';
    var html = '';

    // 添加表头
    html += '<div class="wifi-item" style="font-weight: 600; background-color: var(--input); color: var(--primary); border-bottom: 1px solid var(--border); display: flex; align-items: center; justify-content: space-between;">' +
      '<div style="display: flex; align-items: center; gap: 12px; flex: 1;">' +
      '<span style="width: 40px; text-align: center;">#</span>' +
      '<span class="wifi-name">' + t.ssid + '</span>' +
      '</div>' +
      '<div style="width: 160px; text-align: right; padding-right: 50px;">' +
      t.operation +
      '</div>' +
      '</div>';

    for (var i = 0; i < wifiList.length; i++) {
      var wifi = wifiList[i];
      html += '<div class="wifi-item" data-ssid="' + escapeHtml(wifi.ssid) + '" style="display: flex; align-items: center; justify-content: space-between;">' +
        '<div style="display: flex; align-items: center; gap: 12px; flex: 1;">' +
        '<span style="width: 40px; text-align: center;">' + (i + 1) + '</span>' +
        '<span class="wifi-name">' + escapeHtml(wifi.ssid) + '</span>' +
        '</div>' +
        '<div style="display: flex; gap: 8px; width: 160px; justify-content: flex-end;">' +
        '<button type="button" class="btn small" data-action="connect">' + t.connect + '</button>' +
        '<button type="button" class="btn small error" data-action="delete">' + t.delete + '</button>' +
        '</div>' +
        '</div>';
    }

    savedList.innerHTML = html;
  };

  // ========== 加载 mDNS 设置 ==========
  var initMdnsSettings = function() {
    if (state.wifiStatus && state.wifiStatus.hostname && state.wifiStatus.hostname !== '-') {
      var btn = document.getElementById('btn-mdns');
      if (btn) btn.textContent = state.wifiStatus.hostname + '.local';
    }
  };

  // ========== OTA 状态 ==========
  var fetchOtaStatus = async function() {
    try {
      var ctrl = new AbortController();
      var timer = setTimeout(function() { ctrl.abort(); }, 5000);
      var response = await fetch('/api/ota/status', { signal: ctrl.signal });
      clearTimeout(timer);
      var data = await response.json();
      state.otaStatus = data;

      var verEl = document.getElementById('firmware-version');
      if (verEl) verEl.textContent = data.current_version || '-';

      updateOtaBadge();
    } catch (e) {
      // OTA 状态获取失败，不处理
    }
  };

  var updateOtaBadge = function() {
    var badge = document.getElementById('ota-badge');
    if (!badge) return;
    if (state.otaStatus && state.otaStatus.update_available) {
      badge.classList.remove('hidden');
    } else {
      badge.classList.add('hidden');
    }
  };

  window.showOtaDialog = function() {
    var t = i18n[state.lang];
    var dialog = document.getElementById('ota-dialog');
    var titleEl = document.getElementById('ota-dialog-title');
    var infoTable = document.getElementById('ota-info-table');
    var curVerEl = document.getElementById('ota-current-ver');
    var newVerRow = document.getElementById('ota-new-ver-row');
    var newVerEl = document.getElementById('ota-new-ver');
    var descRow = document.getElementById('ota-desc-row');
    var descEl = document.getElementById('ota-desc');
    var contentRow = document.getElementById('ota-content-row');
    var contentEl = document.getElementById('ota-content');
    var offlineRow = document.getElementById('ota-offline-row');
    var updateBtn = document.getElementById('ota-dialog-update');
    var checkBtn = document.getElementById('ota-dialog-check');
    var closeBtn = document.getElementById('ota-dialog-close');
    var cancelBtn = document.getElementById('ota-dialog-cancel');
    var successTitleEl = document.getElementById('ota-success-title');
    var successHintEl = document.getElementById('ota-success-hint');

    titleEl.textContent = t.otaTitle;
    closeBtn.textContent = t.otaCloseBtn;
    checkBtn.textContent = t.otaCheckBtn;
    if (successTitleEl) successTitleEl.textContent = t.otaSuccessTitle;
    if (successHintEl) successHintEl.innerHTML = t.otaSuccessHint.replace('\n', '<br>');

    document.getElementById('label-cur-ver').textContent = t.otaLabelCurVer;
    document.getElementById('label-new-ver').textContent = t.otaLabelNewVer;
    document.getElementById('label-ota-desc').textContent = t.otaLabelDesc;
    document.getElementById('label-ota-content').textContent = t.otaLabelContent;

    checkBtn.disabled = false;
    checkBtn.style.display = '';
    updateBtn.style.display = '';
    updateBtn.textContent = t.otaUpdateBtn;
    cancelBtn.style.display = 'none';

    // 重置进度和成功区域
    var progressArea = document.getElementById('ota-progress-area');
    var successArea = document.getElementById('ota-success-area');
    if (progressArea) progressArea.style.display = 'none';
    if (successArea) successArea.style.display = 'none';
    var errArea = document.getElementById('ota-error-area');
    if (errArea) errArea.style.display = 'none';

    if (state.otaStatus) {
      offlineRow.style.display = 'none';
      infoTable.style.display = '';
      curVerEl.textContent = state.otaStatus.current_version || '1.0.0';
      newVerRow.style.display = '';
      newVerEl.textContent = state.otaStatus.new_version || state.otaStatus.current_version || '1.0.0';

      if (state.otaStatus.error) {
        descRow.style.display = 'none';
        contentRow.style.display = 'none';
        updateBtn.style.display = 'none';
      } else if (state.otaStatus.update_available) {
        descRow.style.display = state.otaStatus.description ? '' : 'none';
        descEl.innerHTML = (state.otaStatus.description || '').replace(/\\n/g, '<br>');
        contentRow.style.display = '';
        if (state.otaStatus.has_firmware && state.otaStatus.has_spiffs) {
          contentEl.textContent = t.otaContentBoth;
        } else if (state.otaStatus.has_firmware) {
          contentEl.textContent = t.otaContentFirmware;
        } else if (state.otaStatus.has_spiffs) {
          contentEl.textContent = t.otaContentSpiffs;
        } else {
          contentRow.style.display = 'none';
        }
        updateBtn.style.display = '';
        updateBtn.disabled = false;
        updateBtn.textContent = t.otaUpdateBtn;
      } else {
        descRow.style.display = 'none';
        contentRow.style.display = 'none';
        updateBtn.style.display = 'none';
      }
    } else {
      offlineRow.style.display = '';
      offlineRow.textContent = t.otaDisconnected;
      infoTable.style.display = 'none';
      newVerRow.style.display = 'none';
      descRow.style.display = 'none';
      contentRow.style.display = 'none';
      updateBtn.style.display = 'none';
    }

    dialog.classList.add('show');
  };

  var closeOtaDialog = function() {
    if (_otaPollTimer) return;
    var dialog = document.getElementById('ota-dialog');
    dialog.classList.remove('show');
    // 等待过渡动画完成后再重置内容
    setTimeout(function() {
      // 过渡完成，可以重置内容（如果需要）
    }, 300);
  };

  var _otaPollTimer = null;
  var _otaSpiffsSeen = false;
  var _otaPollFailCount = 0;

  var handleOtaUpdate = async function() {
    var t = i18n[state.lang];
    var updateBtn = document.getElementById('ota-dialog-update');
    var closeBtn = document.getElementById('ota-dialog-close');
    var checkBtn = document.getElementById('ota-dialog-check');
    var cancelBtn = document.getElementById('ota-dialog-cancel');
    var progressArea = document.getElementById('ota-progress-area');
    var fwBar = document.getElementById('ota-fw-bar');
    var fwPct = document.getElementById('ota-fw-pct');
    var fwLabel = document.getElementById('ota-fw-label');
    var spiffsRow = document.getElementById('ota-spiffs-progress-row');
    var spiffsBar = document.getElementById('ota-spiffs-bar');
    var spiffsPct = document.getElementById('ota-spiffs-pct');
    var spiffsLabel = document.getElementById('ota-spiffs-label');
    var fwRow = document.getElementById('ota-fw-progress-row');
    var successArea = document.getElementById('ota-success-area');
    var infoTable = document.getElementById('ota-info-table');

    var hasFw = state.otaStatus && state.otaStatus.has_firmware;
    var hasSp = state.otaStatus && state.otaStatus.has_spiffs;
    _otaSpiffsSeen = false;
    _otaPollFailCount = 0;

    updateBtn.disabled = true;
    closeBtn.disabled = true;
    checkBtn.disabled = true;
    updateBtn.textContent = t.otaUpdating;
    state.otaBusy = true;

    try {
      var ctrl = new AbortController();
      var timer = setTimeout(function() { ctrl.abort(); }, 10000);
      var response = await fetch('/api/ota/update', { method: 'POST', signal: ctrl.signal });
      clearTimeout(timer);
      var data = await response.json();
      if (!data || !data.ok) {
        throw new Error(data && data.error ? data.error : 'update failed');
      }
      updateBtn.style.display = 'none';
      if (cancelBtn) {
        cancelBtn.style.display = '';
        cancelBtn.textContent = t.otaCancelBtn;
        cancelBtn.disabled = false;
      }
      if (progressArea) {
        progressArea.style.display = '';
        if (hasFw) {
          fwRow.style.display = '';
          fwBar.style.width = '0%';
          fwPct.textContent = '0%';
          fwLabel.textContent = t.otaFirmwarePhase;
        } else {
          fwRow.style.display = 'none';
        }
        if (hasSp) {
          spiffsRow.style.display = '';
          spiffsBar.style.width = '0%';
          spiffsPct.textContent = hasFw ? t.otaWaiting : '0%';
          spiffsLabel.textContent = t.otaSpiffsPhase;
        } else {
          spiffsRow.style.display = 'none';
        }
      }

      _otaPollTimer = setInterval(async function() {
        try {
          await fetchOtaStatus();
          if (!state.otaStatus || !state.otaStatus.busy) {
            clearInterval(_otaPollTimer);
            _otaPollTimer = null;
            state.otaBusy = false;
            if (infoTable) infoTable.style.display = 'none';
            if (progressArea) progressArea.style.display = 'none';
            if (cancelBtn) cancelBtn.style.display = 'none';
            updateBtn.style.display = 'none';
            checkBtn.style.display = 'none';
            closeBtn.style.display = '';
            closeBtn.disabled = false;
            if (state.otaStatus && state.otaStatus.error) {
              /* 更新出错 */
              var errMsg = state.otaStatus.status_msg || t.otaError;
              successArea.style.display = 'none';
              var errArea = document.getElementById('ota-error-area');
              var errMsgEl = document.getElementById('ota-error-msg');
              if (errArea) errArea.style.display = '';
              if (errMsgEl) errMsgEl.textContent = errMsg;
            } else {
              /* 更新成功 */
              if (successArea) successArea.style.display = '';
              if (hasFw) {
                fwBar.style.width = '100%';
                fwPct.textContent = '100%';
              }
              if (hasSp && _otaSpiffsSeen) {
                spiffsBar.style.width = '100%';
                spiffsPct.textContent = '100%';
              }
              /* 3秒倒计时自动刷新 */
              var countdown = 3;
              var countdownEl = document.getElementById('ota-countdown');
              var countdownTimer = setInterval(function() {
                countdown--;
                if (countdownEl) countdownEl.textContent = countdown;
                if (countdown <= 0) {
                  clearInterval(countdownTimer);
                  window.location.reload();
                }
              }, 1000);
            }
            return;
          }
          var p = state.otaStatus.progress || 0;
          var msg = state.otaStatus.status_msg;

          if (msg === 'spiffs') {
            _otaSpiffsSeen = true;
            if (hasFw) {
              fwBar.style.width = '100%';
              fwPct.textContent = '100%';
              fwLabel.textContent = t.otaFirmwarePhase;
            }
            if (spiffsRow) spiffsRow.style.display = '';
            spiffsBar.style.width = p + '%';
            spiffsPct.textContent = p + '%';
            spiffsLabel.textContent = t.otaSpiffsPhase;
          } else {
            if (hasFw) {
              fwBar.style.width = p + '%';
              fwPct.textContent = p + '%';
              fwLabel.textContent = t.otaFirmwarePhase;
            }
            if (spiffsRow && hasSp) {
              spiffsRow.style.display = '';
              spiffsBar.style.width = '0%';
              spiffsPct.textContent = t.otaWaiting;
            }
          }
        } catch (e) {
          _otaPollFailCount++;
          // 连续 3 次超时才判定为断连，避免后端写 SPIFFS 时短暂无响应就提前放弃
          if (_otaPollFailCount < 3) return;
          clearInterval(_otaPollTimer);
          _otaPollTimer = null;
          state.otaBusy = false;
          if (hasFw) {
            fwBar.style.width = '100%';
            fwPct.textContent = '100%';
          }
          if (hasSp && _otaSpiffsSeen) {
            spiffsBar.style.width = '100%';
            spiffsPct.textContent = '100%';
          }
          if (infoTable) infoTable.style.display = 'none';
          if (progressArea) progressArea.style.display = 'none';
          successArea.style.display = 'none';
          var errArea = document.getElementById('ota-error-area');
          if (errArea) {
            errArea.style.display = '';
            errArea.textContent = t.otaDisconnected;
          }
          if (cancelBtn) cancelBtn.style.display = 'none';
          updateBtn.style.display = 'none';
          checkBtn.style.display = 'none';
          closeBtn.style.display = '';
          closeBtn.disabled = false;
        }
      }, 1000);
    } catch (e) {
      state.otaBusy = false;
      updateBtn.textContent = t.otaFailed;
      updateBtn.disabled = false;
      closeBtn.disabled = false;
      checkBtn.disabled = false;
    }
  };

  var handleOtaCancel = async function() {
    try { await fetch('/api/ota/cancel', { method: 'POST' }); } catch (e) {}
    if (_otaPollTimer) {
      clearInterval(_otaPollTimer);
      _otaPollTimer = null;
    }
    state.otaBusy = false;
    var progressArea = document.getElementById('ota-progress-area');
    if (progressArea) progressArea.style.display = 'none';
    var updateBtn = document.getElementById('ota-dialog-update');
    var closeBtn = document.getElementById('ota-dialog-close');
    var checkBtn = document.getElementById('ota-dialog-check');
    var cancelBtn = document.getElementById('ota-dialog-cancel');
    updateBtn.textContent = i18n[state.lang].otaUpdateBtn;
    updateBtn.style.display = '';
    updateBtn.disabled = false;
    closeBtn.disabled = false;
    checkBtn.disabled = false;
    if (cancelBtn) cancelBtn.style.display = 'none';
  };

  var handleOtaCheck = async function() {
    var t = i18n[state.lang];
    var checkBtn = document.getElementById('ota-dialog-check');
    var updateBtn = document.getElementById('ota-dialog-update');
    var infoTable = document.getElementById('ota-info-table');
    var curVerEl = document.getElementById('ota-current-ver');
    var newVerRow = document.getElementById('ota-new-ver-row');
    var newVerEl = document.getElementById('ota-new-ver');
    var descRow = document.getElementById('ota-desc-row');
    var descEl = document.getElementById('ota-desc');
    var contentRow = document.getElementById('ota-content-row');
    var contentEl = document.getElementById('ota-content');
    var offlineRow = document.getElementById('ota-offline-row');
    var checkingArea = document.getElementById('ota-checking-area');
    var checkingText = document.getElementById('ota-checking-text');

    checkBtn.disabled = true;
    updateBtn.disabled = true;
    infoTable.style.display = 'none';
    checkingText.textContent = t.otaChecking;
    checkingArea.style.display = '';

    // 重置进度区域
    var progressArea = document.getElementById('ota-progress-area');
    if (progressArea) progressArea.style.display = 'none';

    try {
      var resp = await fetch('/api/ota/check', { method: 'POST' });
      await resp.json();
      await fetchOtaStatus();

      checkingArea.style.display = 'none';

      if (state.otaStatus) {
        offlineRow.style.display = 'none';
        infoTable.style.display = '';
        curVerEl.textContent = state.otaStatus.current_version || '1.0.0';
        newVerRow.style.display = '';
        newVerEl.textContent = state.otaStatus.new_version || state.otaStatus.current_version || '1.0.0';

        if (state.otaStatus.error) {
          descRow.style.display = 'none';
          contentRow.style.display = 'none';
          updateBtn.style.display = 'none';
        } else if (state.otaStatus.update_available) {
          descRow.style.display = state.otaStatus.description ? '' : 'none';
          descEl.innerHTML = (state.otaStatus.description || '').replace(/\\n/g, '<br>');
          contentRow.style.display = '';
          if (state.otaStatus.has_firmware && state.otaStatus.has_spiffs) {
            contentEl.textContent = t.otaContentBoth;
          } else if (state.otaStatus.has_firmware) {
            contentEl.textContent = t.otaContentFirmware;
          } else if (state.otaStatus.has_spiffs) {
            contentEl.textContent = t.otaContentSpiffs;
          } else {
            contentRow.style.display = 'none';
          }
          updateBtn.style.display = '';
          updateBtn.disabled = false;
        } else {
          descRow.style.display = 'none';
          contentRow.style.display = 'none';
          updateBtn.style.display = 'none';
        }
      } else {
        offlineRow.style.display = '';
        offlineRow.textContent = t.otaDisconnected;
        infoTable.style.display = 'none';
        newVerRow.style.display = 'none';
        descRow.style.display = 'none';
        contentRow.style.display = 'none';
        updateBtn.style.display = 'none';
      }
    } catch (e) {
      checkingArea.style.display = 'none';
      offlineRow.style.display = '';
      offlineRow.textContent = t.otaDisconnected;
      infoTable.style.display = 'none';
      newVerRow.style.display = 'none';
      descRow.style.display = 'none';
      contentRow.style.display = 'none';
      updateBtn.style.display = 'none';
    }
    checkBtn.disabled = false;
  };

  // ========== 加载已保存WiFi列表数据 ==========
  var loadSavedWifi = async function() {
    // 文件/OTA 操作进行中：跳过轮询，避免带宽竞争
    if (state.fileBusy || state.otaBusy) return;
    var t = i18n[state.lang];

    try {
      var ctrl = new AbortController();
      var timer = setTimeout(function() { ctrl.abort(); }, 3000);
      var response = await fetch('/api/wifi/saved', { signal: ctrl.signal });
      clearTimeout(timer);

      if (!response.ok) {
        throw new Error('Network response was not ok');
      }

      var data = await response.json();

      if (!data || !Array.isArray(data.items)) {
        throw new Error('Invalid response');
      }

      state.savedWifiList = data.items;
      renderSavedWifi();
    } catch (e) {
      // 失败时保留旧列表，不闪现"暂无已保存WiFi"
    }
  };

  // ========== 连接已保存的WiFi ==========
  // 先暴露函数到全局，确保HTML的onclick可以调用
  window.connectSavedWifi = function(ssid) {
    console.log('connectSavedWifi called with ssid:', ssid);
    console.log('Current wifiStatus:', state.wifiStatus);
    
    // 检查当前状态，如果是STA模式且已连接，显示确认对话框
    if (state.wifiStatus && state.wifiStatus.connected && state.wifiStatus.mode === 'STA') {
      console.log('Showing confirm dialog');
      showConfirmDialog(function(confirmed) {
        if (confirmed) {
          doConnectSavedWifi(ssid);
        }
      });
    } else {
      console.log('Skipping confirm dialog, connecting directly');
      doConnectSavedWifi(ssid);
    }
  };

  var doConnectSavedWifi = async function(ssid) {
    state.status = 1;
    state.isConnecting = true;
    state.targetSsid = ssid;
    updateUI();

    try {
      var ctrl = new AbortController();
      var timer = setTimeout(function() { ctrl.abort(); }, 15000);
      await fetch('/api/wifi/saved/connect?ssid=' + encodeURIComponent(ssid), { method: 'POST', signal: ctrl.signal });
      clearTimeout(timer);
    } catch (e) {
      // POST可能超时但ESP32已收到请求并开始连接，不直接标记失败
    }

    // 无论POST结果如何，始终通过轮询获取真实连接状态
    pollConnectionStatus();
  };

  // ========== 删除已保存的WiFi ==========
  window.deleteSavedWifi = function(ssid) {
    showConfirmDialog(function(confirmed) {
      if (confirmed) {
        doDeleteSavedWifi(ssid);
      }
    }, null, 'confirmDeleteMessage');
  };

  var doDeleteSavedWifi = async function(ssid) {
    var t = i18n[state.lang];
    try {
      var response = await fetch('/api/wifi/saved/delete?ssid=' + encodeURIComponent(ssid), { method: 'POST' });
      if (response.ok) {
        // 删除成功，重新加载列表
        loadSavedWifi();
      }
    } catch (e) {
      console.error('Failed to delete saved WiFi:', e);
    }
  };

  // ========== 开发者模式 ==========

  /** @brief Logo 3 秒内连击 10 次进入/退出开发者模式 */
  var devClickCount = 0;
  var devClickTimer = null;

  var initDevMode = function() {
    // Logo 点击计数
    $('logo-wrapper').addEventListener('click', function(e) {

      // 锁定模式或断网模式下禁用开发者模式入口（Logo 连击无效）
      if (_isLockingMode(state.usbMode) || state.offline) {
        devClickCount = 0;
        if (devClickTimer) { clearTimeout(devClickTimer); devClickTimer = null; }
        return;
      }

      // 第一次点击时启动 3 秒计时器
      if (devClickCount === 0) {
        devClickTimer = setTimeout(function() {
          devClickCount = 0;
          devClickTimer = null;
        }, 3000);
      }

      devClickCount++;

      if (devClickCount >= 10) {
        clearTimeout(devClickTimer);
        devClickTimer = null;
        devClickCount = 0;
        state.devMode = !state.devMode;
        localStorage.setItem('devMode', state.devMode ? '1' : '0');
        applyDevMode();
      }
    });

    applyDevMode();
  };

  var applyDevMode = function() {
    var badge = $('dev-badge');
    var devView = $('dev-view');

    // 锁定模式或断网模式下强制隐藏开发者模式 UI
    var canShow = state.devMode && !_isLockingMode(state.usbMode) && !state.offline;

    if (canShow) {
      badge.classList.add('visible');
      badge.classList.remove('hidden');  // 清除遗留的 !important hidden
      devView.classList.remove('hidden');
      loadOtaConfig();
    } else {
      badge.classList.remove('visible');
      // 不添加 hidden class（由 CSS .dev-badge { display:none } 保证隐藏），
      // 避免 .hidden { display:none !important } 覆盖 .visible { display:block }
      devView.classList.add('hidden');
    }
  };

  // ========== OTA 配置管理 ==========

  var loadOtaConfig = function() {
    fetch('/api/ota/config')
      .then(function(r) { return r.json(); })
      .then(function(data) {
        if (data.version_url) {
          $('ota-url-input').value = data.is_default ? '' : data.version_url;
          $('btn-ota-url').textContent = data.is_default
            ? '已加载默认服务器地址'
            : data.version_url;
        }
      })
      .catch(function() {});
  };

  var showOtaUrlDialog = function() {
    var dialog = $('ota-url-dialog');
    dialog.classList.add('show');
    $('ota-url-input').focus();
    $('ota-url-input').select();
  };

  var closeOtaUrlDialog = function() {
    $('ota-url-dialog').classList.remove('show');
  };

  // ========== 初始化 ==========
  var switchPage;  // 提升到 IIFE 作用域
  document.addEventListener('DOMContentLoaded', async function() {
    // 启动时持久化主题，供 iframe（如 NCM）预加载时从 localStorage 读取
    localStorage.setItem('theme', state.theme);

    // ============================================================
    // 从 PAGES_CONFIG（pages.js）读取所有功能页面并动态生成
    // 自动创建侧边栏按钮 + iframe 容器 + 注册 USB 模式映射
    // 所有功能页面完全由 pages.js 动态发现和生成
    // ============================================================
    var loadDynamicPages = function() {
      try {
        var config = (typeof PAGES_CONFIG !== 'undefined') ? PAGES_CONFIG : { lockingPages: [], pages: [] };
        var allPages = Array.isArray(config.pages) ? config.pages : [];

        // --- 收集 pages.js 中注册的所有页面 ID ---
        // 用于判断"当前页是否属于 pages.js 注册页"，决定模式变化时是否自动跳转
        // 以及 NCM 等锁定模式下哪些按钮需要禁用
        _ALL_REGISTERED_PAGE_IDS = {};
        allPages.forEach(function(p) {
          if (p && p.id) _ALL_REGISTERED_PAGE_IDS[p.id] = true;
        });

        _LOCKING_PAGE_IDS = {};
        var lockingPagesArr = Array.isArray(config.lockingPages) ? config.lockingPages : [];
        lockingPagesArr.forEach(function(id) { _LOCKING_PAGE_IDS[id] = true; });

        // --- 分组：按是否声明了 usbMode 分开 ---
        // usbPages:   需要特定 USB 模式才能访问的功能（文件管理、键鼠、无线网卡等）
        // otherPages: 通用功能，任何模式下可访问（系统信息、固件更新等）
        var usbPages = [];
        var otherPages = [];
        allPages.forEach(function(p) {
          if (!p || !p.id || typeof p.entry !== 'string') return;
          if (typeof p.usbMode === 'number') usbPages.push(p);
          else otherPages.push(p);
        });

        // --- 构建单个侧边栏按钮 ---
        var buildButton = function(page) {
          PAGE_ENTRY[page.id] = page.entry;

          var btn = document.createElement('button');
          btn.className = 'sidebar-btn';
          btn.dataset.page = page.id;
          if (page.usbMode !== undefined && page.usbMode !== null) {
            btn.dataset.usbMode = page.usbMode;
          }

          var iconHtml = page.icon || '';
          if (iconHtml) {
            var iconWrapper = document.createElement('span');
            iconWrapper.innerHTML = iconHtml;
            var svgEl = iconWrapper.firstElementChild;
            if (svgEl) btn.appendChild(svgEl);
          }

          var btnLabel = document.createElement('span');
          if (page.labelI18n && typeof page.labelI18n === 'object') {
            btnLabel.dataset.labelI18n = JSON.stringify(page.labelI18n);
            btnLabel.textContent = page.labelI18n[state.lang] || page.labelI18n['en'] || page.id;
          } else {
            btnLabel.dataset.labelKey = page.label || '';
            btnLabel.dataset.labelFallback = page.labelEn || page.label || page.id;
            if (page.label && i18n[state.lang] && i18n[state.lang][page.label]) {
              btnLabel.textContent = i18n[state.lang][page.label];
            } else if (state.lang === 'en' && page.labelEn) {
              btnLabel.textContent = page.labelEn;
            } else {
              btnLabel.textContent = page.label || page.id;
            }
          }
          btn.appendChild(btnLabel);

          var modeInd = document.createElement('span');
          modeInd.className = 'mode-indicator';
          modeInd.innerHTML = '<svg viewBox="0 0 1024 1024"><path d="M512 0C229.696 0 0 229.696 0 512s229.696 512 512 512 512-229.696 512-512-229.696-512-512-512z m0 960c-247.04 0-448-201.024-448-448 0-247.04 200.96-448 448-448 246.976 0 448 200.96 448 448 0 246.976-201.024 448-448 448z"/><path d="M512 512m-288 0a288 288 0 1 0 576 0 288 288 0 1 0-576 0Z"/></svg>';
          btn.appendChild(modeInd);
          btn.onclick = function() { switchPage(page.id); };
          return btn;
        };

        // --- 构建 iframe 容器（通用，两组都需要）---
        var buildIframe = function(page) {
          var iframePage = document.createElement('div');
          iframePage.id = 'page-' + page.id;
          iframePage.className = 'page hidden';
          iframePage.setAttribute('data-page-container', 'true');
          var iframe = document.createElement('iframe');
          iframe.id = 'page-iframe-' + page.id;
          iframe.setAttribute('data-entry', page.entry);
          iframePage.appendChild(iframe);
          var mainEl = document.querySelector('main.main');
          if (mainEl) mainEl.appendChild(iframePage);
          else document.body.appendChild(iframePage);

          if (typeof page.usbMode === 'number') {
            _DYNAMIC_PAGE_MODES[page.id] = page.usbMode;
          }
        };

        // --- 构建分组标题（标题自身通过 ::before/::after 绘制左右分隔线，形如：—— 标题 ——）---
        var buildSectionTitle = function(labelZh, labelEn) {
          var title = document.createElement('div');
          title.className = 'sidebar-section-title';
          title.dataset.labelZh = labelZh;
          title.dataset.labelEn = labelEn;
          title.textContent = state.lang === 'en' ? labelEn : labelZh;
          return title;
        };

        var sidebarItem = document.querySelector('.sidebar-item');
        var fallbackContainer = document.querySelector('aside');

        // 第一组：USB 功能页
        if (usbPages.length > 0) {
          sidebarItem.appendChild(buildSectionTitle('USB功能', 'USB Functions'));
          usbPages.forEach(function(page) {
            sidebarItem.appendChild(buildButton(page));
            buildIframe(page);
          });
        }

        // 第二组：其他功能页（仅在有页面时才显示，不显示标题）
        if (otherPages.length > 0) {
          sidebarItem.appendChild(buildSectionTitle('其他功能', 'Other Functions'));
          otherPages.forEach(function(page) {
            sidebarItem.appendChild(buildButton(page));
            buildIframe(page);
          });
        }
      } catch (e) { console.error('loadDynamicPages failed:', e); }
    };
    loadDynamicPages();

    // 预加载锁定模式页的 iframe（如 ncm），确保切换到对应 USB 模式时即时显示
    Object.keys(_LOCKING_PAGE_IDS).forEach(function(pageId) {
      var iframe = document.getElementById('page-iframe-' + pageId);
      if (iframe && !iframe.src && iframe.dataset.entry) {
        iframe.src = '/' + iframe.dataset.entry;
      }
    });

    var sidebar = $('sidebar');
    var drawerHandle = $('drawer-handle');

    // 桌面端默认展开侧边栏
    if (window.innerWidth >= 768) {
      sidebar.classList.add('open');
      drawerHandle.textContent = '◀';
    }

    // 侧边栏切换
    var toggleSidebar = function() {
      var isOpen = sidebar.classList.toggle('open');
      drawerHandle.textContent = isOpen ? '◀' : '▶';
    };
    drawerHandle.onclick = toggleSidebar;

    // 页面切换
    switchPage = function(pageId, force) {
      var displayPageId = pageId;

      // --- 动态发现页面的 USB 模式检查 ---
      // 动态页面通过 pages.js 的 usbMode 字段声明所需模式
      // mode 为 null / undefined 时不检查（可在任何模式下访问）
      if (typeof _DYNAMIC_PAGE_MODES === 'object' && _DYNAMIC_PAGE_MODES &&
          _DYNAMIC_PAGE_MODES[pageId] !== undefined) {
        var requiredMode = _DYNAMIC_PAGE_MODES[pageId];
        if (requiredMode !== null && state.usbMode !== requiredMode) {
          // 锁定模式（如 NCM）：立即切页 + 后台 POST，不等确认
          if (_isLockingMode(requiredMode)) {
            _enterLockingMode(requiredMode, pageId);
            return;
          }
          switchUsbMode(requiredMode, function() { switchPage(pageId); });
          return;
        }
      }

      state.currentPage = pageId;

      // 隐藏所有静态的 network-page / other-page
      document.getElementById('network-page').classList.add('hidden');
      document.getElementById('other-page').classList.add('hidden');

      // 隐藏所有动态生成的页面容器（data-page-container=true）
      var dynamicPages = document.querySelectorAll('[data-page-container="true"]');
      dynamicPages.forEach(function(el) {
        el.classList.add('hidden');
      });

      // 显示目标页面：优先查找动态容器，再回退 {pageId}-page
      var showEl = document.getElementById('page-' + displayPageId);
      if (!showEl) {
        showEl = document.getElementById(displayPageId + '-page');
      }
      if (showEl) {
        showEl.classList.remove('hidden');
        // 懒加载 iframe：首次切到该页时才设 src，避免首屏并发加载所有功能页
        var iframe = showEl.querySelector('iframe');
        if (iframe && !iframe.src && iframe.dataset.entry) {
          iframe.src = '/' + iframe.dataset.entry;
        }
      }

      $('offline-overlay').classList.add('hidden');
      // version-view（系统管理）: 仅在非锁定模式下、且在 other 页时显示（断网模式也隐藏）
      var hideVersionView = (displayPageId !== 'other') || _isLockingMode(state.usbMode) || state.offline;
      $('version-view').style.display = hideVersionView ? 'none' : '';

      // 网络页面显示时同步断网/锁定模式内容切换
      if (displayPageId === 'network') {
        _applyNetworkPage();
      }

      // 侧边栏高亮: 高亮用户实际点击的页面
      var highlightPageId = pageId;
      var buttons = document.querySelectorAll('.sidebar-btn[data-page]');
      buttons.forEach(function(btn) {
        if (btn.dataset.page === highlightPageId) {
          btn.classList.add('active');
        } else {
          btn.classList.remove('active');
        }
        // 触控设备上 :active 伪类可能残留，主动 blur 清除
        btn.blur();
      });
    };

    // USB 模式轮询（其他设备切模式时自动跳转）
    var fetchUsbMode = async function() {
      if (state.fileBusy || state.otaBusy) return;
      try {
        var ctrl = new AbortController();
        var timer = setTimeout(function() { ctrl.abort(); }, 5000);
        var response = await fetch('/api/usb/status', { signal: ctrl.signal });
        clearTimeout(timer);
        var data = await response.json();
        if (data && typeof data.mode === 'number') {
          var oldMode = state.usbMode;
          // pending_mode：物理按钮切换时暂存
          // - -1：无待处理切换，直接同步 usbMode
          // - locking mode：暂存起来，等断网后自动进入该模式
          if (data.pending_mode === -1) {
            _pendingLockingMode = -1;
            state.usbMode = data.mode;
            // 锁定模式从断网中恢复时，退出离线模式（物理按钮竞态场景）
            if (state.offline && _isLockingMode(data.mode)) {
              exitOffline();
            }
          } else if (_isLockingMode(data.pending_mode)) {
            _pendingLockingMode = data.pending_mode;
          }
          updateUsbModeUI();
          // 非 locking mode 间的切换（MSC ↔ HID）：自动跳转
          // 跳过用户刚点击锁定模式后的轮询反馈（避免 state.usbMode 被覆盖切回）
          if (_lockTransitionPending) {
            _lockTransitionPending = false;
          } else if (typeof oldMode === 'number' && oldMode !== data.mode && data.pending_mode === -1) {
            var mappedPage = _findPageByUsbMode(data.mode);
            if (mappedPage && state.currentPage in _ALL_REGISTERED_PAGE_IDS) {
              switchPage(mappedPage);
            }
          }
        }
      } catch (e) {
        // 进入锁定模式后断网（物理按键 / UI 按钮已发 POST，网络切换中）
        // - UI 按钮：_enterLockingMode 已立即切页，此处只是冗余加固
        // - 物理按键：读到 pending_mode 后断网，确认 NCM 已生效，切页
        if (_pendingLockingMode !== -1) {
          var lockMode = _pendingLockingMode;
          _pendingLockingMode = -1;
          state.usbMode = lockMode;
          updateUsbModeUI();
          var lockPage = _findPageByUsbMode(lockMode);
          if (lockPage) switchPage(lockPage, true);
        }
      }
    };

    // 绑定侧边栏按钮点击事件
    var sidebarButtons = document.querySelectorAll('.sidebar-btn[data-page]');
    sidebarButtons.forEach(function(btn) {
      btn.onclick = function() {
        switchPage(this.dataset.page);
        // 不自动收缩侧边栏，用户可手动控制
      };
    });

    // 主题切换
    $('btn-theme').onclick = function() {
      state.theme = state.theme === 'dark' ? 'light' : 'dark';
      localStorage.setItem('theme', state.theme);
      // 实时通知其他页面
      try {
        var bc = new BroadcastChannel('superdongle_theme');
        bc.postMessage(state.theme);
        bc.close();
      } catch(e) {}
      updateUI();
    };

    // 语言切换
    $('btn-lang').onclick = function() {
      state.lang = state.lang === 'zh' ? 'en' : 'zh';
      localStorage.setItem('lang', state.lang);
      try {
        var bc = new BroadcastChannel('superdongle_lang');
        bc.postMessage(state.lang);
        bc.close();
      } catch(e) {}
      updateUI();
    };

    // 动态生成的 USB 功能按钮已在 loadDynamicPages 内绑定 onclick（通用 switchPage 调用）
    // switchPage 内部会根据 _DYNAMIC_PAGE_MODES 检查是否需要切换 USB 模式

    // LED 开关切换
    $('led-toggle').onchange = function() {
      var enabled = this.checked;
      fetch('/api/settings/led/save', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ enabled: enabled })
      }).catch(function() {});
    };

    // 加载 LED 初始状态
    fetch('/api/settings/led/state')
      .then(function(r) { return r.json(); })
      .then(function(data) {
        $('led-toggle').checked = data.enabled;
      })
      .catch(function() {});

    // mDNS 设置
    $('btn-mdns').onclick = function() {
      var t = i18n[state.lang];
      var current = $('btn-mdns').textContent.replace('.local', '');
      mdnsDialogTitle.textContent = t.mdnsDialogTitle;
      mdnsDialogHint.textContent = t.mdnsDialogHint;
      mdnsInput.placeholder = t.mdnsPlaceholder;
      mdnsDialogOk.textContent = t.mdnsSave;
      mdnsDialogCancel.textContent = t.cancel;
      mdnsInput.value = current;
      mdnsInput.focus();
      mdnsInput.select();
      mdnsDialog.classList.add('show');
    };

    // WiFi列表点击 (事件委托)
    $('wifi-list').addEventListener('click', function(e) {
      var item = e.target.closest('.wifi-item');
      if (!item) return;

      var items = $('wifi-list').querySelectorAll('.wifi-item');
      for (var i = 0; i < items.length; i++) {
        items[i].classList.remove('selected');
      }
      item.classList.add('selected');
      var ssid = item.dataset.ssid;
      var encrypted = item.dataset.encrypted === 'true';
      $('input-ssid').value = ssid;
      $('input-pass').value = ''; // 清空密码
      state.status = 0; // 重置状态
      updateUI();
      state.selectedNetwork = { ssid: ssid, encrypted: encrypted };
      // 不自动聚焦输入框，避免手机弹出键盘
    });

    // 表单提交
    $('config-form').onsubmit = function(e) {
      e.preventDefault();
      var t = i18n[state.lang];
      var ssid = $('input-ssid').value.trim();
      var pass = $('input-pass').value;

      // 查找当前输入SSID的加密状态
      var networks = state.originalNetworks || [];
      var targetNetwork = null;
      for (var i = 0; i < networks.length; i++) {
        if (networks[i].ssid === ssid) {
          targetNetwork = networks[i];
          break;
        }
      }

      // SSID必须在扫描列表中
      if (!targetNetwork) {
        var ssidInput = $('input-ssid');
        ssidInput.value = '';
        ssidInput.placeholder = t.wifiNotFound;
        ssidInput.classList.add('error');
        ssidInput.focus();
        return;
      }

      // 密码验证：加密网络密码不能为空且至少8位；开放网络清空密码
      if (targetNetwork.auth > 0) {
        if (pass.length === 0 || pass.length < 8) {
          var passInput = $('input-pass');
          passInput.value = '';
          passInput.placeholder = t.passMinLen;
          passInput.classList.add('error');
          passInput.focus();
          return;
        }
      } else {
        // 开放网络：清空密码
        $('input-pass').value = '';
      }

      var passInput = $('input-pass');
      passInput.placeholder = '密码';
      passInput.classList.remove('error');

      if (ssid) {
        // 检查当前状态，如果是STA模式且已连接，显示确认对话框
        if (state.wifiStatus && state.wifiStatus.connected && state.wifiStatus.mode === 'STA') {
          showConfirmDialog(function(confirmed) {
            if (confirmed) {
              saveConfig(ssid, pass);
            }
          });
        } else {
          saveConfig(ssid, pass);
        }
      }
    };

    // 当用户开始输入密码时，重置placeholder和颜色
    $('input-pass').addEventListener('input', function() {
      var t = i18n[state.lang];
      if (this.placeholder === t.passMinLen) {
        this.placeholder = t.passPlaceholder;
        this.classList.remove('error');
      }
    });

    // 当用户开始输入SSID时，重置placeholder和颜色
    $('input-ssid').addEventListener('input', function() {
      var t = i18n[state.lang];
      if (this.placeholder === t.wifiNotFound) {
        this.placeholder = t.ssidPlaceholder;
        this.classList.remove('error');
      }
    });

    // 重新扫描
    $('btn-rescan').onclick = scanWiFi;

    // 密码显示/隐藏
    $('pass-toggle').onclick = function() {
      var input = $('input-pass');
      var isPassword = input.type === 'password';
      input.type = isPassword ? 'text' : 'password';
      $('icon-eye').classList.toggle('hidden', !isPassword);
      $('icon-eye-off').classList.toggle('hidden', isPassword);
    };
    
    // 初始化确认对话框元素引用
    confirmDialog = $('confirm-dialog');
    confirmTitle = $('confirm-title');
    confirmMessage = $('confirm-message');
    confirmOk = $('confirm-ok');
    confirmCancel = $('confirm-cancel');
    
    // 确认对话框按钮事件
    confirmOk.onclick = function() {
      if (confirmCallback) {
        confirmCallback(true);
      }
      hideConfirmDialog();
    };
    
    confirmCancel.onclick = function() {
      if (confirmCallback) {
        confirmCallback(false);
      }
      hideConfirmDialog();
    };

    // 初始化连接成功弹窗元素引用
    successDialog = $('success-dialog');
    successTitle = $('success-title');
    successHint = $('success-hint');
    successIpLabel = $('success-ip-label');
    successIpValue = $('success-ip-value');
    successMdnsLabel = $('success-mdns-label');
    successMdnsValue = $('success-mdns-value');
    successOk = $('success-ok');

    // 连接成功弹窗确定按钮
    successOk.onclick = hideSuccessDialog;

    // 初始化 mDNS 编辑弹窗
    mdnsDialog = $('mdns-dialog');
    mdnsDialogTitle = $('mdns-dialog-title');
    mdnsDialogHint = $('mdns-dialog-hint');
    mdnsInput = $('mdns-input');
    mdnsDialogOk = $('mdns-dialog-ok');
    mdnsDialogCancel = $('mdns-dialog-cancel');

    mdnsDialogOk.onclick = function() {
      var newName = mdnsInput.value.trim();
      if (!newName || newName.length < 1) return;
      mdnsDialog.classList.remove('show');
      fetch('/api/settings/mdns/save', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ hostname: newName })
      }).then(function(r) { return r.json(); })
        .then(function(data) {
          if (data.ok) {
            $('btn-mdns').textContent = newName + '.local';
            showRebootNotification('mDNS 地址已保存，设备正在重启...', null, 'http://' + newName + '.local/');
          }
        })
        .catch(function() { showRebootDialog('保存失败，请重试。'); });
    };

    mdnsDialogCancel.onclick = function() {
      mdnsDialog.classList.remove('show');
    };
    mdnsDialog.addEventListener('click', function(e) {
      if (e.target === mdnsDialog) mdnsDialog.classList.remove('show');
    });

    // Enter 提交 / ESC 关闭
    mdnsInput.addEventListener('keydown', function(e) {
      if (e.key === 'Enter') mdnsDialogOk.click();
      if (e.key === 'Escape') mdnsDialogCancel.click();
    });

    // OTA 对话框事件
    $('ota-dialog-close').onclick = closeOtaDialog;
    $('ota-dialog-check').onclick = handleOtaCheck;
    $('ota-dialog-update').onclick = handleOtaUpdate;
    $('ota-dialog-cancel').onclick = handleOtaCancel;
    document.getElementById('ota-dialog').addEventListener('click', function(e) {
      if (e.target === this) closeOtaDialog();
    });

    // 监听文件页面（iframe）的 postMessage：文件操作忙/空闲状态
    window.addEventListener('message', function(e) {
      if (e.data && e.data.type === 'fileBusy') {
        state.fileBusy = !!e.data.busy;
      }
    });

    // 初始化开发者模式（Logo 连击 10 次）
    initDevMode();

    // OTA URL 配置按钮 → 弹出对话框
    $('btn-ota-url').onclick = showOtaUrlDialog;

    // OTA URL 对话框事件
    $('ota-url-dialog-cancel').onclick = closeOtaUrlDialog;
    $('ota-url-dialog').addEventListener('click', function(e) {
      if (e.target === this) closeOtaUrlDialog();
    });
    $('ota-url-input').addEventListener('keydown', function(e) {
      if (e.key === 'Enter') $('ota-url-dialog-save').click();
      if (e.key === 'Escape') closeOtaUrlDialog();
    });
    $('ota-url-dialog-save').onclick = function() {
      var url = $('ota-url-input').value.trim();
      fetch('/api/ota/config/set', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ version_url: url })
      }).then(function(r) { return r.json(); })
        .then(function(data) {
          if (data.ok && data.rebooting) {
            closeOtaUrlDialog();
            showRebootNotification(url
              ? 'OTA 地址已保存，设备正在重启...'
              : '已恢复默认地址，设备正在重启...', 'OTA Server URL');
          }
        })
        .catch(function() { showRebootDialog('保存失败，请检查网络。'); });
    };

    // ========== 重启设备弹窗 ==========

    var showRebootDialog = function(msg) {
      var t = i18n[state.lang];
      $('reboot-title').textContent = '重启设备';
      $('reboot-dialog-msg').textContent = msg;
      $('reboot-dialog-cancel').textContent = t.cancel;
      $('reboot-dialog-confirm').textContent = t.confirm;
      $('reboot-dialog').classList.add('show');
      $('reboot-dialog-cancel').style.display = '';
      $('reboot-dialog-confirm').style.display = '';
      $('reboot-dialog-confirm').disabled = false;
    };

    // 仅通知 + 自动倒计时，无按钮（已确认过的情况下使用）
    var _rebootTimer = null;
    var showRebootNotification = function(msg, title, redirectUrl) {
      if (_rebootTimer) {
        clearInterval(_rebootTimer);
        _rebootTimer = null;
      }
      $('reboot-title').textContent = title || '重启设备';
      $('reboot-dialog-msg').textContent = msg;
      $('reboot-dialog').classList.add('show');
      $('reboot-dialog-cancel').style.display = 'none';
      $('reboot-dialog-confirm').style.display = 'none';
      var count = 3;
      _rebootTimer = setInterval(function() {
        if (count <= 0) {
          clearInterval(_rebootTimer);
          _rebootTimer = null;
          if (redirectUrl) {
            window.location.href = redirectUrl;
          } else {
            window.location.reload();
          }
          return;
        }
        var t = i18n[state.lang];
        $('reboot-dialog-msg').textContent =
          (redirectUrl ? '设备正在重启，' + count + ' 秒后跳转至新地址...'
                      : '设备正在重启，' + count + ' 秒后自动刷新...');
        count--;
      }, 1000);
    };

    $('reboot-dialog-cancel').onclick = function() {
      $('reboot-dialog').classList.remove('show');
    };
    $('reboot-dialog').addEventListener('click', function(e) {
      if (e.target === this) $('reboot-dialog').classList.remove('show');
    });

    // 退出开发者模式
    $('btn-dev-exit').onclick = function() {
      state.devMode = false;
      localStorage.removeItem('devMode');
      applyDevMode();
    };

    // 重启设备
    $('btn-dev-reboot').onclick = function() {
      showRebootDialog('确认重启设备？');
    };
    $('reboot-dialog-confirm').onclick = function() {
      fetch('/api/system/reboot', { method: 'POST' })
        .then(function() { showRebootNotification('设备正在重启...'); })
        .catch(function() { showRebootDialog('重启失败，请重试。'); });
    };

    // ========== 恢复出厂设置弹窗 ==========
    window.factoryResetTimer = null;
    window.showFactoryResetDialog = function() {
      var t = i18n[state.lang];
      
      // 更新弹窗文字
      $('factory-reset-title').textContent = t.factoryResetTitle;
      $('factory-reset-message').textContent = t.factoryResetMessage;
      $('factory-reset-dialog-cancel').textContent = t.factoryResetCancel;
      
      $('factory-reset-dialog').classList.add('show');
      var btn = $('factory-reset-dialog-confirm');
      btn.disabled = true;
      
      // 清除之前的计时器（如果存在）
      if (window.factoryResetTimer) {
        clearInterval(window.factoryResetTimer);
      }
      
      var count = 5;
      btn.textContent = t.factoryResetConfirmCountdown.replace('{count}', count);
      
      window.factoryResetTimer = setInterval(function() {
        var t2 = i18n[state.lang];
        count--;
        if (count <= 0) {
          clearInterval(window.factoryResetTimer);
          window.factoryResetTimer = null;
          btn.disabled = false;
          btn.textContent = t2.factoryResetConfirm;
        } else {
          btn.textContent = t2.factoryResetConfirmCountdown.replace('{count}', count);
        }
      }, 1000);
    };

    $('factory-reset-dialog-cancel').onclick = function() {
      $('factory-reset-dialog').classList.remove('show');
      if (window.factoryResetTimer) {
        clearInterval(window.factoryResetTimer);
        window.factoryResetTimer = null;
      }
    };
    $('factory-reset-dialog').addEventListener('click', function(e) {
      if (window._factoryResetLocked) return;
      if (e.target === this) {
        $('factory-reset-dialog').classList.remove('show');
        if (window.factoryResetTimer) {
          clearInterval(window.factoryResetTimer);
          window.factoryResetTimer = null;
        }
      }
    });

    $('factory-reset-dialog-confirm').onclick = function() {
      var t = i18n[state.lang];
      var btn = this;
      btn.disabled = true;
      btn.textContent = t.factoryResetting;
      
      fetch('/api/system/factory_reset', { method: 'POST' })
        .then(function() {
          $('factory-reset-message').textContent = t.factoryResetDone;
          // 隐藏确认/取消按钮，锁定弹窗不可通过点击空白处关闭
          $('factory-reset-dialog-cancel').style.display = 'none';
          $('factory-reset-dialog-confirm').style.display = 'none';
          window._factoryResetLocked = true;
        })
        .catch(function() {
          var t2 = i18n[state.lang];
          btn.disabled = false;
          btn.textContent = t2.factoryResetConfirm;
        });
    };

    updateUI();

    // 先获取WiFi状态，成功后再扫描和加载列表，避免同时发起多个请求
    getWifiStatus().then(function() {
      renderWifiStatus();
      initMdnsSettings();
      fetchOtaStatus();
      fetchUsbMode();
      loadSavedWifi();
      setTimeout(scanWiFi, 500);
    }).catch(function() {});

    // 每2秒自动更新WiFi状态和已保存WiFi列表（错开1秒避免同时超时）
    setInterval(fetchWifiStatus, 2000);
    setTimeout(function() { setInterval(loadSavedWifi, 2000); }, 1000);

    // OTA状态在页面加载时读取一次，延迟3秒再读一次以覆盖检查未完成的情况
    setTimeout(fetchOtaStatus, 3000);

    // 每2秒检查USB模式变化（与按钮2s预通知配合，提前感知NCM切换）
    fetchUsbMode();
    setInterval(fetchUsbMode, 2000);

    // 触控设备上 :active 伪类可能残留，pointerup 时强制 reflow 清除
    document.addEventListener('pointerup', function() {
      void document.body.offsetHeight;
    }, { passive: true });

    // 事件委托：处理已保存WiFi列表的按钮点击
    $('saved-list').addEventListener('click', function(e) {
      console.log('saved-list click event triggered');
      var target = e.target;
      
      // 查找点击的按钮
      if (target.tagName === 'BUTTON' && target.dataset.action) {
        var wifiItem = target.closest('.wifi-item');
        if (!wifiItem) return;
        
        var ssid = wifiItem.dataset.ssid;
        var action = target.dataset.action;
        
        console.log('Action:', action, 'SSID:', ssid);
        
        if (action === 'connect') {
          connectSavedWifi(ssid);
        } else if (action === 'delete') {
          deleteSavedWifi(ssid);
        }
      }
    });
  });
})();