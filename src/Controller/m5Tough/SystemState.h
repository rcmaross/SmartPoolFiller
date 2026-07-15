#pragma once
#include <Arduino.h>
#include <Preferences.h>

struct SystemState {
    // --- Existing Hardware & Calibration State ---
    float empty_volts = 0.6f;
    float full_volts = 3.0f;
    float offset_in = 60.0f;
    float sim_voltage = 1.0f;
    bool use_metric = false;
    bool ads_hardware_found = false;

    // --- Existing Network Tracking State ---
    int system_id = 1; // default system to allow multiple sets on 1 wifi.
    int connection_type = 1;     // 0=OFF, 1=WIFI, 2=AUTO, 3+=MANUAL
    int espnow_channel = 1;      
    String wifi_ssid = "";
    String wifi_pass = "";
    uint8_t mac_address[6] = {0};
    bool is_peer_connected = false;

    // --- Fill Windows & Well Protection State ---
    int allow_fill_start_hour = 20;   
    int allow_fill_end_hour = 8;      
    bool well_mode_active = false;   
    int well_max_run_mins = 30;      
    int well_rest_selection = 1; // 0=None, 1=5min, 2=10min, 3=15min      

    // --- Clock Configuration Sync Preferences ---
    bool ntp_sync_active = true;     
    int timezone_offset_hours = -5;  
    String tz_posix_rule = "EST5EDT,M3.2.0,M11.1.0"; 
    bool use_24hr_format = false;

    // scale time for testing.  perhaps someday we can make this configurable?
    //uint32_t time_scale_factor = 360; // 1hr = 10s 
    uint32_t time_scale_factor = 1; //real world
    float fill_deadband_trigger = 0.25f; 

    // 60-sample historical depth tracker array (representing 60 sequential minutes)
    float hourly_depth_history[60] = {0.0f};
    int history_write_index = 0;
    // Tracks active physical valve open seconds in real-time
    uint32_t live_valve_run_seconds_current_hour = 0;
    
    // (0=STANDBY/OFF, 1=FILLING, 2=WELL RECOVERY REST)
    uint8_t active_master_command_state = 0; 

    // Helper method to pull the smoothed rolling historical depth average
    float getRollingOneHourDepthAverage() {
        float sum = 0.0f;
        for (int i = 0; i < 60; i++) sum += hourly_depth_history[i];
        return sum / 60.0f;
    }

    bool timeAllowed() {
        // Fetch local time coordinates
        time_t raw_time = time(nullptr);
        struct tm* timeinfo = localtime(&raw_time);
        int current_hour = timeinfo->tm_hour;
        bool time_is_allowed = false;
        if (allow_fill_start_hour > allow_fill_end_hour) {
            if (current_hour >= allow_fill_start_hour || current_hour < allow_fill_end_hour) 
                return true;
        } else {
            if (current_hour >= allow_fill_start_hour && current_hour < allow_fill_end_hour) 
                return true;
        }    
        return false;
    }

    void saveToFlash() {
        Preferences prefs;
        prefs.begin("pool_cfg", false);
        
        prefs.putInt("system_id", system_id);

        prefs.putFloat("v_empty", empty_volts);
        prefs.putFloat("v_full", full_volts);
        prefs.putFloat("v_offset", offset_in);
        prefs.putBool("u_metric", use_metric);
        prefs.putFloat("f_deadband", fill_deadband_trigger);

        prefs.putInt("net_type", connection_type);
        prefs.putInt("now_chan", espnow_channel);
        prefs.putString("w_ssid", wifi_ssid);
        prefs.putString("w_pass", wifi_pass);
        
        prefs.putInt("f_start", allow_fill_start_hour);
        prefs.putInt("f_end", allow_fill_end_hour);
        prefs.putBool("w_active", well_mode_active);
        prefs.putInt("w_run", well_max_run_mins);
        prefs.putInt("w_rest", well_rest_selection);
        
        prefs.putBool("t_ntp", ntp_sync_active);
        prefs.putInt("t_tz", timezone_offset_hours);
        prefs.putString("t_tzrule", tz_posix_rule);
        prefs.putBool("t_format", use_24hr_format);
        
        prefs.end();
    }

