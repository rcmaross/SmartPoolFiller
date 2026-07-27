#include "WebHistory.h"

// Stored completely in Flash (PROGMEM). Zero DRAM or global BSS segment impact.
const char* INDEX_HISTORY_HTML PROGMEM = R"rawliteral(
<div class="card">
    <h2>👁️ History</h2>
    <div style="display:flex; justify-content:space-between; margin-bottom:15px; font-weight:bold;">
        <span id="webDayStats">Past 24H: 0 mins</span>
        <span id="webWkStats">7D Ave: 0.0 mins</span>
    </div>
    
    <!-- High-Performance Interactive Browser Canvas Graph Container -->
    <div style="position:relative; width:100%; max-width:600px; margin:0 auto; background:#fff; border:1px solid #e2e8f0; padding:10px; border-radius:4px;">
        <canvas id="historyChart" width="550" height="200" style="width:100%; height:auto; display:block;"></canvas>
    </div>
</div>

<script>
    document.addEventListener("DOMContentLoaded", () => {
        fetch('/api/history-data')
            .then(res => res.json())
            .then(data => {
                // 1. Map Text Statistics Fields
                document.getElementById('webDayStats').innerText = data.dayStats;
                document.getElementById('webWkStats').innerText = data.wkStats;

                // 2. Extract Data Channels (24 hourly nodes)
                const lvls = data.levels; // Array of integers/floats
                const valves = data.valves; // Array of run minutes

                // Setup HTML5 Canvas Context
                const canvas = document.getElementById('historyChart');
                const ctx = canvas.getContext('2d');
                if (!ctx) return;

                // Layout Dimensions Constants
                const padL = 40, padR = 40, padT = 20, padB = 30;
                const w = canvas.width - padL - padR;
                const h = canvas.height - padT - padB;

                // Clean Background Grid
                ctx.clearRect(0, 0, canvas.width, canvas.height);
                ctx.strokeStyle = '#f1f5f9';
                ctx.lineWidth = 1;
                for (let i = 0; i <= 4; i++) {
                    let y = padT + (h / 4) * i;
                    ctx.beginPath(); ctx.moveTo(padL, y); ctx.lineTo(canvas.width - padR, y); ctx.stroke();
                }

                // Compute Left Axis Boundaries (Water Depth Scale mapping)
                const validLvls = lvls.filter(v => v !== -1); // Skip NONE tags
                const minLvl = validLvls.length ? Math.min(...validLvls) - 10 : 0;
                const maxLvl = validLvls.length ? Math.max(...validLvls) + 10 : 100;
                const spanLvl = maxLvl - minLvl || 1;

                // Draw Primary Channel: Water Depth Line Graph (Blue Color Axis)
                ctx.strokeStyle = '#3b82f6';
                ctx.lineWidth = 3;
                ctx.beginPath();
                let first = true;
                for (let i = 0; i < 24; i++) {
                    if (lvls[i] === -1) continue; // Skip unpopulated slots
                    let x = padL + (w / 23) * i;
                    let y = padT + h - ((lvls[i] - minLvl) / spanLvl) * h;
                    if (first) { ctx.moveTo(x, y); first = false; } else { ctx.lineTo(x, y); }
                }
                ctx.stroke();

                // Draw Secondary Channel: Valve Run Minutes Bar Chart (Red Color Axis)
                ctx.fillStyle = 'rgba(239, 68, 68, 0.4)';
                for (let i = 0; i < 24; i++) {
                    if (valves[i] === -1) continue;
                    let x = padL + (w / 23) * i - 4;
                    let barH = (valves[i] / 60) * h; // Normalizes max 60 minutes capacity window height
                    let y = padT + h - barH;
                    ctx.fillRect(x, y, 8, barH);
                }

                // Draw Axis Outlines
                ctx.strokeStyle = '#94a3b8';
                ctx.beginPath(); ctx.moveTo(padL, padT); ctx.lineTo(padL, padT + h); ctx.lineTo(canvas.width - padR, padT + h); ctx.stroke();

                // Label Axis Ticks
                ctx.fillStyle = '#3b82f6'; ctx.font = '10px sans-serif';
                ctx.fillText((maxLvl/100).toFixed(1), 5, padT + 10);
                ctx.fillText((minLvl/100).toFixed(1), 5, padT + h);

                ctx.fillStyle = '#ef4444'; ctx.textAlign = 'right';
                ctx.fillText("60m", canvas.width - 5, padT + 10);
                ctx.fillText("0m", canvas.width - 5, padT + h);
            }).catch(() => {});
    });
</script>
)rawliteral";
