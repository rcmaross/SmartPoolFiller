#pragma once
#include "BaseTab.h"
#include "SystemState.h"

class TabMainDisplay : public BaseTab {
private:
    lv_obj_t* l_measurement = nullptr;
    lv_obj_t* l_inches_delta = nullptr; 
    lv_obj_t* l_live_measure = nullptr;
    lv_obj_t* l_raw_voltage = nullptr;

    lv_obj_t* l_valve_state = nullptr;

    lv_obj_t* rect_top_red = nullptr;    
    lv_obj_t* rect_mid_yellow = nullptr; 
    lv_obj_t* rect_bot_blue = nullptr;   

    const int tank_x = 25;       
    const int tank_w = 35;       
    const int max_h  = 130;      
    const int full_h = 108;      

public:
    void setup(lv_obj_t* tab_container) override {
        lv_obj_set_layout(tab_container, 0);
        lv_obj_set_style_pad_all(tab_container, 0, 0);

        // Hollow Outer Tank Framing Container
        lv_obj_t* tank_bg = lv_obj_create(tab_container);
        lv_obj_set_size(tank_bg, tank_w + 4, max_h + 4); 
        lv_obj_set_pos(tank_bg, tank_x - 2, 13);
        lv_obj_set_style_bg_opa(tank_bg, LV_OPA_TRANSP, 0); 
        lv_obj_set_style_border_width(tank_bg, 2, 0);
        lv_obj_set_style_border_color(tank_bg, lv_palette_main(LV_PALETTE_GREY), 0); 
        lv_obj_set_style_radius(tank_bg, 2, 0);
        lv_obj_set_style_pad_all(tank_bg, 0, 0);
        lv_obj_remove_flag(tank_bg, LV_OBJ_FLAG_SCROLLABLE);

        // UI Shapes via BaseTab Factory Constructors
        rect_top_red    = createRectangle(tab_container, tank_w, LV_SIZE_CONTENT, lv_palette_main(LV_PALETTE_RED));        
        rect_mid_yellow = createRectangle(tab_container, tank_w, LV_SIZE_CONTENT, lv_palette_main(LV_PALETTE_YELLOW));        
        rect_bot_blue   = createRectangle(tab_container, tank_w, LV_SIZE_CONTENT, lv_palette_main(LV_PALETTE_BLUE));        

        // Set static boot colors right here at startup
        l_measurement  = createString(tab_container, "0.0 in", 24, 110, 20, lv_color_black()); 
        l_inches_delta = createString(tab_container, "0.0 in", 18, 110, 52, lv_palette_main(LV_PALETTE_GREY)); 
        l_valve_state  = createString(tab_container, "VALVE: OFFLINE", 14, 110, 75, lv_palette_main(LV_PALETTE_GREY));
        l_live_measure = createString(tab_container, "0.0 in (0.0in)", 14, 110, 92, lv_palette_main(LV_PALETTE_GREY));
        l_raw_voltage  = createString(tab_container, "Sensor: 0.000 V", 14, 110, 110, lv_color_black());
    }

