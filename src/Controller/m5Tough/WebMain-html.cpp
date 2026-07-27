#include "WebMain.h"

// 1. Central Stylesheet Storage
const char* SHARED_CSS PROGMEM = R"rawliteral(
:root { --primary: #0284c7; --bg: #f4f7f6; --card-bg: #ffffff; --text: #333; }
body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; margin: 0; background: var(--bg); color: var(--text); }
.system-header { background: #1e293b; color: #fff; padding: 12px 20px; display: flex; justify-content: space-between; align-items: center; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
.system-title { font-weight: bold; font-size: 1.1em; }
.telemetry-strip { font-size: 0.9em; font-family: monospace; }
.nav-tabs { display: flex; background: #0f172a; border-bottom: 3px solid var(--primary); overflow-x: auto; }
.tab-btn { display: block; color: #94a3b8; padding: 14px 20px; font-size: 0.95em; font-weight: 500; text-decoration: none; transition: all 0.2s; white-space: nowrap; }
.tab-btn:hover { color: #fff; background: #1e293b; }
.tab-btn.active { color: #fff; background: var(--primary); }
.container { max-width: 900px; margin: 20px auto; padding: 0 15px; }
.card { background: var(--card-bg); padding: 20px; border-radius: 8px; box-shadow: 0 4px 6px rgba(0,0,0,0.05); margin-bottom: 20px; }
h2 { color: var(--primary); margin-top: 0; border-bottom: 1px solid #e2e8f0; padding-bottom: 8px; }
.dashboard-grid { display: flex; gap: 30px; align-items: flex-start; margin-top: 15px; }
.tank-container { width: 35px; height: 130px; border: 2px solid #94a3b8; border-radius: 2px; position: relative; background: #f1f5f9; overflow: hidden; display: flex; flex-direction: column; justify-content: flex-end; }
.tank-segment { width: 100%; transition: height 0.3s ease; }
.seg-red { background: #ef4444; height: 0%; }
.seg-yellow { background: #eab308; height: 0%; }
.seg-blue { background: #3b82f6; height: 0%; }
.metric-list { display: flex; flex-direction: column; gap: 6px; }
.m-depth { font-size: 24px; font-weight: bold; color: #000; margin: 0; }
.m-delta { font-size: 18px; color: #64748b; margin: 0; }
.m-text { font-size: 14px; color: #475569; font-family: monospace; margin: 0; }
.status-indicator { display: inline-block; width: 10px; height: 10px; border-radius: 50%; margin-left: 6px; vertical-align: middle; transition: background 0.4s ease; }
)rawliteral";

// 2. Global Document Top & System Header
const char* HEADER_HTML PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <link rel="stylesheet" href="/style.css">
</head>
<body>
    <header class="system-header">
        <div class="system-title">🏊 SmartPoolFiller Framework</div>
        <div class="telemetry-strip">
            <span id="hdrStatus">LVL: --% | INIT</span>
            <span>TIME: <strong id="hdrTime">00:00</strong></span>
            <span>📶 WiFi: <span class="status-indicator" id="hdrWifiDot" style="background:#94a3b8;"></span>
        </div>
    </header>
)rawliteral";

// 3. Global Navigation Matrix (No active classes embedded statically!)
const char* NAVIGATION_HTML PROGMEM = R"rawliteral(
    <nav class="nav-tabs">
        <a id="lnk-main" class="tab-btn" href="/">💧 MainDisplay</a>
        <a id="lnk-settings" class="tab-btn" href="/settings">⚙️ Settings</a>
        <a id="lnk-calibration" class="tab-btn" href="/calibration">🛠️ Calibration</a>
        <a id="lnk-network" class="tab-btn" href="/network">📶 Network</a>
        <a id="lnk-history" class="tab-btn" href="/history">👁️ History</a>
        <a id="lnk-download" class="tab-btn" href="/download">📊 Download</a>
    </nav>
    <div class="container">
)rawliteral";

// 4. Isolated MainDisplay Content Fragment
const char* INDEX_HTML PROGMEM = R"rawliteral(
        <div class="card">
            <h2>💧 MainDisplay</h2>
            <div class="dashboard-grid">
                <div class="tank-container">
                    <div id="tankRed" class="tank-segment seg-red"></div>
                    <div id="tankYellow" class="tank-segment seg-yellow"></div>
                    <div id="tankBlue" class="tank-segment seg-blue"></div>
                </div>
                <div class="metric-list">
                    <p id="lblDepth" class="m-depth">0.0 in</p>
                    <p id="lblDelta" class="m-delta">0.0 in</p>
                    <p id="lblValve" class="m-text" style="font-weight:bold;">VALVE: STANDBY</p>
                    <p id="lblLive" class="m-text">Inst: --</p>
                    <p id="lblVolt" class="m-text">Sensor: --</p>
                    <p id="lblMac" class="m-text" style="color:#94a3b8;">MAC: --</p>
                </div>
            </div>
        </div>
)rawliteral";

// 5. Global Script Operations & Document Enclosure
const char* FOOTER_HTML PROGMEM = R"rawliteral(
    </div>
    <script>
        // Universal Tab Activation Engine (Looks at current browser path address string)
        document.addEventListener("DOMContentLoaded", () => {
            const currentPath = window.location.pathname;
            let targetId = "lnk-main";
            if (currentPath.includes("settings")) targetId = "lnk-settings";
            else if (currentPath.includes("calibration")) targetId = "lnk-calibration";
            else if (currentPath.includes("network")) targetId = "lnk-network";
            else if (currentPath.includes("history")) targetId = "lnk-history";
            else if (currentPath.includes("download")) targetId = "lnk-download";
            
            const activeBtn = document.getElementById(targetId);
            if (activeBtn) activeBtn.classList.add("active");
        });

        // Universal 10s Header Poller Loop
        function updateGlobalHeaderLoop() {
            fetch('/api/telemetry')
                .then(res => res.json())
                .then(data => {
                    const statusBox = document.getElementById('hdrStatus');
                    const timeBox   = document.getElementById('hdrTime');
                    const dotElement = document.getElementById('hdrWifiDot');
                    
                    if (statusBox)  statusBox.innerText = data.sysStatus;
                    if (timeBox)    timeBox.innerText = data.sysTime;
                    if (dotElement) dotElement.style.background = data.wifiColor; // Sync indicator dot
                }).catch(() => {
                    const statusBox = document.getElementById('hdrStatus');
                    const dotElement = document.getElementById('hdrWifiDot');
                    if (statusBox)  statusBox.innerText = "CONNECTION LOST";
                    if (dotElement) dotElement.style.background = '#ef4444'; // Red fault alarm
                });
        }
        setInterval(updateGlobalHeaderLoop, 10000);
        updateGlobalHeaderLoop();
                function updateMainDisplayData() {
            fetch('/api/main-data')
                .then(res => res.json())
                .then(data => {
                    document.getElementById('lblDepth').innerText = data.depth;
                    document.getElementById('lblDelta').innerText = data.delta;
                    
                    const valveTxtBlock = document.getElementById('lblValve');
                    if (valveTxtBlock) {
                        valveTxtBlock.innerText = data.valve;
                        valveTxtBlock.style.color = data.valveColor;
                    }
                    
                    document.getElementById('lblLive').innerText  = data.live;
                    document.getElementById('lblVolt').innerText  = data.volt;
                    document.getElementById('lblMac').innerText   = data.mac;

                    // Compute your exact hardware tank rectangle scale layout dimensions dynamically
                    const pct = data.pct;
                    const rSeg = document.getElementById('tankRed');
                    const ySeg = document.getElementById('tankYellow');
                    const bSeg = document.getElementById('tankBlue');

                    if (pct < 100) {
                        rSeg.style.height = "0%";
                        let blueHeight = (108 / 130) * pct; // % scale relative to full container height
                        let yellowHeight = 83.0 - blueHeight; // Yellow takes the remaining full headroom gap
                        
                        bSeg.style.height = blueHeight + "%";
                        ySeg.style.height = yellowHeight + "%";
                    } else if (pct > 100) {
                        ySeg.style.height = "0%";
                        bSeg.style.height = "83.0%"; // Blue locked down full at baseline level
                        
                        let extraPct = pct - 100;
                        let redHeight = ((130 - 108) / 130) * (extraPct / 20.0) * 100;
                        if (redHeight > 17.0) redHeight = 17.0; // Enforce max_h boundaries
                        
                        rSeg.style.height = redHeight + "%";
                    } else {
                        rSeg.style.height = "0%";
                        ySeg.style.height = "0%";
                        bSeg.style.height = "83.0%"; // Lock solid at 100% capacity level
                    }
                }).catch(() => {});
        }
        
        // Register the active view polling worker loop thread
        setInterval(updateMainDisplayData, 10000);
        document.addEventListener("DOMContentLoaded", updateMainDisplayData);

    </script>
</body>
</html>
)rawliteral";
