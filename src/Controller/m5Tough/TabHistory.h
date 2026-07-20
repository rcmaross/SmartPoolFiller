#pragma once
#include "BaseTab.h"
#include "SystemState.h"
#include <stdio.h>

class TabHistory : public BaseTab {
private:
    lv_obj_t* chart_trends = nullptr;
    lv_obj_t* scale_left = nullptr;
    lv_obj_t* scale_right = nullptr;

    // LVGL v9 handles: Series line graph references
    lv_chart_series_t* series_level = nullptr;
    lv_chart_series_t* series_valve = nullptr;

    // 1. The 24-Element Linear Display Data Arrays (Used directly by the chart)
    int32_t level_chart_data[24];
    int32_t valve_chart_data[24];

    // 2. YOUR PROPOSED DESIGN: The 168-Element Rolling Master History Logs (7 Full Days)
    int32_t level_history_log[168];
    int32_t valve_history_log[168];
    
    int ring_write_index = 0; // Circulating index pointer (0 to 167)

    lv_obj_t* l_day_stats = nullptr;
    lv_obj_t* l_wk_stats = nullptr;

    uint32_t last_chart_tick = 0;
    bool last_known_unit_mode = false;

    const int Y_AXIS_PADDING = 30;
public:
    void rescaleLeftAxis(int min_val, int max_val) {
        if (!chart_trends || !scale_left) return;

        int data_span = sysState.convertFromInch(max_val - min_val);
        int step = 100; // Fallback default step
        int dynamic_high = sysState.convertFromInch(max_val);

        if (sysState.use_metric) {
            // --- METRIC MODE RES BINS (Scaled by 100 -> Hundredths of a cm) ---
            if (data_span <= 2100)      step = 700;  // ~1/4" equiv -> 7.00 cm steps (Total Span = 21.00 cm)
            else if (data_span <= 3900) step = 1300; // ~1/2" equiv -> 13.00 cm steps (Total Span = 39.00 cm)
            else                        step = 2600; // ~1.0" equiv -> 26.00 cm steps (Total Span = 78.00 cm)
        } else {
            // --- IMPERIAL MODE RES BINS (Scaled by 100 -> Hundredths of an inch) ---
            if (data_span <= 75)        step = 25;   // 1/4" steps (0.25 inches)
            else if (data_span <= 150)  step = 50;   // 1/2" steps (0.50 inches)
            else                        step = 100;  // 1.0" steps (1.00 inch)
        }

        while (dynamic_high % step != 0) {
            dynamic_high++;
        }


        // -----------------------------------------------------------------
        // STEP 3: Set the other 3 values accordingly based on the scale step
        // -----------------------------------------------------------------
        int dynamic_low = dynamic_high - (3 * step);

        lv_chart_set_range(chart_trends, LV_CHART_AXIS_PRIMARY_Y, dynamic_low, dynamic_high);

        // -----------------------------------------------------------------
        // STEP 4: Reset the scale labels and redraw the graph canvas
        // -----------------------------------------------------------------
        static char label_buffers[4][16];
        static const char* custom_labels[5];

        for (int i = 0; i < 4; i++) {
            int current_tick_val = dynamic_low + (i * step);

            float display_val = current_tick_val / 100.0f;
            snprintf(label_buffers[i], sizeof(label_buffers[i]), "%g", display_val);
            custom_labels[i] = label_buffers[i];
        }
        custom_labels[4] = NULL; // Explicit null-termination block wrapper

        lv_scale_set_text_src(scale_left, custom_labels);
        // lv_obj_invalidate(scale_left);
    }

