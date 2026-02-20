/* WiFiManagerLite - Advanced JavaScript (Captive Portal Compatible) */
/* Native form submit - JS only for config loading and status polling */

// Config Loading (pre-fill form fields)
function loadConfig() {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', '/wml/config', true);
  xhr.onreadystatechange = function() {
    if (xhr.readyState !== 4 || xhr.status !== 200) return;
    
    try {
      var cfg = JSON.parse(xhr.responseText);
      
      var setVal = function(id, val) {
        var el = document.getElementById(id);
      if (el) el.value = val || '';
    };
      var setChk = function(id, val) {
        var el = document.getElementById(id);
      if (el) el.checked = !!val;
    };

    setVal('devicename', cfg.deviceName);
    setVal('ssid0', cfg.ssid0);
    setVal('password0', cfg.pass0);
    setVal('bssid0', cfg.bssid0);
    setChk('bssidLock', cfg.bssidLock);
    setVal('ssid1', cfg.ssid1);
    setVal('password1', cfg.pass1);
    setVal('ip', cfg.ip);
    setVal('subnet', cfg.subnet);
    setVal('gateway', cfg.gateway);
    setVal('dns', cfg.dns);
  } catch (e) {
    console.error('Failed to load config:', e);
  }
  };
  xhr.send();
}

// Status Page Polling
var statusInterval = null;

function loadStatus() {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', '/wml/status.json', true);
  xhr.onreadystatechange = function() {
    if (xhr.readyState !== 4 || xhr.status !== 200) return;
    
    try {
      var s = JSON.parse(xhr.responseText);

      var setStatus = function(id, val) {
        var el = document.getElementById(id);
      if (el) el.textContent = val;
    };

      // Update connection indicator
      var indicator = document.getElementById('connection-indicator');
      if (indicator) {
        indicator.className = 'indicator ' + (s.connected ? 'connected' : 'disconnected');
        indicator.textContent = s.connected ? 'Connected' : 'Disconnected';
      }

      // Update device name in header
      var deviceName = document.getElementById('device-name');
      if (deviceName && s.hostname) {
        deviceName.textContent = s.hostname;
      }

      setStatus('status-ssid', s.ssid || '—');
      setStatus('status-ip', s.ip || '—');
      setStatus('status-rssi', s.rssi ? s.rssi + ' dBm' : '—');
      setStatus('status-mac', s.mac || '—');
      setStatus('status-hostname', s.hostname || '—');
    setStatus('status-uptime', formatUptime(s.uptime || 0));
    setStatus('status-heap', Math.round((s.heap || 0) / 1024) + ' KB');
      setStatus('status-version', s.version || '—');
  } catch (e) {
    console.error('Failed to load status:', e);
  }
  };
  xhr.send();
}

function formatUptime(seconds) {
  var d = Math.floor(seconds / 86400);
  var h = Math.floor((seconds % 86400) / 3600);
  var m = Math.floor((seconds % 3600) / 60);
  var sec = seconds % 60;

  if (d > 0) return d + 'd ' + h + 'h ' + m + 'm';
  if (h > 0) return h + 'h ' + m + 'm ' + sec + 's';
  if (m > 0) return m + 'm ' + sec + 's';
  return sec + 's';
}

// Initialize advanced features on page load
document.addEventListener('DOMContentLoaded', function() {
  // Setup page - load config to pre-fill form
  if (document.getElementById('wifiForm')) {
    loadConfig();
  }

  // Status page - start polling
  if (document.getElementById('connection-indicator')) {
    loadStatus();
    statusInterval = setInterval(loadStatus, 5000);
  }
});

// Cleanup on page unload
window.addEventListener('beforeunload', function() {
  if (statusInterval) clearInterval(statusInterval);
});
