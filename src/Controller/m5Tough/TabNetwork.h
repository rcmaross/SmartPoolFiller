#pragma once
#include <lvgl.h>
#include "BaseTab.h"

class TabNetwork : public BaseTab {
private:
    // --- Row 1 Elements: ESP-NOW Configuration Deck ---
    lv_obj_t* btn_espnow_mode = nullptr; 
    lv_obj_t* l_espnow_text = nullptr;    
    lv_obj_t* l_peer_status = nullptr;   

    // --- Wifi Group Sub-Container ---
    lv_obj_t* container_wifi_group = nullptr; 
    lv_obj_t* dd_wifi_ssids = nullptr;       
    lv_obj_t* ta_wifi_pass = nullptr;        
    lv_obj_t* cb_show_pass = nullptr; // FIXED: Variable declared inside header class boundary

    // --- Shared System Component ---
    lv_obj_t* kb_system_deck = nullptr;       

    // Internal scanner protection tracking flag
    bool is_scanning_networks = false;

    // Internal static callback handlers required by LVGL API rules
    static void espnow_cycle_cb(lv_event_t* e);
    static void wifi_dropdown_click_cb(lv_event_t* e);
    static void password_field_focus_cb(lv_event_t* e);
    static void keyboard_action_done_cb(lv_event_t* e);
    static void network_option_select_cb(lv_event_t* e);
    static void show_pass_toggle_cb(lv_event_t* e); 

public:
    void setup(lv_obj_t* tab_container);
    void update();
    void connectNetwork();
    void executeDynamicAutoScan();
    const char* getWifiStatusIcon(lv_color_t& out_color);
    void broadcastControlPacket();
};
