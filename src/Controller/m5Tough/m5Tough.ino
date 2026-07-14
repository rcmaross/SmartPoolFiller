#define LGFX_NOT_USE_FONT_2      1  
#define LGFX_NOT_USE_FONT_4      1
#define LGFX_NOT_USE_FONT_6      1
#define LGFX_NOT_USE_FONT_7      1
#define LGFX_NOT_USE_FONT_8      1

#include <M5Unified.h>
#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>
#include <Wire.h>               
#include <WiFi.h>
#include <ArduinoOTA.h>

#include "SystemState.h"
#include "PoolSensor.h"       
#include "TabMainDisplay.h"
#include "TabCalibration.h"
#include "TabSettings.h"
#include "TabNetwork.h"
#include "TabHistory.h"

SystemState sysState;
PoolSensor* activeSensor = nullptr; 
lv_obj_t* sl_status_label = nullptr;
lv_obj_t* sl_wifi_icon_label = nullptr; 

static const uint32_t screenWidth  = 320;
static const uint32_t screenHeight = 240;
static uint8_t * draw_buf = nullptr;

TabMainDisplay *tabMainDisplay = nullptr;
TabCalibration *tabCalibration = nullptr;
TabNetwork *tabNetwork = nullptr;
TabSettings *tabSettings = nullptr;
TabHistory *tabHistory = nullptr;

unsigned long lastHardwareSample = 0;
const unsigned long HARDWARE_INTERVAL = 1000; // ms 1000ms = 1s
const unsigned long UI_INTERVAL = 1000; // ms 1000ms = 1s

void my_disp_flush(lv_display_t * d, const lv_area_t * area, uint8_t * px) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    M5.Display.pushImage(area->x1, area->y1, w, h, (uint16_t *)px);
    lv_display_flush_ready(d);
}

void my_touchpad_read(lv_indev_t * indev, lv_indev_data_t * data) {
    M5.update();
    auto touch = M5.Touch.getDetail();
    if (touch.isPressed()) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = touch.x;
        data->point.y = touch.y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// FIXED: Converted to classic v2.x naming rules for stable compilation
void WiFiEventTracker(WiFiEvent_t event, WiFiEventInfo_t info) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_START:
            Serial.println(F("[DIAGNOSTIC] WiFi Station Interface Active."));
            break;
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.println(F("[DIAGNOSTIC] Connected to AP. Exchanging keys..."));
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.print(F("[DIAGNOSTIC] DHCP Success! IP Assigned: "));
            Serial.println(WiFi.localIP().toString());
            if (WiFi.status() == WL_CONNECTED) {
                uint8_t currentAPChannel = WiFi.channel();
                if (currentAPChannel > 0) sysState.espnow_channel = currentAPChannel;
            }
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            // FIXED: Changed property back from .reason_code to classic .reason
            Serial.printf("[DIAGNOSTIC LINK FAIL] Disconnected. Reason Code: %d\n", 
                          info.wifi_sta_disconnected.reason);
            break;
        default:
            break;
    }
}

lv_obj_t* sl_time_label = nullptr; 

