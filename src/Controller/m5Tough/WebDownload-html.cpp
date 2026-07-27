#include "WebDownload.h"

const char* INDEX_DOWNLOAD_HTML = R"rawliteral(
        <div class="card">
            <h2>📊 Download Storage Logs</h2>
            <ul class="file-list" id="fileList">
                <li class="empty-state">Loading yearly logs from /SmartPoolFiller...</li>
            </ul>
        </div>
    <script>
        // Dedicated, isolated local list ingestion
        document.addEventListener("DOMContentLoaded", () => {
            const list = document.getElementById('fileList');
            if (!list) return;
            fetch('/api/files').then(res => res.json()).then(files => {
                list.innerHTML = '';
                if (files.length === 0) {
                    list.innerHTML = '<li class="empty-state">No log sheets found.</li>';
                    return;
                }
                files.forEach(f => {
                    const li = document.createElement('li');
                    li.className = 'file-item';
                    li.innerHTML = `
                        <span class="file-name">📄 ${f.name} (${f.size} bytes)</span>
                        <a href="/api/download?file=${encodeURIComponent(f.name)}" class="btn-download">Download</a>
                    `;
                    list.appendChild(li);
                });
            }).catch(() => {
                list.innerHTML = '<li class="empty-state" style="color: #ef4444;">Error accessing storage disk.</li>';
            });
        });
    </script>
)rawliteral";
