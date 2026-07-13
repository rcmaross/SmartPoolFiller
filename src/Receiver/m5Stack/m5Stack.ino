#include <M5Unified.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Preferences.h>

const int RELAY_TRIGGER_PIN = 32;          

// --- System State Profiles ---
bool isRelayOn = false;
bool isMasterConnected = false;
uint32_t lastHeartbeatReceived = 0;

// --- TWO DISTINCT SAFETIES ---
const uint32_t VISUAL_DISCONNECT_ALERT_MS = 5000;   // Alert on screen after 5 seconds of silence
const uint32_t VALVE_SHUTDOWN_TIMEOUT_MS = 60000;   // Force valve closed after 1 minute of silence

// Channel Hopping Sweep Tracking Parameters
uint8_t currentTestChannel = 1;
uint32_t lastChannelHopTime = 0;
const uint32_t HOP_WINDOW_MS = 200;        

// Runtime Tracking Counters
uint32_t valveOpenStartTime = 0;
uint32_t totalElapsedTimeSeconds = 0;

// Override State Registers: 0=AUTO, 1=FORCED_ON, 2=FORCED_OFF
uint8_t stickLocalOverrideMode = 0; 
uint8_t incomingMasterCommandState = 0;

struct __attribute__((packed)) PoolControlPacket {
    uint8_t master_command_state; 
    uint8_t stick_override_state; 
    uint8_t active_channel;       
    uint32_t heartbeat_tick; 
};

// --- INTERCEPT PACKETS DROPPING FROM M5TOUGH (v3.x Core Signature) ---
void onDataReceive(const esp_now_recv_info_t* recvInfo, const uint8_t* data, int len) {
    if (len < (int)sizeof(PoolControlPacket)) return;
    PoolControlPacket* packet = (PoolControlPacket*)data;
    
    uint8_t master_true_channel = packet->active_channel;

    // FIXED: If we catch a bleed frame while scanning an incorrect channel,
    // re-tune the radio core instantly, but DO NOT exit. Let the code fall through
    // to lock the connection flags and commit Channel 1 to your flash memory immediately!
    if (!isMasterConnected && (currentTestChannel != master_true_channel)) {
        Serial.printf("[NET SCAN] Bleed caught on Ch %d. Snapping to true Channel %d...\n", currentTestChannel, master_true_channel);
        currentTestChannel = master_true_channel;
        esp_wifi_set_channel(currentTestChannel, WIFI_SECOND_CHAN_NONE);
    }
    
    // Core state connection locking routines
    if (!isMasterConnected) {
        isMasterConnected = true;
        
        Preferences prefs;
        prefs.begin("stick_cfg", false);
        prefs.putInt("saved_chan", currentTestChannel);
        prefs.end();
        
        Serial.printf("[NET LOCK] Authentic connection locked to Channel %d. Configuration saved.\n", currentTestChannel);
    }
    
    // Always feed your safety watchdogs on every valid frame arrival
    lastHeartbeatReceived = millis();
    incomingMasterCommandState = packet->master_command_state;
    
    // Send bidirectional confirmation payload reply back to your M5Tough
    PoolControlPacket confirmation_reply;
    confirmation_reply.master_command_state = incomingMasterCommandState;
    confirmation_reply.stick_override_state = stickLocalOverrideMode;
    confirmation_reply.active_channel = currentTestChannel;
    confirmation_reply.heartbeat_tick = packet->heartbeat_tick;
    
    esp_now_send(recvInfo->src_addr, (uint8_t*)&confirmation_reply, sizeof(confirmation_reply));
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Power.setExtOutput(true); 

    M5.Lcd.setRotation(1); 
    M5.Lcd.setTextSize(2);
    M5.Lcd.fillScreen(TFT_BLACK);

    Serial.begin(115200);
    delay(200);
    Serial.println(F("\n=== M5STICK RECEIVER WITH FIXED CHANNEL MEMORY ==="));

    pinMode(RELAY_TRIGGER_PIN, OUTPUT);
    digitalWrite(RELAY_TRIGGER_PIN, LOW); 

    // 1. Initialize the base station radio framework layout
    WiFi.mode(WIFI_STA);
    delay(20);

    // 2. Read your last successful locked channel parameter out of flash memory
    Preferences prefs;
    prefs.begin("stick_cfg", true);
    currentTestChannel = prefs.getInt("saved_chan", 1); 
    prefs.end();
    
    Serial.printf("[BOOT] Loaded last valid channel out of flash memory: Channel %d\n", currentTestChannel);

    // 3. Initialize the ESP-NOW core listening stack FIRST
    if (esp_now_init() == ESP_OK) {
        Serial.println(F("[NET] ESP-NOW core ready. Locking communication callback handles..."));
        esp_now_register_recv_cb(onDataReceive);
        lastChannelHopTime = millis();
        
        // 4. FIXED: Lock your physical transceiver channel AFTER the core engine boots up!
        // This stops esp_now_init() from wiping your flash memory setting.
        esp_wifi_set_channel(currentTestChannel, WIFI_SECOND_CHAN_NONE);
        Serial.printf("[NET] Physical transceiver core locked cleanly onto Channel %d\n", currentTestChannel);
    } else {
        Serial.println(F("[CRITICAL ERROR] Failed to initialize ESP-NOW layer!"));
    }
}

