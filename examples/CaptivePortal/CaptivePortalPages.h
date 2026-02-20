#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <wifiMangerLite.h>

// Keep HTML helpers in a header to keep CaptivePortal.ino easier to read.
// This file is compiled as part of the Arduino sketch (no separate .cpp needed).

static String buildStatusPageHtml(WML::WiFiManagerLite& wifiMgr, const WML::Config& cfg) {
    String html = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8"/>
  <meta name="viewport" content="width=device-width,initial-scale=1.0,user-scalable=no"/>
  <title>)rawhtml" + String(cfg.deviceName) + R"rawhtml( - Status</title>
  <link rel="stylesheet" href="/wml/style.css"/>
  <style>
    /* Status page specific styles */
    .status-list{display:flex;flex-direction:column;gap:2px}
    .status-item{display:flex;justify-content:space-between;align-items:center;padding:12px 0;border-bottom:1px solid var(--border-soft)}
    .status-item:last-child{border-bottom:none}
    .status-label{color:var(--text-muted);font-weight:500;font-size:14px}
    .status-value{font-weight:600;font-family:'SF Mono',Monaco,monospace;font-size:14px}
    .indicator{display:inline-flex;align-items:center;gap:8px}
    .indicator.connected{color:#00A554}
    .indicator.disconnected{color:var(--accent-pink)}
    .indicator::before{content:'';width:10px;height:10px;border-radius:50%}
    .indicator.connected::before{background:#00A554}
    .indicator.disconnected::before{background:var(--accent-pink)}
    .card-title-icon{width:24px;height:24px;background-size:contain;background-repeat:no-repeat;background-position:center}
  </style>
</head>
<body>
  <div class="app">
    <div class="page-header">
    <h1>)rawhtml" + String(cfg.deviceName) + R"rawhtml(</h1>
      <div class="subtitle">Device Status</div>
    </div>

    <div class="card">
      <div class="card-header">
        <div class="card-title">
          <div class="card-title-icon" style="background-image:url('data:image/svg+xml,%3Csvg%20width%3D%2232%22%20height%3D%2232%22%20viewBox%3D%220%200%2032%2032%22%20fill%3D%22none%22%20xmlns%3D%22http%3A//www.w3.org/2000/svg%22%3E%3Crect%20x%3D%224%22%20y%3D%2219%22%20width%3D%223%22%20height%3D%229%22%20rx%3D%221.5%22%20fill%3D%22%23374957%22/%3E%3Crect%20x%3D%2211%22%20y%3D%2215%22%20width%3D%223%22%20height%3D%2213%22%20rx%3D%221.5%22%20fill%3D%22%23374957%22/%3E%3Crect%20x%3D%2218%22%20y%3D%2210%22%20width%3D%223%22%20height%3D%2218%22%20rx%3D%221.5%22%20fill%3D%22%23374957%22/%3E%3Crect%20x%3D%2225%22%20y%3D%225%22%20width%3D%223%22%20height%3D%2223%22%20rx%3D%221.5%22%20fill%3D%22%23374957%22/%3E%3C/svg%3E')"></div>
          Connection
        </div>
        <span class="indicator )rawhtml" + String(wifiMgr.isConnected() ? "connected" : "disconnected") + R"rawhtml(">
          )rawhtml" + String(wifiMgr.isConnected() ? "Connected" : (wifiMgr.isAPMode() ? "AP Mode" : "Disconnected")) + R"rawhtml(
        </span>
      </div>
      <div class="status-list">
        <div class="status-item">
          <span class="status-label">SSID</span>
          <span class="status-value">)rawhtml" + (wifiMgr.isConnected() ? wifiMgr.getSSID() : "—") + R"rawhtml(</span>
        </div>
        <div class="status-item">
          <span class="status-label">IP Address</span>
          <span class="status-value">)rawhtml" + (wifiMgr.isConnected() ? wifiMgr.getStationIP().toString() : wifiMgr.getAPIP().toString()) + R"rawhtml(</span>
      </div>
        <div class="status-item">
          <span class="status-label">Signal</span>
          <span class="status-value">)rawhtml" + (wifiMgr.isConnected() ? String(wifiMgr.getRSSI()) + " dBm" : "—") + R"rawhtml(</span>
      </div>
        <div class="status-item">
          <span class="status-label">MAC Address</span>
          <span class="status-value">)rawhtml" + WiFi.macAddress() + R"rawhtml(</span>
      </div>
      </div>
    </div>

    <div class="card">
      <div class="card-header">
        <div class="card-title">
          <div class="card-title-icon" style="background-image:url('data:image/svg+xml,%3Csvg xmlns=\'http://www.w3.org/2000/svg\' width=\'32\' height=\'32\' fill=\'none\' viewBox=\'0 0 32 32\'%3E%3Cpath fill=\'%23374957\' d=\'M5 8.75h2.736a4 4 0 0 0 7.195 0H27a1 1 0 1 0 0-2H14.931a4 4 0 0 0-7.195 0H5a1 1 0 0 0 0 2Zm6.333-2.75a1.75 1.75 0 1 1 0 3.5 1.75 1.75 0 0 1 0-3.5ZM27 15h-2.736a4 4 0 0 0-7.194 0H5a1 1 0 1 0 0 2h12.07a4 4 0 0 0 7.194 0H27a1 1 0 1 0 0-2Zm-6.333 2.75a1.75 1.75 0 1 1 0-3.5 1.75 1.75 0 0 1 0 3.5ZM27 23.25H14.931a4 4 0 0 0-7.195 0H5a1 1 0 1 0 0 2h2.736a4 4 0 0 0 7.195 0H27a1 1 0 1 0 0-2Zm-15.667 2.75a1.75 1.75 0 1 1 0-3.5 1.75 1.75 0 0 1 0 3.5Z\'/%3E%3C/svg%3E')"></div>
          Configuration
        </div>
      </div>
      <div class="status-list">
        <div class="status-item">
          <span class="status-label">Device Name</span>
          <span class="status-value">)rawhtml" + String(cfg.deviceName) + R"rawhtml(</span>
        </div>
        <div class="status-item">
          <span class="status-label">Primary SSID</span>
          <span class="status-value">)rawhtml" + String(cfg.primary.ssid[0] != '\0' ? cfg.primary.ssid : "—") + R"rawhtml(</span>
        </div>
        <div class="status-item">
          <span class="status-label">BSSID Lock</span>
          <span class="status-value">)rawhtml" + String(cfg.primary.bssidLock ? "Yes" : "No") + R"rawhtml(</span>
      </div>
        <div class="status-item">
          <span class="status-label">Fallback SSID</span>
          <span class="status-value">)rawhtml" + String(cfg.secondary.ssid[0] != '\0' ? cfg.secondary.ssid : "—") + R"rawhtml(</span>
      </div>
        <div class="status-item">
          <span class="status-label">Static IP</span>
          <span class="status-value">)rawhtml" + String(cfg.staticIP.ip[0] != '\0' ? cfg.staticIP.ip : "DHCP") + R"rawhtml(</span>
      </div>
      </div>
    </div>

    <div class="button-stack">
      <a href="/wml/setup" class="btn btn-large btn-cta">WiFi Settings</a>
      <button class="btn btn-large btn-secondary" onclick="doReset()">Reset WiFi</button>
      <button class="btn btn-large btn-danger" onclick="doFactoryReset()">Factory Reset</button>
    </div>

    <div class="page-footer">
      )rawhtml" + String(cfg.deviceName) + R"rawhtml( · Firmware 1.1.0<br>
      Free Heap: )rawhtml" + String(ESP.getFreeHeap() / 1024) + R"rawhtml( KB
    </div>
  </div>

  <script>
    async function doReset(){
      if(!confirm('Reset WiFi and enter AP mode?'))return;
      try{
        await fetch('/wml/reset',{method:'POST'});
        alert('Resetting... Please reconnect to the AP.');
        setTimeout(()=>location.reload(),3000);
      }catch(e){alert('Reset failed')}
    }
    async function doFactoryReset(){
      if(!confirm('⚠️ Factory Reset will delete ALL settings!\n\nAre you sure?'))return;
      if(!confirm('This cannot be undone. Continue?'))return;
      try{
        await fetch('/wml/factoryreset',{method:'POST'});
        alert('Factory reset complete. Device will restart.');
        setTimeout(()=>location.reload(),5000);
      }catch(e){alert('Reset failed')}
    }
  </script>
</body>
</html>
)rawhtml";

    return html;
}