    void loadFromFlash() {
        Preferences prefs;
        prefs.begin("pool_cfg", true);
        
        system_id = prefs.getInt("system_id", 1);

        empty_volts = prefs.getFloat("v_empty", 0.6f);
        full_volts = prefs.getFloat("v_full", 3.0f);
        offset_in = prefs.getFloat("v_offset", 60.0f);
        use_metric = prefs.getBool("u_metric", false);
        fill_deadband_trigger = prefs.getFloat("f_deadband", 0.25f);

        connection_type = prefs.getInt("net_type", 1);
        espnow_channel = prefs.getInt("now_chan", 1);
        wifi_ssid = prefs.getString("w_ssid", "");
        wifi_pass = prefs.getString("w_pass", "");
        
        allow_fill_start_hour = prefs.getInt("f_start", 20);
        allow_fill_end_hour = prefs.getInt("f_end", 8);
        well_mode_active = prefs.getBool("w_active", false);
        well_max_run_mins = prefs.getInt("w_run", 30);
        well_rest_selection = prefs.getInt("w_rest", 1);
        
        ntp_sync_active = prefs.getBool("t_ntp", true);
        timezone_offset_hours = prefs.getInt("t_tz", -5);
        tz_posix_rule = prefs.getString("t_tzrule", "EST5EDT,M3.2.0,M11.1.0");
        use_24hr_format = prefs.getBool("t_format", false);
        
        prefs.end();
    }

    const char* getPoolStatus(int pct) {
        if (pct >= 95 && pct <= 100)
            return("FULL");
        else if (pct > 100)
            return("HIGH");
        else if (pct < 95 && pct >= 90)
            return("LOW");
        else if (pct < 90 && pct > 0)
            return("CRITICAL");
        else
            return("EMPTY / NO SIGNAL");
    }

    void getInstantaneousPoolMetrics(int& out_pct, float& out_depth, const char*& out_status) {
        float volts = sim_voltage;
        if (volts < empty_volts) volts = empty_volts;
        
        float span = full_volts - empty_volts;
        float pct_f = 0.0f;
        if (span > 0.001f) {
            pct_f = ((volts - empty_volts) / span) * 100.0f;
        }
        out_pct = (int)pct_f;

        out_depth = (pct_f / 100.0f) * offset_in;
        if (use_metric)
            out_depth *= 2.54f;

        out_status = getPoolStatus(out_pct);
    }

    void getPoolMetrics(int& out_pct, float& out_depth, const char*& out_status) {
        float pct_f = 0.0f;

        out_depth = getRollingOneHourDepthAverage();

        float offset = offset_in;
        if (use_metric)
            offset *= 2.54f;
        pct_f = ((out_depth)/offset) * 100.0f;
        out_pct = (int)pct_f;

        out_status = getPoolStatus(out_pct);
    }

    void initHistory(){
        int init_pct = 0; float init_depth = 0.0f; const char* init_status = "";
        getInstantaneousPoolMetrics(init_pct, init_depth, init_status);

        for (int i = 0; i < 60; i++) {
            hourly_depth_history[i] = init_depth;
        }

        history_write_index = 0;
    }
};

struct __attribute__((packed)) PoolControlPacket {
    uint8_t system_id; // 1-8 - for distingushing multiple pairs
    uint8_t reserved_1;
    uint16_t reserved_2;
    uint8_t reserved_3;
    uint8_t master_command_state; // Master: 0=OFF, 1=FILL, 2=WELL_REST
    uint8_t stick_override_state; // Stick Feedback: 0=AUTO, 1=FORCED_ON, 2=FORCED_OFF
    uint8_t active_channel;       
    uint32_t heartbeat_tick;      
};

extern SystemState sysState;
