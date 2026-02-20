/* WiFiManagerLite - Common JavaScript (Captive Portal Compatible) */
/* No async/await, no fetch() - uses XMLHttpRequest for compatibility */

// WiFi Signal Strength to CSS class
function getSignalClass(rssi) {
  if (rssi >= -50) return 'strength-100';
  if (rssi >= -60) return 'strength-75';
  if (rssi >= -70) return 'strength-50';
  return 'strength-25';
}

// WiFi Signal Bars HTML Generator
function signalHTML(rssi) {
  var cls = getSignalClass(rssi);
  return '<div class="wifi-signal ' + cls + '">' +
    '<div class="wifi-bar"></div>' +
    '<div class="wifi-bar"></div>' +
    '<div class="wifi-bar"></div>' +
    '<div class="wifi-bar"></div>' +
  '</div>';
}

// Toast Notifications (Homewind Style)
function toast(type, msg, title) {
  var el = document.getElementById('toast');
  var backdrop = document.getElementById('toast-backdrop');
  var icon = document.getElementById('toast-icon');
  var titleEl = document.getElementById('toast-title');
  var text = document.getElementById('toast-msg');
  
  if (!el || !icon || !text) return;

  // Set icon class
  icon.className = '';
  if (type === 'success') {
    icon.className = 'success';
  } else if (type === 'error') {
    icon.className = 'error';
  } else {
    icon.className = 'spinner';
  }

  // Set title
  if (titleEl) {
    var defaultTitle = '';
    if (type === 'success') defaultTitle = 'Success';
    else if (type === 'error') defaultTitle = 'Error';
    else if (type === 'loading') defaultTitle = 'Please Wait';
    titleEl.textContent = title || defaultTitle;
  }

  // Set message
  text.textContent = msg;

  // Show toast and backdrop
  el.classList.add('show');
  if (backdrop) backdrop.classList.add('show');

  // Auto-hide for success/error (not for loading)
  if (type !== 'loading') {
    setTimeout(function() {
      el.classList.remove('show');
      if (backdrop) backdrop.classList.remove('show');
    }, 3000);
  }
}

// Hide toast programmatically
function hideToast() {
  var el = document.getElementById('toast');
  var backdrop = document.getElementById('toast-backdrop');
  if (el) el.classList.remove('show');
  if (backdrop) backdrop.classList.remove('show');
}

// Network List Loading
var scanRetries = 0;
var selectedSSID = '';
var selectedBSSID = '';

function loadNetworks() {
  var container = document.getElementById('networks');
  if (!container) return;

    container.innerHTML = '<div class="network empty">' + 
    '<div style="width:32px;height:32px;background:var(--icon-spinner) no-repeat center/contain"></div>' +
    '<span>' + (scanRetries > 0 ? 'Scanning... (' + scanRetries + ')' : 'Scanning...') + '</span>' +
  '</div>';
    
  var xhr = new XMLHttpRequest();
  xhr.open('GET', '/wml/netlist', true);
  xhr.onreadystatechange = function() {
    if (xhr.readyState !== 4) return;
    
    if (xhr.status !== 200) {
      container.innerHTML = '<div class="network empty">Scan failed</div>';
      toast('error', 'Network scan failed');
      return;
    }
    
    try {
      var data = JSON.parse(xhr.responseText);

    // Still scanning - retry
    if (data.scanning && scanRetries < 10) {
      scanRetries++;
      setTimeout(loadNetworks, 1500);
      if (!data.networks || !data.networks.length) return;
    }

    container.innerHTML = '';

    if (!data.networks || !data.networks.length) {
      container.innerHTML = '<div class="network empty">No networks found</div>';
      if (scanRetries < 5) {
        scanRetries++;
        setTimeout(loadNetworks, 2000);
      }
      return;
    }

    scanRetries = 0;
      data.networks.sort(function(a, b) { return b.rssi - a.rssi; });

      for (var i = 0; i < data.networks.length; i++) {
        (function(n) {
          var btn = document.createElement('button');
      btn.type = 'button';
      btn.className = 'network';
          btn.innerHTML = signalHTML(n.rssi) +
            '<div class="network-info">' +
              '<div class="ssid">' + (n.ssid || '(hidden)') + '</div>' +
              '<div class="network-meta">' +
                n.rssi + ' dBm' +
                (!n.enc ? '<span style="color:var(--accent-green)">Open</span>' : '') +
              '</div>' +
            '</div>' +
            (n.enc ? '<span class="lock-icon network-lock"></span>' : '');
      
          btn.onclick = function() {
        // Deselect all
            var allNets = document.querySelectorAll('.network');
            for (var j = 0; j < allNets.length; j++) {
              allNets[j].classList.remove('selected');
            }
        btn.classList.add('selected');
        
        selectedSSID = n.ssid || '';
        selectedBSSID = n.bssid || '';
        
        // Update form fields if they exist
            var ssidField = document.getElementById('ssid0');
            var bssidField = document.getElementById('bssid0');
        if (ssidField) ssidField.value = selectedSSID;
        if (bssidField) bssidField.value = selectedBSSID;
        
        // Enable save button if exists
            var saveBtn = document.getElementById('saveBtn');
        if (saveBtn) saveBtn.disabled = !selectedSSID;
        
        // Focus password field
            var pwField = document.getElementById('password') || document.getElementById('password0');
        if (pwField) pwField.focus();
      };
      
      container.appendChild(btn);
        })(data.networks[i]);
      }
  } catch (e) {
      container.innerHTML = '<div class="network empty">Scan failed</div>';
      toast('error', 'Network scan failed');
    }
  };
  xhr.onerror = function() {
    container.innerHTML = '<div class="network empty">Scan failed</div>';
    toast('error', 'Network scan failed');
  };
  xhr.send();
  }

// Refresh Networks
function refreshNetworks() {
  scanRetries = 0;
  loadNetworks();
}

// Reset Functions (using XMLHttpRequest)
function doReset() {
  if (!confirm('Reset WiFi connection and enter AP mode?')) return;
  toast('loading', 'Resetting WiFi...', 'Please Wait');
  
  var xhr = new XMLHttpRequest();
  xhr.open('POST', '/wml/reset', true);
  xhr.onreadystatechange = function() {
    if (xhr.readyState === 4) {
      toast('success', 'Device is restarting...');
      setTimeout(function() { location.reload(); }, 3000);
    }
  };
  xhr.onerror = function() {
    toast('error', 'Reset failed');
  };
  xhr.send();
}

function doFactoryReset() {
  if (!confirm('Factory Reset will delete ALL settings!\n\nAre you sure?')) return;
  if (!confirm('This cannot be undone. Continue?')) return;
  toast('loading', 'Performing factory reset...', 'Please Wait');
  
  var xhr = new XMLHttpRequest();
  xhr.open('POST', '/wml/factoryreset', true);
  xhr.onreadystatechange = function() {
    if (xhr.readyState === 4) {
      toast('success', 'Factory reset complete');
      setTimeout(function() { location.reload(); }, 3000);
    }
  };
  xhr.onerror = function() {
    toast('error', 'Reset failed');
  };
  xhr.send();
}

// Initialize network list on page load
document.addEventListener('DOMContentLoaded', function() {
  if (document.getElementById('networks')) {
    loadNetworks();
  }
});