    void setup(lv_obj_t* tab) override {

        // total chart dimensions, only change these constants to change the chart size.
        // chart size includes a left and right y-axis
        // The assumption here is that the chart itself takes up as much of the area
        // as it can assuming 2 y-axis
        const int totalScreenWidth = 320;
        const int hwMargin = 8; // need a margin gap or scroll bars appear
        const int screenWidth = totalScreenWidth - hwMargin;
        const int screenSpacing = 2;
        const int chartAndAxisPos[2] = {screenSpacing, 3};  // x,y corr of start
        const int chartAndAxisArea[2] = {screenWidth - screenSpacing, 128}; // x size, y size
        const int axisWidth = 24; // we need this much space for a single axis
        const int axisSpace = 2; // we want some pad to pull it away from the chart
        const int axisHeadFoot = 8; // pad the head so it lines up with the top of the chart

        const int chartPos[2]  = {
            chartAndAxisPos[0] + axisWidth + axisSpace, // start after left axis
            chartAndAxisPos[1]
        };
        const int chartSize[2] = {
            chartAndAxisArea[0] - (2 * axisWidth) - (2 * axisSpace),  // remove left and right axis
            chartAndAxisArea[1]
        };
        const int leftPos[2] = {
            chartAndAxisPos[0],
            chartAndAxisPos[1] + axisHeadFoot
        };
        const int leftSize[2] = {
            axisWidth,
            chartAndAxisArea[1] - (axisHeadFoot * 2)
        };
        const int rightPos[2] = {
            chartAndAxisPos[0] + chartAndAxisArea[0] - axisWidth - axisSpace, // back up 1 axis (and pad) from the end
            chartAndAxisPos[1] + axisHeadFoot
        };
        const int rightSize[2] = {
            axisWidth,
            chartAndAxisArea[1] - (axisHeadFoot * 2)
        };

        lv_obj_set_layout(tab, 0);
        
        lv_obj_set_style_pad_all(tab, 2, 0);
        lv_obj_set_style_pad_left(tab, 5, 0);  
        lv_obj_set_style_pad_right(tab, 5, 0); 

        // Initialize Main Analytical Trend Chart
        chart_trends = lv_chart_create(tab);
        lv_obj_set_size(chart_trends, chartSize[0], chartSize[1]);
        lv_obj_set_pos(chart_trends, chartPos[0], chartPos[1]);
        
        // 1 per hour of 24hr window
        lv_chart_set_point_count(chart_trends, 24); 

        // hone in on full + 1 to full - 2 inches
        int initDepth = sysState.offset_in * 100;

        // Initialize master historical data logs to a standard baseline (100% full, 0 run mins)
        for (int i = 0; i < 168; i++) {
            level_history_log[i] = LV_CHART_POINT_NONE;
            valve_history_log[i] = LV_CHART_POINT_NONE;
        }

        // Initialize display canvas tracking layout lines
        for (int i = 0; i < 24; i++) {
            level_chart_data[i] = LV_CHART_POINT_NONE;
            valve_chart_data[i] = LV_CHART_POINT_NONE;
        }

        lv_chart_set_range(chart_trends, LV_CHART_AXIS_SECONDARY_Y, 0, 60);
        // --- LVGL v9 SCALE REGISTERS CONFIGURATION ---
        scale_left = lv_scale_create(tab);
        lv_obj_set_size(scale_left, leftSize[0], leftSize[1]);
        lv_obj_set_pos(scale_left, leftPos[0], leftPos[1]);
        lv_scale_set_mode(scale_left, LV_SCALE_MODE_VERTICAL_RIGHT);
        lv_scale_set_total_tick_count(scale_left, 7);
        lv_scale_set_major_tick_every(scale_left, 2);
        lv_obj_set_style_line_color(scale_left, lv_palette_main(LV_PALETTE_BLUE), 0);
        lv_obj_set_style_line_color(scale_left, lv_palette_main(LV_PALETTE_BLUE), LV_PART_ITEMS);
        lv_obj_set_style_line_color(scale_left, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
        lv_obj_set_style_text_color(scale_left, lv_palette_main(LV_PALETTE_BLUE), 0);
        lv_obj_set_style_text_font(scale_left, &lv_font_montserrat_10, 0);

        rescaleLeftAxis(initDepth, initDepth);

        scale_right = lv_scale_create(tab);
        lv_obj_set_size(scale_right, rightSize[0], rightSize[1]);
        lv_obj_set_pos(scale_right, rightPos[0], rightPos[1]);
        lv_scale_set_mode(scale_right, LV_SCALE_MODE_VERTICAL_LEFT);
        lv_scale_set_range(scale_right, 0, 60);
        lv_scale_set_total_tick_count(scale_right, 7);
        lv_scale_set_major_tick_every(scale_right, 3);
        lv_obj_set_style_line_color(scale_right, lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_set_style_line_color(scale_right, lv_palette_main(LV_PALETTE_RED), LV_PART_ITEMS);
        lv_obj_set_style_line_color(scale_right, lv_palette_main(LV_PALETTE_RED), LV_PART_INDICATOR);

        lv_obj_set_style_text_color(scale_right, lv_palette_main(LV_PALETTE_RED), 0);
        lv_obj_set_style_text_font(scale_right, &lv_font_montserrat_10, 0);

        series_level = lv_chart_add_series(chart_trends, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
        series_valve = lv_chart_add_series(chart_trends, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_SECONDARY_Y);

        // Map the 24-element presentation arrays to the static series handles at boot
        lv_chart_set_ext_y_array(chart_trends, series_level, level_chart_data);
        lv_chart_set_ext_y_array(chart_trends, series_valve, valve_chart_data);

        // Statistics Presentation Panel Labels via BaseTab Constructors
        // createString(tab, "TOTAL FILL DURATIONS:", 14, 10, 110, lv_palette_main(LV_PALETTE_GREY));
        l_day_stats = createString(tab, "Past 24H: 0 mins", 14, 10, 138);
        l_wk_stats  = createString(tab, "Past 7D: 0.0 hrs", 14, 150, 138);
        
        last_chart_tick = millis();
        lv_chart_refresh(chart_trends);

        last_known_unit_mode = !sysState.use_metric;

        // force an update to get the chart created
        update(true);
    }

    void update(bool force) override {
        if (!chart_trends || !series_level || !series_valve || !l_day_stats || !l_wk_stats) return;

        uint32_t now = millis();

        // 1-Hour Step Window (Real Time = 3,600,000ms. Accelerated 360x Mode = exactly 10 seconds) [INDEX]
        uint32_t scaled_chart_interval = 3600000 / sysState.time_scale_factor;

        // force this to happen now if there is a units change
        if (force || (sysState.use_metric != last_known_unit_mode) || (now - last_chart_tick >= scaled_chart_interval)) {
            last_known_unit_mode = sysState.use_metric;
            last_chart_tick = now;

            int pct = 0; float depth = 0.0f; const char* status = "";
            sysState.getPoolMetrics(pct, depth, status);

            // Calculate true actual runtime minutes accumulated over this exact time window slice.
            // (live_run_seconds / 60) gives raw minutes. Multiply by scale factor to adjust density values.
            float true_window_run_minutes = ((float)sysState.live_valve_run_seconds_current_hour / 60.0f) * sysState.time_scale_factor;
            if (true_window_run_minutes > 60.0f) true_window_run_minutes = 60.0f; // Maximum hour constraint

            int depth_i = (int)(depth * 100.0f); // multiply by 100 to make a psuedo float

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

            int min_depth = depth_i;
            int max_depth = depth_i;
            int valid_results = 0;
            for (int i = 0; i < 24; i++) {
                int lookback_idx = (ring_write_index - 24 + i + 168) % 168;
                int level = level_history_log[lookback_idx];
                if (level != LV_CHART_POINT_NONE) {
                    if (level < min_depth) min_depth = level;
                    if (level > max_depth) max_depth = level;
                    level_chart_data[i] = sysState.convertFromInch(level);
                    valve_chart_data[i] = valve_history_log[lookback_idx];
                    valid_results++;
                }
            }

            if (valid_results < 24) {
                for(int i=0; i < valid_results; ++i){
                    int right_idx = 24 - valid_results + i;
                    level_chart_data[i] = level_chart_data[right_idx];
                    valve_chart_data[i] = valve_chart_data[right_idx];
                }
                for(int i=valid_results; i < 24; ++i){
                    level_chart_data[i] = LV_CHART_POINT_NONE;
                    valve_chart_data[i] = LV_CHART_POINT_NONE;
                }
            }

            rescaleLeftAxis(min_depth, max_depth);

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
            int value = valve_history_log[lookback_idx];
            if (value != LV_CHART_POINT_NONE)
                total_minutes_past_24h += value;
        }

        // Past 7D: Sum all 168 historical elements together across the full buffer [INDEX]
        uint32_t total_minutes_past_7d = 0;
        int valid_entries = 0;
        for (int i = 0; i < 168; i++) {
            int value = valve_history_log[i];
            if (value != LV_CHART_POINT_NONE) {
                total_minutes_past_7d += value;
                valid_entries++;
            }

        }

        int valid_days = valid_entries / 24;
        if (valid_entries % 24)
            valid_days++;

        char d_buf[32] = {0}; 
        char w_buf[32] = {0}; 
        
        snprintf(d_buf, sizeof(d_buf), "Past 24H: %d mins", total_minutes_past_24h);
        snprintf(w_buf, sizeof(w_buf), "7D Ave: %.1f mins", (float)total_minutes_past_7d / (float)valid_days);

        // Update display text blocks seamlessly in light-theme black text
        updateString(l_day_stats, d_buf);
        updateString(l_wk_stats, w_buf);
    }
};
