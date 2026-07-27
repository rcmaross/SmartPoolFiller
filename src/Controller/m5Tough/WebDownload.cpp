#include "WebDownload.h"
#include <SD.h>
#include "WebMain.h"

static const char* POOL_LOG_DIR = "/SmartPoolFiller";

bool handleDownloadWebRoutes(WiFiClient& client, const String& requestLine) {
    // Route 1: Intercept the new multi-page URL target cleanly
    if (requestLine.indexOf("GET /download ") != -1) {
        client.print("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n");
        client.print(HEADER_HTML);
        client.print(NAVIGATION_HTML);
        client.print(INDEX_DOWNLOAD_HTML); // Independent Download view data
        client.print(FOOTER_HTML);
        return true;
    }
    
    // Route 2: Asynchronous Micro-JSON Array Lister API
    if (requestLine.indexOf("GET /api/files ") != -1) {
        client.print("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n");
        client.print("[");

        File root = SD.open(POOL_LOG_DIR);
        if (root && root.isDirectory()) {
            File file = root.openNextFile();
            bool isFirst = true;
            while (file) {
                if (!file.isDirectory()) {
                    if (!isFirst) client.print(",");
                    isFirst = false;
                    client.print("{\"name\":\""); client.print(file.name());
                    client.print("\",\"size\":"); client.print(file.size());
                    client.print("}");
                }
                file = root.openNextFile();
            }
        }
        client.print("]");
        return true;
    } 
    
    // Route 3: Zero-Copy SPI Direct Download Engine
    if (requestLine.indexOf("GET /api/download?file=") != -1) {
        int startIdx = requestLine.indexOf("?file=") + 6;
        int endIdx = requestLine.indexOf(" ", startIdx);
        String filename = requestLine.substring(startIdx, endIdx);

        if (filename.indexOf("..") != -1 || filename.indexOf('/') != -1) {
            client.print("HTTP/1.1 403 Forbidden\r\nConnection: close\r\n\r\nAccess Denied.");
        } else {
            String fullPath = String(POOL_LOG_DIR) + "/" + filename;
            if (SD.exists(fullPath)) {
                File fileToStream = SD.open(fullPath, FILE_READ);
                if (fileToStream) {
                    client.print("HTTP/1.1 200 OK\r\nContent-Type: text/csv\r\nContent-Disposition: attachment; filename=");
                    client.print(filename);
                    client.print("\r\nConnection: close\r\n\r\n");

                    // Optimized stream channel: pipelines blocks cleanly without a manual chunk loop
                    client.write(fileToStream); 
                    
                    fileToStream.close();
                }
            } else {
                client.print("HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\nFile Missing.");
            }
        }
        return true;
    }
    
    return false; // Let request fall through to WebMain routes
}
