#include "TabNetwork.h"
#include "SystemState.h"
#include <WiFi.h>
#include <esp_now.h> 
#include <esp_wifi.h>

void TabNetwork::connectNetwork() {
    WiFi.mode(WIFI_STA);
    delay(20);

    if (esp_now_init() == ESP_OK) {
        esp_now_deinit();
    }
    
    WiFi.disconnect();
    delay(50); 

    if (sysState.connection_type == 0) {
        WiFi.mode(WIFI_OFF);
        Serial.println(F("[NET ENGINE] Wireless radio powered down."));
        return;
    }
    if (sysState.connection_type == 1) {
        if (sysState.wifi_ssid.length() > 0) {
            Serial.printf("[NET ENGINE] WIFI MODE: Initiating router link to SSID: %s\n", sysState.wifi_ssid.c_str());
            
            WiFi.begin(sysState.wifi_ssid.c_str(), sysState.wifi_pass.c_str());
            esp_wifi_set_ps(WIFI_PS_NONE); 

            esp_now_init(); 

            if (sysState.ntp_sync_active) {
                Serial.println(F("[CLOCK] Activating NTP Background Client Engine..."));
                configTzTime("EST5EDT,M3.2.0,M11.1.0", "pool.ntp.org", "time.nist.gov");
            }
        }
        return;
    }
    if (sysState.connection_type == 2) {
        executeDynamicAutoScan();
        return;
    }
    if (sysState.connection_type >= 3) {
        esp_wifi_set_channel(sysState.espnow_channel, WIFI_SECOND_CHAN_NONE);
        if (esp_now_init() == ESP_OK) {
            Serial.printf("[NET ENGINE] Direct Manual ESP-NOW linked to Channel: %d\n", sysState.espnow_channel);
        }
        return;
    }
}

void TabNetwork::executeDynamicAutoScan() {
    Serial.println(F("[NET ENGINE] AUTO MODE: Analyzing airwaves for background channel scoring..."));
    int scanResult = WiFi.scanNetworks();
    
    int channelScores[14] = {0}; 
    for (int i = 0; i < scanResult; i++) {
        int ch = WiFi.channel(i);
        if (ch >= 1 && ch <= 13) {
            channelScores[ch]++; 
        }
    }
    WiFi.scanDelete(); 

    int bestChannel = 1;
    int lowestScore = 999;
    for (int c = 1; c <= 11; c++) {
        if (channelScores[c] < lowestScore) {
            lowestScore = channelScores[c];
            bestChannel = c;
        }
    }

    sysState.espnow_channel = bestChannel;
    sysState.saveToFlash();

    Serial.printf("[NET ENGINE] AUTO MODE: Cleanest slot found: Channel %d. Locking ESP-NOW. Wi-Fi remains OFF.\n", bestChannel);
    
    esp_wifi_set_channel(sysState.espnow_channel, WIFI_SECOND_CHAN_NONE);
    esp_now_init();
}