    void update() override {
        if (!l_measurement || !l_inches_delta || !l_live_measure || !l_raw_voltage || !l_valve_state ||
            !rect_top_red || !rect_mid_yellow || !rect_bot_blue) return;

        int pct = 0; float poolDepth = 0.0f; const char* status = "";
        sysState.getPoolMetrics(pct, poolDepth, status);

        int unused_pct = 0; float instantPoolDepth = 0.0f; const char* unused_status = "";
        sysState.getInstantaneousPoolMetrics(unused_pct, instantPoolDepth, unused_status);

        String depthStr = ""; String deltaStr = "";
        String instantDepthStr = ""; String instantDeltaStr = "";

        // =====================================================================
        // MASTER STATE VALVING MODIFIER (Driven cleanly by your master core states)
        // =====================================================================
        if (sysState.connection_type == 0) {
            // Wireless radio is powered down entirely by operator preference
            updateString(l_valve_state, "RADIO OFF", lv_palette_main(LV_PALETTE_GREY));
        } 
        else if (!sysState.timeAllowed()) {
            updateString(l_valve_state, "VALVE: DELAYED", lv_palette_main(LV_PALETTE_GREY));
        } else {
            // Read the exact command state your background transmission thread is broadcasting
            if (sysState.active_master_command_state == 1) {
                    updateString(l_valve_state, "VALVE: FILLING", lv_palette_main(LV_PALETTE_RED));

            } 
            else if (sysState.active_master_command_state == 2) {
                updateString(l_valve_state, "VALVE: RESTING", lv_palette_main(LV_PALETTE_ORANGE));
            } 
            else {
                updateString(l_valve_state, "VALVE: STANDBY", lv_palette_main(LV_PALETTE_GREEN));
            }
        }

        if (!sysState.use_metric) {
            depthStr = String(poolDepth, 1) + " in";
            float inchesFromFull = poolDepth - sysState.offset_in;
            if (inchesFromFull > 0.05f)        deltaStr = "+" + String(inchesFromFull, 1) + " in";
            else if (inchesFromFull < -0.05f)  deltaStr = String(inchesFromFull, 1) + " in";
            else                               deltaStr = "0.0 in";

            instantDepthStr = String(instantPoolDepth, 1) + " in";
            inchesFromFull = instantPoolDepth - sysState.offset_in;
            if (inchesFromFull > 0.05f)        instantDeltaStr = "+" + String(inchesFromFull, 1) + " in";
            else if (inchesFromFull < -0.05f)  instantDeltaStr = String(inchesFromFull, 1) + " in";
            else                               instantDeltaStr = "0.0 in";

        } else {
            depthStr = String(poolDepth, 1) + " cm";
            float targetCm = sysState.offset_in * 2.54f;
            float cmFromFull = poolDepth - targetCm;
            if (cmFromFull > 0.1f)             deltaStr = "+" + String(cmFromFull, 1) + " cm";
            else if (cmFromFull < -0.1f)       deltaStr = String(cmFromFull, 1) + " cm";
            else                               deltaStr = "0.0 cm";

            instantDepthStr = String(instantPoolDepth, 1) + " cm";
            cmFromFull = instantPoolDepth - targetCm;
            if (cmFromFull > 0.1f)             instantDeltaStr = "+" + String(cmFromFull, 1) + " cm";
            else if (cmFromFull < -0.1f)       instantDeltaStr = String(cmFromFull, 1) + " cm";
            else                               instantDeltaStr = "0.0 cm";

        }

        // 2 ARGUMENTS MATCHING: Uses text-only variant. Background font color is completely UNCHANGED
        updateString(l_measurement, depthStr.c_str());
        updateString(l_inches_delta, deltaStr.c_str());

        // Combine the live depth and its deviation delta together into a single string
        String fullLiveStr = "Inst: " + instantDepthStr + " (" + instantDeltaStr + ")";
        updateString(l_live_measure, fullLiveStr.c_str());

        // Process hardware diagnostics
        if (sysState.ads_hardware_found) {
            String voltStr = "Raw Volt: " + String(sysState.sim_voltage, 3) + " V";
            updateString(l_raw_voltage, voltStr.c_str(), lv_palette_main(LV_PALETTE_BLUE));
        } else {
            if (sysState.sim_voltage <= 0.02f) {
                updateString(l_raw_voltage, "LOOP DISCONNECTED", lv_palette_main(LV_PALETTE_RED));
                updateString(l_measurement, "FAULT", lv_palette_main(LV_PALETTE_RED));
                updateString(l_inches_delta, "OFFLINE", lv_palette_main(LV_PALETTE_RED));
            } else {
                String voltStr = "Sim Volt: " + String(sysState.sim_voltage, 3) + " V";
                updateString(l_raw_voltage, voltStr.c_str(), lv_palette_main(LV_PALETTE_ORANGE));
            }
        }

        // Geometric Layout Sizing
        const int tank_floor_y = 15 + max_h;            
        const int full_line_y  = tank_floor_y - full_h;  

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
