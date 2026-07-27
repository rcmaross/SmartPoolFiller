#include "WebMain.h"
#include "WebDownload.h" 
#include "WebHistory.h"
#include "SystemState.h"
#include <map>

static std::map<String, lv_obj_t*>* uiRegistry = nullptr;

void registerUiObj(const char* name, lv_obj_t* obj) {
    if (uiRegistry == nullptr) {
        uiRegistry = new std::map<String, lv_obj_t*>(); // Lazy-instantiation on heap
    }
    if (obj != nullptr && name != nullptr) {
        (*uiRegistry)[String(name)] = obj; // Inserts or overwrites the unique name binding
    }
}

lv_obj_t* getObjByName(const char* name) {
    if (uiRegistry == nullptr || name == nullptr) return nullptr;
    
    auto it = uiRegistry->find(String(name));
    if (it != uiRegistry->end()) {
        return it->second; // Instant pointer return
    }
    return nullptr;
}

void initPoolWebServer(WiFiServer *server) {
    server->begin();
}

void handlePoolWebClient(WiFiServer *server) {
    WiFiClient client = server->available();
    if (!client) return;

    String requestLine = "";
    if (client.connected() && client.available()) {
        requestLine = client.readStringUntil('\r');
        client.flush();
    } else {
        return;
    }

    if (handleDownloadWebRoutes(client, requestLine)) {
        client.stop();
        return;
    }

    if (handleHistoryWebRoutes(client, requestLine)) {
        client.stop();
        return;
    }

    if (requestLine.indexOf("GET /style.css") != -1) {
        client.print("HTTP/1.1 200 OK\r\nContent-Type: text/css\r\nConnection: close\r\n\r\n");
        client.print(SHARED_CSS);
        client.stop();
        return;
    }

    if (requestLine.indexOf("GET / ") != -1 || requestLine.indexOf("GET /index.html") != -1) {
        client.print("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n");
        
        // Server-Side Block Assembly Pipe Stream
        client.print(HEADER_HTML);
        client.print(NAVIGATION_HTML);
        client.print(INDEX_HTML); // Primary landing view data
        client.print(FOOTER_HTML);
        return;
    }

    if (requestLine.indexOf("GET /api/telemetry") != -1) {
        client.print("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n");
        
        // 1. Look up object pointers from your high-speed registry
        lv_obj_t* statusLabel = getObjByName("status");
        lv_obj_t* timeLabel   = getObjByName("time");
        lv_obj_t* wifiLabel   = getObjByName("wifi-icon");

        // 2. Safely extract text lines
        const char* currentStatus = statusLabel ? lv_label_get_text(statusLabel) : "LVL: --% | INIT";
        const char* currentTime   = timeLabel   ? lv_label_get_text(timeLabel)   : "00:00";
        
        // 3. Extract the text color value and format it directly into a hex string
        String wifiColorHex = "#94a3b8"; // Default soft gray fallback
        if (wifiLabel != nullptr) {
            // Enforce the strict LVGL type enum LV_PART_MAIN
            lv_color_t rawColor = lv_obj_get_style_text_color(wifiLabel, LV_PART_MAIN);
            
            // LVGL v9 direct channel access fields
            uint8_t r = rawColor.red;
            uint8_t g = rawColor.green;
            uint8_t b = rawColor.blue;

            char hexBuffer[8];
            snprintf(hexBuffer, sizeof(hexBuffer), "#%02X%02X%02X", r, g, b);
            wifiColorHex = String(hexBuffer);
        }

        // 4. Stream the multi-value JSON package cleanly
        client.print("{");
        client.print("\"sysStatus\":\""); client.print(currentStatus); client.print("\",");
        client.print("\"sysTime\":\"");   client.print(currentTime);   client.print("\",");
        client.print("\"wifiColor\":\""); client.print(wifiColorHex);   client.print("\"");
        client.print("}");
        client.stop();
        return;
    }

    if (requestLine.indexOf("GET /api/main-data") != -1) {
        client.print("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n");

        lv_obj_t* depthL = getObjByName("main_depth");
        lv_obj_t* deltaL = getObjByName("main_delta");
        lv_obj_t* valveL = getObjByName("main_valve");
        lv_obj_t* liveL  = getObjByName("main_live");
        lv_obj_t* voltL  = getObjByName("main_volt");
        lv_obj_t* macL   = getObjByName("main_mac");

        const char* txtDepth = depthL ? lv_label_get_text(depthL) : "0.0 in";
        const char* txtDelta = deltaL ? lv_label_get_text(deltaL) : "0.0 in";
        const char* txtValve = valveL ? lv_label_get_text(valveL) : "VALVE: OFFLINE";
        const char* txtLive  = liveL  ? lv_label_get_text(liveL)  : "Inst: --";
        const char* txtVolt  = voltL  ? lv_label_get_text(voltL)  : "Sensor: --";
        const char* txtMac   = macL   ? lv_label_get_text(macL)   : "MAC: --";

        // NEW: Read the text color property of the valve label using the LVGL v9 engine
        String valveColorHex = "#64748b"; // Default slate-grey fallback
        if (valveL != nullptr) {
            lv_color_t vColor = lv_obj_get_style_text_color(valveL, LV_PART_MAIN);
            char hexBuf[8];
            snprintf(hexBuf, sizeof(hexBuf), "#%02X%02X%02X", vColor.red, vColor.green, vColor.blue);
            valveColorHex = String(hexBuf);
        }

        int pct = 0;
        float dummy_depth = 0.0f;
        const char* dummy_status = "";
        if (sysState != nullptr) {
            sysState->getPoolMetrics(pct, dummy_depth, dummy_status);
        }

        client.print("{");
        client.print("\"depth\":\""); client.print(txtDepth); client.print("\",");
        client.print("\"delta\":\""); client.print(txtDelta); client.print("\",");
        client.print("\"valve\":\""); client.print(txtValve); client.print("\",");
        client.print("\"valveColor\":\""); client.print(valveColorHex); client.print("\","); // Added parameter link
        client.print("\"live\":\"");  client.print(txtLive);  client.print("\",");
        client.print("\"volt\":\"");  client.print(txtVolt);  client.print("\",");
        client.print("\"mac\":\"");   client.print(txtMac);   client.print("\",");
        client.print("\"pct\":");     client.print(pct);
        client.print("}");
        client.stop();
        return;
    }

    client.stop();
}
