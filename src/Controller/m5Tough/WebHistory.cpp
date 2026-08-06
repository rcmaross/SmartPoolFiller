#include "WebHistory.h"
#include "WebMain.h"
#include "SystemState.h"
#include <SD.h>

bool handleHistoryWebRoutes(WiFiClient& client, const String& requestLine) {
    if (requestLine.indexOf("GET /history") != -1) {
        client.print("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n");
        client.print(HEADER_HTML);
        client.print(NAVIGATION_HTML);
        client.print(INDEX_HISTORY_HTML); 
        client.print(FOOTER_HTML);
        return true;
    }

    if (requestLine.indexOf("GET /api/history-data") != -1) {
        client.print("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n");

        lv_obj_t* dayStatsL = getObjByName("hist_day_stats");
        lv_obj_t* wkStatsL  = getObjByName("hist_wk_stats");
        
        // Fetch the native chart widget handle from our registry map
        lv_obj_t* chartObj = getObjByName("chart_trends");

        const char* txtDay = dayStatsL ? lv_label_get_text(dayStatsL) : "Past 24H: 0 mins";
        const char* txtWk  = wkStatsL  ? lv_label_get_text(wkStatsL)  : "7D Ave: 0.0 mins";

        client.print("{");
        client.print("\"dayStats\":\""); client.print(txtDay); client.print("\",");
        client.print("\"wkStats\":\"");  client.print(txtWk);  client.print("\",");

        // Native LVGL v9 Series Array Ingestion Traversal
        int32_t* levelArray = nullptr;
        int32_t* valveArray = nullptr;

        if (chartObj != nullptr) {
            // In LVGL v9, series are linked in a chain inside the chart object base structure
            lv_chart_series_t* ser1 = lv_chart_get_series_next(chartObj, nullptr); // Grabs first series (Level)
            lv_chart_series_t* ser2 = lv_chart_get_series_next(chartObj, ser1);    // Grabs second series (Valve)
            
            if (ser1 != nullptr) levelArray = lv_chart_get_y_array(chartObj, ser1);
            if (ser2 != nullptr) valveArray = lv_chart_get_y_array(chartObj, ser2);
        }

        // Pipe out real-time Water Depth Level values straight from the LVGL Widget
        client.print("\"levels\":[");
        for (int i = 0; i < 24; i++) {
            if (levelArray != nullptr) {
                int32_t val = levelArray[i];
                client.print(val == LV_CHART_POINT_NONE ? -1 : val);
            } else {
                client.print("-1");
            }
            if (i < 23) client.print(",");
        }

        // Pipe out real-time Valve Run Minutes straight from the LVGL Widget
        client.print("],\"valves\":[");
        for (int i = 0; i < 24; i++) {
            if (valveArray != nullptr) {
                int32_t val = valveArray[i];
                client.print(val == LV_CHART_POINT_NONE ? -1 : val);
            } else {
                client.print("-1");
            }
            if (i < 23) client.print(",");
        }
        client.print("]}");
        return true;
    }

    if (requestLine.indexOf("GET /api/depth-data") != -1) {
        client.print("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n");

        client.print("{");
        client.print("\"calcMean\":");    client.print(sysState->calculateMeanDepth(), 2);    client.print(",");
        client.print("\"calcMedian\":");  client.print(sysState->calculateMedianDepth(), 2);  client.print(",");
        client.print("\"calcTrimmed\":"); client.print(sysState->calculateTrimmedMean(), 2); client.print(",");

        client.print("\"rawBuffer\":[");
        float lowest = 10000.0f;
        float highest = 0.0f;
        for (int i = 0; i < 60; i++) {
            client.print(sysState->depth_history[i], 2); // Pulls straight from your rolling array
            if (i < 59) client.print(",");
            if (sysState->depth_history[i] < lowest)
                lowest = sysState->depth_history[i];
            if (sysState->depth_history[i] > highest)
                highest = sysState->depth_history[i];
        }
        client.print("],");
        client.print("\"calcLowest\":"); client.print(lowest, 2); client.print(",");
        client.print("\"calcHighest\":"); client.print(highest, 2);
        client.print("}");
        return true;
    }

    return false; 
}