void TabNetwork::broadcastControlPacket() {
    if (sysState.connection_type == 0) return;
    
    // Safety check: block broadcasts if M5Tough has no active channel to avoid corrupting slave
    if (sysState.espnow_channel == 0) {
        if (WiFi.status() == WL_CONNECTED && WiFi.channel() > 0) {
            sysState.espnow_channel = WiFi.channel();
        } else {
            return; 
        }
    }

    static uint32_t frame_sequence_counter = 0;
    
    // Persistent Automation State Memory Flags
    static bool core_fill_cycle_active = false;
    static bool well_is_resting = false;
    
    // Scaled time-slice tracking counters
    static uint32_t elapsed_cycle_seconds = 0; 

    PoolControlPacket outbound_data;

    // Fetch local time coordinates
    time_t raw_time = time(nullptr);
    struct tm* timeinfo = localtime(&raw_time);
    int current_hour = timeinfo->tm_hour;

    // Extract calibration baselines and rolling averages from encapsulated handle
    float target_full_depth = sysState.offset_in; 
    float smoothed_historical_depth = sysState.getRollingOneHourDepthAverage();
    bool sensor_is_healthy = sysState.ads_hardware_found;

    // =====================================================================
    // 1. SAFETY LOCKOUT: BREAK FILL ON SENSOR FAULT
    // =====================================================================
    if (!sensor_is_healthy) {
        core_fill_cycle_active = false;
        well_is_resting = false;
        elapsed_cycle_seconds = 0;
    }

    // =====================================================================
    // 2. OVERNIGHT LOCKOUT EVALUATION (20:00 to 08:00)
    // =====================================================================
    bool time_is_allowed = sysState.timeAllowed();

    // =====================================================================
    // 3. CORE AUTOMATION DECISION LINK
    // =====================================================================
    // The valve will now wait until your memory-mapped physical depth cushion drop occurs.
    if (sensor_is_healthy && time_is_allowed && !core_fill_cycle_active) {
        if (smoothed_historical_depth < (target_full_depth - sysState.fill_deadband_trigger)) { 
            core_fill_cycle_active = true;
            well_is_resting = false;
            elapsed_cycle_seconds = 0;
            Serial.printf("[AUTOMATION] One-Hour average dropped below deadband threshold limit (Current Avg: %0.2f, Target: %0.2f, Deadband: %0.2f). Starting cycle.\n",
                          smoothed_historical_depth, target_full_depth, sysState.fill_deadband_trigger);
        }
    }

    // Force loop termination if the system exits the allowed operational hours window
    if (!time_is_allowed && core_fill_cycle_active) {
        core_fill_cycle_active = false;
        well_is_resting = false;
        elapsed_cycle_seconds = 0;
    }

    // =====================================================================
    // 4. DYNAMIC TIME-SCALED WELL TIMERS STATE MACHINE
    // =====================================================================
    if (core_fill_cycle_active) {
        elapsed_cycle_seconds++;

        uint32_t target_run_limit_seconds = 1800 / sysState.time_scale_factor;
        uint32_t target_rest_limit_seconds = (sysState.well_rest_selection * 300) / sysState.time_scale_factor;
        if (target_rest_limit_seconds == 0) target_rest_limit_seconds = 1;

        if (!well_is_resting) {
            // VALVE OPEN PHASE (The master is actively commanding a fill right now)
            
            // --- ADD THIS SINGLE LINE HERE ---
            // Natively log exactly 1 second of actual runtime to our history accumulator
            sysState.live_valve_run_seconds_current_hour++;

            if (elapsed_cycle_seconds >= target_run_limit_seconds) {
                well_is_resting = true;
                elapsed_cycle_seconds = 0; 
            }
        } 
        else {
            // VALVE COOLDOWN REST PHASE (The pump is resting, so we don't count seconds)
            if (elapsed_cycle_seconds >= target_rest_limit_seconds) {
                core_fill_cycle_active = false;
                well_is_resting = false;
                elapsed_cycle_seconds = 0;
            }
        }
    }

    // =====================================================================
    // 5. PACKET PAYLOAD ASSIGNMENTS (No Elses)
    // =====================================================================
    outbound_data.master_command_state = 0; // Default status: Closed
    
    if (core_fill_cycle_active && !well_is_resting) outbound_data.master_command_state = 1; // FILL ACTIVE
    if (core_fill_cycle_active && well_is_resting)  outbound_data.master_command_state = 2; // REST RECOVERY

    sysState.active_master_command_state = outbound_data.master_command_state;

    // Update markers and send
    frame_sequence_counter++;
    outbound_data.heartbeat_tick = frame_sequence_counter;
    outbound_data.stick_override_state = 0;
    outbound_data.active_channel = sysState.espnow_channel;

    uint8_t broadcast_mac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    if (!esp_now_is_peer_exist(broadcast_mac)) {
        esp_now_peer_info_t peer_info = {};
        memcpy(peer_info.peer_addr, broadcast_mac, 6);
        peer_info.channel = sysState.espnow_channel;
        peer_info.encrypt = false;
        esp_now_add_peer(&peer_info);
    }
    
    esp_err_t result = esp_now_send(broadcast_mac, (uint8_t*)&outbound_data, sizeof(outbound_data));
    
    if (result == ESP_OK) {
        Serial.print("."); // Safe lightweight serial debug heartbeat marker
        if (frame_sequence_counter % 60 == 0) Serial.println();
    } else {
        Serial.print("X");
    }
}