void loop() {
    M5.update();
    uint32_t nowTime = millis();

    // --- MANAGE OVERRIDE BUTTON PRESSES ---
    if (M5.BtnA.wasPressed() && M5.BtnB.isPressed()) {
        stickLocalOverrideMode = 0; // Return control to Tough
    } else if (M5.BtnB.wasPressed()) {
        stickLocalOverrideMode = 1; // SIDE BUTTON: FORCE ON
    } else if (M5.BtnA.wasPressed()) {
        stickLocalOverrideMode = 2; // FRONT BUTTON: LOCK OFF
    }

    // --- FREQUENCY CHANNEL-HOPPING SCANNER ENGINE ---
    if (!isMasterConnected) {
        if (nowTime - lastChannelHopTime >= HOP_WINDOW_MS) {
            lastChannelHopTime = nowTime;
            currentTestChannel++;
            if (currentTestChannel > 11) currentTestChannel = 1; 
            esp_wifi_set_channel(currentTestChannel, WIFI_SECOND_CHAN_NONE);
        }
    }

    // =====================================================================
    // SEPARATED SAFETIES WATCHDOG TIMEOUTS
    // =====================================================================
    // Watchdog 1: 5-second drop switches screen display alert back to scanning mode
    if (isMasterConnected && (nowTime - lastHeartbeatReceived > VISUAL_DISCONNECT_ALERT_MS)) {
        isMasterConnected = false; 
        lastChannelHopTime = nowTime; 
    }

    // Watchdog 2: 1-minute drop acts as a hard cutoff, forcing the physical valve shut
    bool connection_is_severed_completely = (nowTime - lastHeartbeatReceived > VALVE_SHUTDOWN_TIMEOUT_MS);
    if (connection_is_severed_completely) {
        incomingMasterCommandState = 0; // Clear master directives
    }

    // Determine target relay state
    bool target_relay_wire = false;
    
    if (stickLocalOverrideMode == 0) {
        target_relay_wire = (incomingMasterCommandState == 1);
    } else if (stickLocalOverrideMode == 1) {
        target_relay_wire = true; // Manual Force On override active
    } else if (stickLocalOverrideMode == 2) {
        target_relay_wire = false; // Manual Lock Off override active
    }

    // Apply safety cutoff overrides
    if (connection_is_severed_completely && stickLocalOverrideMode == 0) {
        target_relay_wire = false; // Force closed on communication loss if running in AUTO
    }

    // Commit changes to the physical valve pin
    if (target_relay_wire != isRelayOn) {
        isRelayOn = target_relay_wire;
        digitalWrite(RELAY_TRIGGER_PIN, isRelayOn ? HIGH : LOW);
        valveOpenStartTime = isRelayOn ? nowTime : 0;
    }

    // Runtime clocks
    if (isRelayOn && valveOpenStartTime > 0) {
        totalElapsedTimeSeconds = (nowTime - valveOpenStartTime) / 1000;
    } else {
        totalElapsedTimeSeconds = 0;
    }

    // =====================================================================
    // REFRESH STICK HUD DISPLAY GRAPHICS
    // =====================================================================
    M5.Lcd.setCursor(10, 10);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.print("LINK: ");
    if (isMasterConnected) {
        M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
        M5.Lcd.printf("CH %d     \n", currentTestChannel); 
    } else {
        M5.Lcd.setTextColor(TFT_RED, TFT_BLACK);
        M5.Lcd.print("SCANNING  \n");
    }

    M5.Lcd.setCursor(10, 34);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.print("MODE: ");
    if (stickLocalOverrideMode == 0) {
        M5.Lcd.setTextColor(TFT_BLUE, TFT_BLACK); M5.Lcd.print("AUTOMATIC\n");
    } else if (stickLocalOverrideMode == 1) {
        M5.Lcd.setTextColor(TFT_YELLOW, TFT_BLACK); M5.Lcd.print("FORCE ON \n");
    } else {
        M5.Lcd.setTextColor(TFT_MAGENTA, TFT_BLACK); M5.Lcd.print("LOCK OFF \n");
    }

    M5.Lcd.setCursor(10, 58);
    M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Lcd.print("VALVE: ");
    if (isRelayOn) {
        M5.Lcd.setTextColor(TFT_GREEN, TFT_BLACK);
        M5.Lcd.printf("ON (%ds)    \n", totalElapsedTimeSeconds);
    } else {
        if (incomingMasterCommandState == 2 && stickLocalOverrideMode == 0) {
            M5.Lcd.setTextColor(TFT_ORANGE, TFT_BLACK); M5.Lcd.print("REST (WELL)\n");
        } else {
            M5.Lcd.setTextColor(TFT_CYAN, TFT_BLACK); M5.Lcd.print("OFF        \n");
        }
    }

    delay(30);
}
