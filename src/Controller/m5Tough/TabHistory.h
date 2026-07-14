#pragma once
#include "BaseTab.h"
#include "SystemState.h"
#include <stdio.h>

class TabHistory : public BaseTab {
private:
    lv_obj_t* chart_trends = nullptr;

    // LVGL v9 handles: Series line graph references
    lv_chart_series_t* series_level = nullptr;
    lv_chart_series_t* series_valve = nullptr;

    // 1. The 24-Element Linear Display Data Arrays (Used directly by the chart)
    int32_t level_chart_data[24] = {0};
    int32_t valve_chart_data[24] = {0};

    // 2. YOUR PROPOSED DESIGN: The 168-Element Rolling Master History Logs (7 Full Days)
    int32_t level_history_log[168] = {0};
    int32_t valve_history_log[168] = {0};
    
    int ring_write_index = 0; // Circulating index pointer (0 to 167)

    lv_obj_t* l_day_stats = nullptr;
    lv_obj_t* l_wk_stats = nullptr;

    uint32_t last_chart_tick = 0;

public:
    void setup(lv_obj_t* tab) override {
        lv_obj_set_layout(tab, 0);
        
        lv_obj_set_style_pad_all(tab, 2, 0);
        lv_obj_set_style_pad_left(tab, 5, 0);  
        lv_obj_set_style_pad_right(tab, 5, 0); 

        // Initialize Main Analytical Trend Chart
        chart_trends = lv_chart_create(tab);
        lv_obj_set_size(chart_trends, 205, 95); 
        lv_obj_set_pos(chart_trends, 35, 5);   
        
        // FIXED: Expanded point count to explicitly hold 24 entries (1 full day)
        lv_chart_set_point_count(chart_trends, 24); 

        // hone in on full+1 to full -2 inches
        int lowDepth = sysState.offset_in - 2;
        int highDepth = sysState.offset_in + 1;

        if(sysState.use_metric){
            lowDepth *= 2.54f;
            highDepth *= 2.54f;
        }

        // Set ranges across the vertical dimensions
        lv_chart_set_range(chart_trends, LV_CHART_AXIS_PRIMARY_Y, lowDepth * 100, highDepth * 100);
        lv_chart_set_range(chart_trends, LV_CHART_AXIS_SECONDARY_Y, 0, 60);

        // --- LVGL v9 SCALE REGISTERS CONFIGURATION ---
        lv_obj_t* scale_left = lv_scale_create(tab);
        lv_obj_set_size(scale_left, 30, 95); lv_obj_set_pos(scale_left, 2, 5); 
        lv_scale_set_mode(scale_left, LV_SCALE_MODE_VERTICAL_RIGHT);


        lv_scale_set_range(scale_left, lowDepth, highDepth);
        lv_scale_set_total_tick_count(scale_left, 4); lv_scale_set_major_tick_every(scale_left, 1);
        lv_obj_set_style_text_color(scale_left, lv_color_black(), 0);
        lv_obj_set_style_text_font(scale_left, &lv_font_montserrat_14, 0);

        lv_obj_t* scale_right = lv_scale_create(tab);
        lv_obj_set_size(scale_right, 30, 95); lv_obj_set_pos(scale_right, 242, 5);
        lv_scale_set_mode(scale_right, LV_SCALE_MODE_VERTICAL_LEFT);
        lv_scale_set_range(scale_right, 0, 60);
        lv_scale_set_total_tick_count(scale_right, 3); lv_scale_set_major_tick_every(scale_right, 1);
        lv_obj_set_style_text_color(scale_right, lv_color_black(), 0);
        lv_obj_set_style_text_font(scale_right, &lv_font_montserrat_14, 0);

        series_level = lv_chart_add_series(chart_trends, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
        series_valve = lv_chart_add_series(chart_trends, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_SECONDARY_Y);

        // Map the 24-element presentation arrays to the static series handles at boot
        lv_chart_set_ext_y_array(chart_trends, series_level, level_chart_data);
        lv_chart_set_ext_y_array(chart_trends, series_valve, valve_chart_data);

        // Initialize master historical data logs to a standard baseline (100% full, 0 run mins)
        for (int i = 0; i < 168; i++) {
            level_history_log[i] = highDepth * 100;
            valve_history_log[i] = 0;
        }

        // Initialize display canvas tracking layout lines
        for (int i = 0; i < 24; i++) {
            level_chart_data[i] = highDepth * 100;
            valve_chart_data[i] = 0;
        }

        // Statistics Presentation Panel Labels via BaseTab Constructors
        createString(tab, "TOTAL FILL DURATIONS:", 14, 10, 110, lv_palette_main(LV_PALETTE_GREY));
        l_day_stats = createString(tab, "Past 24H: 0 mins", 14, 10, 128);
        l_wk_stats  = createString(tab, "Past 7D: 0.0 hrs", 14, 150, 128);
        
        last_chart_tick = millis();
    }

    void update() override {
        if (!chart_trends || !series_level || !series_valve || !l_day_stats || !l_wk_stats) return;

        uint32_t now = millis();

        // 1-Hour Step Window (Real Time = 3,600,000ms. Accelerated 360x Mode = exactly 10 seconds) [INDEX]
        uint32_t scaled_chart_interval = 3600000 / sysState.time_scale_factor;

        if (now - last_chart_tick >= scaled_chart_interval) {
            last_chart_tick = now;

            int pct = 0; float depth = 0.0f; const char* status = "";
            sysState.getPoolMetrics(pct, depth, status);

            // Calculate true actual runtime minutes accumulated over this exact time window slice.
            // (live_run_seconds / 60) gives raw minutes. Multiply by scale factor to adjust density values.
            float true_window_run_minutes = ((float)sysState.live_valve_run_seconds_current_hour / 60.0f) * sysState.time_scale_factor;
            if (true_window_run_minutes > 60.0f) true_window_run_minutes = 60.0f; // Maximum hour constraint

            int depth_i = (int)(depth * 100); // multiply by 100 to make a psuedo float

            // A. Overwrite the single oldest slot in the master 168-hour history log [INDEX]
            level_history_log[ring_write_index] = depth_i;
            valve_history_log[ring_write_index] = (int32_t)true_window_run_minutes;

            // PADDING SERIAL PRINT DEBUG LINES:
            Serial.printf("[HIST LOG] Slot %d saved -> Level: %f, Actual Valve Run: %d mins (Raw Loop Accumulator: %d seconds)\n", 
                          ring_write_index, depth, (int32_t)true_window_run_minutes, sysState.live_valve_run_seconds_current_hour);

            // Instantly clear the global real-time accumulator block for the next hour window pass
            sysState.live_valve_run_seconds_current_hour = 0;

            // Advance the rolling write cursor pointer, wrapping back around cleanly at 168 [INDEX]
            ring_write_index = (ring_write_index + 1) % 168;

            // B. YOUR PROPOSED DESIGN: Copy backwards to populate the linear 24-element presentation array [INDEX]
            // We loop chronologically from oldest (i=0, 24 hours ago) to newest (i=23, 1 hour ago) [INDEX]
            for (int i = 0; i < 24; i++) {
                int lookback_idx = (ring_write_index - 24 + i + 168) % 168;
                level_chart_data[i] = level_history_log[lookback_idx];
                valve_chart_data[i] = valve_history_log[lookback_idx];
            }

            // Push a serial validation marker to trace the linear display snapshot mapping
            Serial.printf("[HIST CHART] Linear 24-Hour view compiled. Latest slot mapped to index 23: %d mins\n", valve_chart_data[23]);

            // Notify the graph to instantly re-raster the lines based on our static layout arrays
            lv_chart_refresh(chart_trends);
        }

        // =====================================================================
        // COMPUTE REAL-TIME STATS FROM THE 168-HOUR CIRCULAR BUFFER
        // =====================================================================
        // Past 24H: Sum up the last 24 elements from the current write cursor position [INDEX]
        uint32_t total_minutes_past_24h = 0;
        for (int i = 0; i < 24; i++) {
            int lookback_idx = (ring_write_index - 24 + i + 168) % 168;
            total_minutes_past_24h += valve_history_log[lookback_idx];
        }

        // Past 7D: Sum all 168 historical elements together across the full buffer [INDEX]
        uint32_t total_minutes_past_7d = 0;
        for (int i = 0; i < 168; i++) {
            total_minutes_past_7d += valve_history_log[i];
        }

        char d_buf[32] = {0}; 
        char w_buf[32] = {0}; 
        
        snprintf(d_buf, sizeof(d_buf), "Past 24H: %d mins", total_minutes_past_24h);
        snprintf(w_buf, sizeof(w_buf), "Past 7D: %.1f hrs", (float)total_minutes_past_7d / 60.0f);

        // Update display text blocks seamlessly in light-theme black text
        updateString(l_day_stats, d_buf);
        updateString(l_wk_stats, w_buf);
    }
};
