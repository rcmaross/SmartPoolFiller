#include "WebDownload.h"

const char* INDEX_DOWNLOAD_HTML PROGMEM = R"rawliteral(
<style>
    .file-list { list-style: none; padding: 0; margin: 0; }
    .file-item { display: flex; justify-content: space-between; align-items: center; padding: 12px; border-bottom: 1px solid #edf2f7; }
    .file-item:last-child { border-bottom: none; }
    .file-name { font-weight: 500; color: #2d3748; }
    .btn-download { background: #0284c7; color: white; text-decoration: none; padding: 6px 12px; border-radius: 4px; font-size: 0.85em; transition: background 0.2s; }
    .btn-download:hover { background: #0369a1; }
    .empty-state { text-align: center; color: #718096; padding: 20px; }
</style>

<div class="card">
    <h2>📊 Download Storage Logs</h2>
    <ul class="file-list" id="fileList">
        <li class="empty-state">Loading yearly logs from /SmartPoolFiller...</li>
    </ul>
</div>

<script>
    document.addEventListener("DOMContentLoaded", () => {
        const list = document.getElementById('fileList');
        if (!list) return;
        
        function loadFilesList() {
            fetch('/api/files')
                .then(res => res.json())
                .then(files => {
                    list.innerHTML = '';
                    if (files.length === 0) {
                        list.innerHTML = '<li class="empty-state">No log sheets found inside /SmartPoolFiller.</li>';
                        return;
                    }
                    files.forEach(f => {
                        const li = document.createElement('li');
                        li.className = 'file-item';
                        li.innerHTML = `
                            <span class="file-name">📄 ${f.name} (${f.size} bytes)</span>
                            <div style="display:flex; gap:10px; align-items:center;">
                                <a href="/api/download?file=${encodeURIComponent(f.name)}" class="btn-download">Download</a>
                                <!-- Dynamic individual file erase button -->
                                <button onclick="deleteSingleFile('${encodeURIComponent(f.name)}')" 
                                        style="background:#ef4444; color:white; border:none; padding:6px 10px; border-radius:4px; cursor:pointer; font-size:0.85em; transition:0.2s;"
                                        onmouseover="this.style.background='#dc2626'" 
                                        onmouseout="this.style.background='#ef4444'">
                                    🗑️ Erase
                                </button>
                            </div>
                        `;
                        list.appendChild(li);
                    });
                })
                .catch(() => {
                    list.innerHTML = '<li class="empty-state" style="color: #ef4444;">Error accessing storage disk over network.</li>';
                });
        }

        // Global single-file deletion function handler
        window.deleteSingleFile = function(filename) {
            if (!confirm(`Are you absolutely sure you want to permanently delete ${decodeURIComponent(filename)}?`)) return;
            
            fetch(`/api/delete-file?file=${filename}`, { method: 'DELETE' })
                .then(res => {
                    if (res.ok) {
                        loadFilesList(); // Force an immediate local re-poll to update the list layout smoothly!
                    } else {
                        alert("Failed to delete file from disk storage.");
                    }
                }).catch(() => alert("Network communication breakdown during deletion."));
        };

        loadFilesList();
    });
</script>
)rawliteral";
