#include "WebDownload.h"
#include "WebMain.h"
#include "StorageDisk.h"

bool handleDownloadWebRoutes(WiFiClient& client, const String& requestLine) {
    // Look up our runtime allocated instance pointer natively
    StorageDisk* disk = StorageDisk::getInstance();
    if (disk == nullptr) {
        // Safe fallback: Hardware not initialized yet, drop out of routing chain cleanly
        return false; 
    }

    // Route 1: HTML Page delivery
    if (requestLine.indexOf("GET /download") != -1) {
        client.print("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n");
        client.print(HEADER_HTML); client.print(NAVIGATION_HTML);
        client.print(INDEX_DOWNLOAD_HTML); client.print(FOOTER_HTML);
        return true;
    }
    
    // Route 2: Asynchronous Micro-JSON Array Lister API
    if (requestLine.indexOf("GET /api/files") != -1) {
        StorageDisk::ActiveFileStream dirStream = disk->openAppDirectoryStream();
        if (!dirStream.isOpen()) {
            client.print("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n[]");
            return true;
        }

        client.print("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n[");
        File& root = dirStream.getFile();
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
        client.print("]");
        return true; 
    } 
    
    // Route 3: Zero-Copy SPI Direct Download Engine
    if (requestLine.indexOf("GET /api/download") != -1) {
        int startIdx = requestLine.indexOf("?file=") + 6;
        int endIdx = requestLine.indexOf(" ", startIdx);
        if (startIdx < 6 || endIdx == -1) return false;
        
        String filename = requestLine.substring(startIdx, endIdx);
        if (filename.indexOf("..") != -1 || filename.indexOf('/') != -1) {
            client.print("HTTP/1.1 403 Forbidden\r\nConnection: close\r\n\r\nAccess Denied.");
            return true;
        }

        if (disk->fileExistsInApp(filename)) {
            StorageDisk::ActiveFileStream fileStream = disk->openFileReadStream(filename);
            if (fileStream.isOpen()) {
                client.print("HTTP/1.1 200 OK\r\nContent-Type: text/csv\r\nContent-Disposition: attachment; filename=");
                client.print(filename);
                client.print("\r\nConnection: close\r\n\r\n");
                client.write(fileStream.getFile());
            }
        } else {
            client.print("HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\nFile Missing.");
        }
        return true; 
    }
    
    if (requestLine.indexOf("DELETE /api/delete-file") != -1) {
        int startIdx = requestLine.indexOf("?file=") + 6;
        int endIdx = requestLine.indexOf(" ", startIdx);
        if (startIdx < 6 || endIdx == -1) return false;
        
        String filename = requestLine.substring(startIdx, endIdx);
        
        // Prevent path traversal escape attacks completely
        if (filename.indexOf("..") != -1 || filename.indexOf('/') != -1) {
            client.print("HTTP/1.1 403 Forbidden\r\nConnection: close\r\n\r\nAccess Denied.");
            return true;
        }

        // Query our hybrid singleton instance mapping handle
        StorageDisk* disk = StorageDisk::getInstance();
        if (disk != nullptr && disk->removeFileFromApp(filename)) {
            client.print("HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nSuccess");
            Serial.printf("[WEB-STORAGE] Explicitly erased target file asset: %s\n", filename.c_str());
        } else {
            client.print("HTTP/1.1 500 Internal Error\r\nConnection: close\r\n\r\nDeletion Failed.");
        }
        return true;
    }

    return false; 
}
