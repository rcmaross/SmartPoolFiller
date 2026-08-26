#pragma once
#include "BaseTab.h"
#include "SystemState.h"
#include "WebMain.h"

class TabMainDisplay : public BaseTab {
private:
    lv_obj_t* l_full = nullptr;
    lv_obj_t* l_measurement = nullptr;
    lv_obj_t* l_live_measure = nullptr;
    lv_obj_t* l_raw_voltage = nullptr;
    lv_obj_t* l_raw_pressure = nullptr;
    lv_obj_t* l_mac_addr = nullptr;
    lv_obj_t* l_valve_state = nullptr;
    lv_obj_t* rect_top_red = nullptr;
    lv_obj_t* rect_mid_yellow = nullptr;
    lv_obj_t* rect_bot_blue = nullptr;

    const int tank_x = 10;
    const int tank_w = 35;
    const int tank_l = 8;
    const int max_h = 130;
    const int full_h = 108;
    const int tank_floor_y = 15 + 130;

public:
    void setup(lv_obj_t* tab_container) override {
        lv_obj_set_layout(tab_container, 0);
        lv_obj_set_style_pad_all(tab_container, 0, 0);

        lv_obj_t* tank_bg = lv_obj_create(tab_container);
        lv_obj_set_size(tank_bg, tank_w + 4, max_h + 4);
        lv_obj_set_pos(tank_bg, tank_x - 2, tank_l);
        lv_obj_set_style_bg_opa(tank_bg, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(tank_bg, 2, 0);
        lv_obj_set_style_border_color(tank_bg, lv_palette_main(LV_PALETTE_GREY), 0);
        lv_obj_set_style_radius(tank_bg, 2, 0);
        lv_obj_set_style_pad_all(tank_bg, 0, 0);
        lv_obj_remove_flag(tank_bg, LV_OBJ_FLAG_SCROLLABLE);

        rect_top_red = createRectangle(tab_container, tank_w, LV_SIZE_CONTENT, lv_palette_main(LV_PALETTE_RED));
        rect_mid_yellow = createRectangle(tab_container, tank_w, LV_SIZE_CONTENT, lv_palette_main(LV_PALETTE_YELLOW));
        rect_bot_blue = createRectangle(tab_container, tank_w, LV_SIZE_CONTENT, lv_palette_main(LV_PALETTE_BLUE));

        lv_obj_set_size(rect_top_red, tank_w, 0);
        lv_obj_set_pos(rect_top_red, tank_x, tank_floor_y - max_h);

        lv_obj_set_size(rect_bot_blue, tank_w, 0);
        lv_obj_set_pos(rect_bot_blue, tank_x, tank_floor_y);

        lv_obj_set_size(rect_mid_yellow, tank_w, full_h);
        lv_obj_set_pos(rect_mid_yellow, tank_x, tank_floor_y - full_h);
        l_full = createString(tab_container, "0.0", 14, tank_x + tank_w + 8, tank_floor_y - full_h - 8, lv_palette_main(LV_PALETTE_GREY));

        // left half of display...
        int left_margin = 100 + tank_x;
        int first = 20;
        int big_offset = 32;
        int sm_offset = 17;
        int next = first;
        l_measurement = createString(tab_container, "0.00 in (0.00 in)", 24, left_margin, next, lv_color_black());
        next += big_offset;
        l_valve_state = createString(tab_container, "VALVE: OFFLINE", 14, left_margin, next, lv_palette_main(LV_PALETTE_GREY));
        next += sm_offset;
        l_live_measure = createString(tab_container, "0.00 in (0.00in)", 14, left_margin, next, lv_palette_main(LV_PALETTE_GREY));
        next += sm_offset;
        l_raw_voltage = createString(tab_container, "Raw Volt: 0.000 V", 14, left_margin, next, lv_color_black());
        next += sm_offset;
        l_raw_pressure = createString(tab_container, "Pressure: 00.00 inHg", 14, left_margin, next, lv_color_black());
        next += sm_offset;
        l_mac_addr = createString(tab_container, "MAC: 00:00:00:00:00:00", 14, left_margin, next, lv_palette_main(LV_PALETTE_GREY));

        registerUiObj("main_depth", l_measurement);
        registerUiObj("main_valve", l_valve_state);
        registerUiObj("main_live",  l_live_measure);
        registerUiObj("main_volt",  l_raw_voltage);
        registerUiObj("main_pressure", l_raw_pressure); 
        registerUiObj("main_mac",   l_mac_addr);
        registerUiObj("main_full", l_full);

    }

    void update(bool force) override {
        if (!l_measurement || !l_live_measure || !l_raw_voltage || !l_valve_state || !rect_top_red || !rect_mid_yellow || !rect_bot_blue || !l_mac_addr) return;

        int pct = 0;
        float poolDepth = 0.0f;
        const char* status = "";
        sysState->getPoolMetrics(pct, poolDepth, status);

        int unused_pct = 0;
        float instantPoolDepth = 0.0f;
        const char* unused_status = "";
        sysState->getInstantaneousPoolMetrics(unused_pct, instantPoolDepth, unused_status);

        String depthStr = "";
        String deltaStr = "";
        String instantDepthStr = "";
        String instantDeltaStr = "";

        if (sysState->connection_type == 0) {
            updateString(l_valve_state, "RADIO OFF", lv_palette_main(LV_PALETTE_GREY));
        } else if (!sysState->timeAllowed()) {
            updateString(l_valve_state, "VALVE: DELAYED", lv_palette_main(LV_PALETTE_GREY));
        } else {
            if (sysState->active_master_command_state == 1) {
                updateString(l_valve_state, "VALVE: FILLING", lv_palette_main(LV_PALETTE_RED));
            } else if (sysState->active_master_command_state == 2) {
                updateString(l_valve_state, "VALVE: RESTING", lv_palette_main(LV_PALETTE_ORANGE));
            } else {
                updateString(l_valve_state, "VALVE: STANDBY", lv_palette_main(LV_PALETTE_GREEN));
            }
        }
        if (!sysState->use_metric) {
            depthStr = String(poolDepth, 2);
            float inchesFromFull = poolDepth - sysState->offset_in;
            if (inchesFromFull > 0.0f) deltaStr = "+" + String(inchesFromFull, 2);
            else if (inchesFromFull < 0.0f) deltaStr = String(inchesFromFull, 2);
            else deltaStr = "0.00";

            instantDepthStr = String(instantPoolDepth, 2);
            inchesFromFull = instantPoolDepth - sysState->offset_in;
            if (inchesFromFull > 0.0f) instantDeltaStr = "+" + String(inchesFromFull, 2);
            else if (inchesFromFull < 0.0f) instantDeltaStr = String(inchesFromFull, 2);
            else instantDeltaStr = "0.00";
        } else {
            float cmPoolDepth = sysState->convertFromInch(poolDepth);
            depthStr = String(cmPoolDepth, 2);
            float cmFromFull = cmPoolDepth - sysState->convertFromInch(sysState->offset_in);
            if (cmFromFull > 0.0f) deltaStr = "+" + String(cmFromFull, 2);
            else if (cmFromFull < 0.0f) deltaStr = String(cmFromFull, 2);
            else deltaStr = "0.00";

            float cmInstantPoolDepth = sysState->convertFromInch(instantPoolDepth);
            instantDepthStr = String(cmInstantPoolDepth, 2);
            cmFromFull = cmInstantPoolDepth - sysState->convertFromInch(sysState->offset_in);
            if (cmFromFull > 0.0f) instantDeltaStr = "+" + String(cmFromFull, 2);
            else if (cmFromFull < 0.0f) instantDeltaStr = String(cmFromFull, 2);
            else instantDeltaStr = "0.00";
        }

        char b_off[32];
        if (!sysState->use_metric) {
            snprintf(b_off, sizeof(b_off), "%.1f", sysState->offset_in);
        } else {
            snprintf(b_off, sizeof(b_off), "%.1f", sysState->offset_in * 2.54f);
        }

        updateString(l_full, b_off);
        String measurementStr = deltaStr + " (" + depthStr + ")";
        updateString(l_measurement, measurementStr.c_str());
        String fullLiveStr = "Inst: " + instantDeltaStr + " (" + instantDepthStr + ")";
        updateString(l_live_measure, fullLiveStr.c_str());

        char mac_buffer[] = "MAC: 00:00:00:00:00:00";
        snprintf(mac_buffer, sizeof(mac_buffer), "MAC: %02X:%02X:%02X:%02X:%02X:%02X", (int)sysState->mac_address[0], (int)sysState->mac_address[1], (int)sysState->mac_address[2], (int)sysState->mac_address[3], (int)sysState->mac_address[4], (int)sysState->mac_address[5]);
        updateString(l_mac_addr, mac_buffer);

        float ui_pressure = sysState->convertFromInHg(sysState->pressure_inHg);
        String pressureStr = "Pressure: " + String(ui_pressure);
        updateString(l_raw_pressure, pressureStr.c_str(), lv_palette_main(LV_PALETTE_BLUE));
        if (sysState->ads_hardware_found) {
            String voltStr = "Raw Volt: " + String(sysState->sim_voltage, 3) + " V";
            updateString(l_raw_voltage, voltStr.c_str(), lv_palette_main(LV_PALETTE_BLUE));
        } else {
            if (sysState->sim_voltage <= 0.02f) {
                updateString(l_raw_voltage, "LOOP DISCONNECTED", lv_palette_main(LV_PALETTE_RED));
                updateString(l_measurement, "FAULT(OFFLINE)", lv_palette_main(LV_PALETTE_RED));
                updateString(l_live_measure, "FAULT", lv_palette_main(LV_PALETTE_RED));
            } else {
                String voltStr = "Sim Volt: " + String(sysState->sim_voltage, 3) + " V";
                updateString(l_raw_voltage, voltStr.c_str(), lv_palette_main(LV_PALETTE_ORANGE));
            }
        }

        const int full_line_y = tank_floor_y - full_h;

        if (pct < 100) {
            lv_obj_set_size(rect_top_red, tank_w, 0);
            int h_blue = (int)(full_h * (pct / 100.0f));
            int y_blue = tank_floor_y - h_blue;
            lv_obj_set_size(rect_bot_blue, tank_w, h_blue);
            lv_obj_set_pos(rect_bot_blue, tank_x, y_blue);
            int h_yellow = full_h - h_blue;
            int y_yellow = y_blue - h_yellow;
            lv_obj_set_size(rect_mid_yellow, tank_w, h_yellow);
            lv_obj_set_pos(rect_mid_yellow, tank_x, y_yellow);
        } else if (pct > 100) {
            lv_obj_set_size(rect_mid_yellow, tank_w, 0);
            lv_obj_set_size(rect_bot_blue, tank_w, full_h);
            lv_obj_set_pos(rect_bot_blue, tank_x, full_line_y);
            int extra_pct = pct - 100;
            int h_red = (int)((max_h - full_h) * (extra_pct / 20.0f));
            if (h_red > (max_h - full_h)) h_red = max_h - full_h;
            int y_red = full_line_y - h_red;
            lv_obj_set_size(rect_top_red, tank_w, h_red);
            lv_obj_set_pos(rect_top_red, tank_x, y_red);
        } else {
            lv_obj_set_size(rect_top_red, tank_w, 0);
            lv_obj_set_size(rect_mid_yellow, tank_w, 0);
            lv_obj_set_size(rect_bot_blue, tank_w, full_h);
            lv_obj_set_pos(rect_bot_blue, tank_x, full_line_y);
        }
    }
};