void setup_ui() {
    // Top Row Master Bar Container canvas layer
    lv_obj_t * sb = lv_obj_create(lv_screen_active());
    lv_obj_set_size(sb, LV_PCT(100), 40);
    lv_obj_align(sb, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(sb, lv_color_black(), 0);
    lv_obj_set_style_border_width(sb, 0, 0);
    lv_obj_set_style_radius(sb, 0, 0);
    lv_obj_set_style_pad_all(sb, 0, 0);

    // Left Side Text Label: Tracks Level %, Depth, and State Notes
    sl_status_label = lv_label_create(sb);
    lv_label_set_text(sl_status_label, "LVL: --% | INIT");
    lv_obj_set_style_text_color(sl_status_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(sl_status_label, &lv_font_montserrat_14, 0);
    lv_obj_align(sl_status_label, LV_ALIGN_LEFT_MID, 10, 0);

    // NEW: CENTER TIME LABEL (Sits perfectly in the middle of your bar space)
    sl_time_label = lv_label_create(sb);
    lv_label_set_text(sl_time_label, "00:00");
    lv_obj_set_style_text_color(sl_time_label, lv_palette_main(LV_PALETTE_GREY), 0); // Soft grey accent tint
    lv_obj_set_style_text_font(sl_time_label, &lv_font_montserrat_14, 0);
    lv_obj_align(sl_time_label, LV_ALIGN_LEFT_MID, 213, 0); 

    // Right Side Icon Label: Universal Wi-Fi Connectivity Panel
    sl_wifi_icon_label = lv_label_create(sb);
    lv_label_set_text(sl_wifi_icon_label, "");
    lv_obj_set_style_text_font(sl_wifi_icon_label, &lv_font_montserrat_14, 0);
    lv_obj_align(sl_wifi_icon_label, LV_ALIGN_RIGHT_MID, -10, 0);

    // Master TabView grid configurations
    lv_obj_t * tv = lv_tabview_create(lv_screen_active());
    lv_tabview_set_tab_bar_position(tv, LV_DIR_BOTTOM);
    lv_tabview_set_tab_bar_size(tv, 40);
    lv_obj_set_size(tv, LV_PCT(100), 200);
    lv_obj_align(tv, LV_ALIGN_BOTTOM_MID, 0, 0);
    
    lv_obj_t * t1 = lv_tabview_add_tab(tv, LV_SYMBOL_TINT);     
    lv_obj_t * t2 = lv_tabview_add_tab(tv, LV_SYMBOL_SETTINGS); 
    lv_obj_t * t3 = lv_tabview_add_tab(tv, LV_SYMBOL_EDIT);     
    lv_obj_t * t4 = lv_tabview_add_tab(tv, LV_SYMBOL_WIFI);     
    lv_obj_t * t5 = lv_tabview_add_tab(tv, LV_SYMBOL_EYE_OPEN); 

    lv_obj_t * tb = lv_tabview_get_tab_bar(tv);
    lv_obj_set_style_text_font(tb, &lv_font_montserrat_32, 0);
    lv_obj_set_style_bg_color(tb, lv_color_white(), 0);
    lv_obj_set_style_text_color(tb, lv_color_black(), 0);

    for(int i = 0; i < 5; i++) {
        lv_obj_t * button = lv_obj_get_child(tb, i);
        lv_obj_set_style_bg_color(button, lv_color_white(), LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_CHECKED);
        lv_obj_remove_local_style_prop(button, LV_STYLE_TEXT_COLOR, LV_STATE_ANY);
        lv_obj_set_style_text_color(button, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN | LV_STATE_CHECKED);
    }

    if (tabMainDisplay != nullptr) tabMainDisplay->setup(t1);  
    if (tabSettings != nullptr)    tabSettings->setup(t2);
    if (tabCalibration != nullptr) tabCalibration->setup(t3);
    if (tabNetwork != nullptr)     tabNetwork->setup(t4);
    if (tabHistory != nullptr)     tabHistory->setup(t5); 
}

void hw_loop(unsigned long currentMillis) {
    if (activeSensor != nullptr) {
        activeSensor->update(); 
        sysState.sim_voltage = activeSensor->getVoltage();
        sysState.ads_hardware_found = activeSensor->isHardwarePresent() && !activeSensor->isFaulted();
    }

    // =====================================================================
    // INTEGRATED: TIME-SCALED HISTORY ACCUMULATOR
    // =====================================================================
    static uint32_t last_history_sample_time = 0;
    
    // Real-world sample window is 60,000ms (1 min).
    uint32_t scaled_sample_interval = 60000 / sysState.time_scale_factor;

    if (currentMillis - last_history_sample_time >= scaled_sample_interval) {
        last_history_sample_time = currentMillis;
        
        int pct; float instant_depth; const char* status;
        sysState.getInstantaneousPoolMetrics(pct, instant_depth, status);

        // Append instant depth readings cleanly into the encapsulated struct history buffer
        sysState.hourly_depth_history[sysState.history_write_index] = instant_depth;
        sysState.history_write_index++;
        
        if (sysState.history_write_index >= 60) {
            sysState.history_write_index = 0;
        }
    }

    if (tabNetwork != nullptr) {
        tabNetwork->broadcastControlPacket(); // Triggers the radio directly on Core 1
    }

    int pct = 0; float depth = 0.0f; const char* status = "";
    sysState.getPoolMetrics(pct, depth, status);
    
    if (sl_status_label) {
        char sb_buf[64];
        if (activeSensor != nullptr && activeSensor->isHardwarePresent() && activeSensor->isFaulted()) {
            snprintf(sb_buf, sizeof(sb_buf), "LEVEL: ERROR | HW LOSS");
        } else {
            snprintf(sb_buf, sizeof(sb_buf), "LVL: %d%% | %0.1f%s | %s", 
                     pct, depth, (sysState.use_metric ? "cm" : "in"), status);
        }
        lv_label_set_text(sl_status_label, sb_buf);
    }

    // --- REAL-TIME HARDWARE RTC CENTER CLOCK CONTROLLER ---
    if (sl_time_label) {
        time_t raw_time = time(nullptr);
        struct tm* local_time = localtime(&raw_time);

        char clk_buf[16] = {0}; 
        
        if (sysState.use_24hr_format) {
            // Standard 24-Hour Military Layout (e.g., 14:05)
            snprintf(clk_buf, sizeof(clk_buf), "%02d:%02d", local_time->tm_hour, local_time->tm_min);
        } else {
            // Standard 12-Hour Layout with AM/PM Designators
            int display_hour = local_time->tm_hour % 12;
            if (display_hour == 0) display_hour = 12; // Handle midnight/noon zero offsets
            const char* period = (local_time->tm_hour >= 12) ? "PM" : "AM";
            
            snprintf(clk_buf, sizeof(clk_buf), "%d:%02d %s", display_hour, local_time->tm_min, period);
        }
        
        lv_label_set_text(sl_time_label, clk_buf);
    }

    if (sl_wifi_icon_label && tabNetwork != nullptr) {
        lv_color_t top_bar_icon_color;
        const char* top_bar_icon_symbol = tabNetwork->getWifiStatusIcon(top_bar_icon_color);
        lv_label_set_text(sl_wifi_icon_label, top_bar_icon_symbol);
        lv_obj_set_style_text_color(sl_wifi_icon_label, top_bar_icon_color, 0);
    }

}
void loop() {
    unsigned long currentMillis = millis();

    ArduinoOTA.handle();

    if (currentMillis - lastHardwareSample >= HARDWARE_INTERVAL) {
        lastHardwareSample = currentMillis;
        hw_loop(currentMillis);
    }
    
    static uint32_t lastMenuUpdate = 0;
    if (currentMillis - lastMenuUpdate >= UI_INTERVAL) {
        lastMenuUpdate = currentMillis;
        
        if (tabMainDisplay != nullptr)  tabMainDisplay->update();
        if (tabSettings != nullptr)     tabSettings->update();
        if (tabCalibration != nullptr)  tabCalibration->update();
        if (tabNetwork != nullptr)      tabNetwork->update();
        if (tabHistory != nullptr)      tabHistory->update();
    }

    static uint32_t last_tick = 0;
    uint32_t now = currentMillis;
    lv_tick_inc(now - last_tick);
    last_tick = now;

    lv_timer_handler(); 
    delay(5);
}

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Power.setExtOutput(true); 

    Serial.begin(115200); 
    
    sysState.loadFromFlash();

    tabMainDisplay = new TabMainDisplay();
    tabSettings    = new TabSettings();
    tabCalibration = new TabCalibration();
    tabNetwork     = new TabNetwork();
    tabHistory     = new TabHistory();

    Wire.begin(33, 32);
    Wire.setClock(10000); 
    Wire.setTimeOut(50);
    
    if (PoolSensor::probeHardware(Wire)) {
        Serial.println(F("[BOOT] Physical Silicon verified on I2C bus."));
        activeSensor = new PhysicalAdcSensor(Wire);

    } else {
        Serial.println(F("[BOOT] Bus lines open or unpowered. Launching Simulator..."));
        activeSensor = new SimulatedSerialSensor();
    }
    activeSensor->begin();
    sysState.sim_voltage = activeSensor->getVoltage();

    sysState.initHistory();

    WiFi.onEvent(WiFiEventTracker);

    lv_init();

    lv_display_t * disp = lv_display_create(screenWidth, screenHeight);
    lv_display_set_flush_cb(disp, my_disp_flush);

    uint32_t buf_size = screenWidth * 2 * sizeof(lv_color_t);
    draw_buf = (uint8_t*)malloc(buf_size);
    
    if (draw_buf != nullptr) {
        lv_display_set_buffers(disp, draw_buf, NULL, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
        Serial.println("[BOOT] Graphics buffer allocated.");
    } else {
        Serial.println("[ERROR] PSRAM allocation failed!");
    }
    
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, my_touchpad_read);

    setup_ui();
    tabNetwork->connectNetwork(); 

    ArduinoOTA.setHostname("SmartPoolFiller-Tough");
    ArduinoOTA.setPassword("admin");
    ArduinoOTA.setRebootOnSuccess(true); 

    Serial.println("\n--- HARDWARE MEMORY AUDIT ---");
    size_t freeInternalRAM = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t totalInternalRAM = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    size_t usedInternalRAM = totalInternalRAM - freeInternalRAM;
    Serial.printf("Internal DRAM Used: %d bytes\n", usedInternalRAM);
    Serial.printf("Internal DRAM Free: %d bytes\n", freeInternalRAM);
    size_t freePSRAM = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    Serial.printf("External PSRAM Free: %d bytes\n", freePSRAM);
    Serial.println("-----------------------------\n");

    ArduinoOTA.begin();

}
