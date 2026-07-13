#include "TabNetwork.h"
#include "SystemState.h"
#include <WiFi.h>

// --- Static Callback Logical Routines ---
void TabNetwork::espnow_cycle_cb(lv_event_t* e) {
    TabNetwork* thiz = (TabNetwork*)lv_event_get_user_data(e);
    if (!thiz) return;
    
    if (sysState.connection_type == 1)      sysState.connection_type = 2; // WIFI -> AUTO
    else if (sysState.connection_type == 2) { sysState.connection_type = 3; sysState.espnow_channel = 1; } // AUTO -> CH 1
    else if (sysState.connection_type >= 3) {
        sysState.espnow_channel++;
        if (sysState.espnow_channel > 11) { sysState.connection_type = 0; sysState.espnow_channel = 1; } // CH 11 -> OFF
    } else {
        sysState.connection_type = 1; // OFF -> WIFI
    }
    
    sysState.saveToFlash();
    thiz->connectNetwork(); 
}

void TabNetwork::network_option_select_cb(lv_event_t* e) {
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    TabNetwork* thiz = (TabNetwork*)lv_event_get_user_data(e);
    if (!thiz || !btn) return;
    
    lv_obj_t* label = lv_obj_get_child(btn, 1); 
    if (!label) return;

    const char* selectedName = lv_label_get_text(label);
    sysState.wifi_ssid = String(selectedName);

    // Context-safely update the text using the button handle passed from user data
    lv_obj_t* dd_label = lv_obj_get_child(thiz->dd_wifi_ssids, 0);
    if (dd_label) lv_label_set_text(dd_label, selectedName);

    lv_obj_t* full_screen_modal = lv_obj_get_parent(lv_obj_get_parent(btn));
    if (full_screen_modal) lv_obj_delete(full_screen_modal);
}

void TabNetwork::wifi_dropdown_click_cb(lv_event_t* e) {
    TabNetwork* thiz = (TabNetwork*)lv_event_get_user_data(e);
    if (!thiz) return;

    Serial.println(F("[NET] Auditing airwaves for networks..."));
    int networksFound = WiFi.scanNetworks(false, false, false, 150); 

    lv_obj_t* modal = lv_obj_create(lv_screen_active());
    lv_obj_set_size(modal, 320, 240); lv_obj_set_pos(modal, 0, 0);
    lv_obj_set_style_bg_color(modal, lv_color_black(), 0); lv_obj_set_style_bg_opa(modal, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(modal, 10, 0); lv_obj_set_style_radius(modal, 0, 0); lv_obj_set_style_border_width(modal, 0, 0);

    lv_obj_t* header_title = lv_label_create(modal);
    lv_label_set_text(header_title, "SELECT WIRELESS NETWORK:");
    lv_obj_set_style_text_color(header_title, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_text_font(header_title, &lv_font_montserrat_14, 0);
    lv_obj_align(header_title, LV_ALIGN_TOP_LEFT, 5, 0);

    lv_obj_t* list = lv_list_create(modal);
    lv_obj_set_size(list, 300, 195); lv_obj_align(list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(list, lv_palette_darken(LV_PALETTE_GREY, 4), 0);
    lv_obj_set_style_border_width(list, 0, 0); lv_obj_set_style_pad_all(list, 2, 0);

    if (networksFound <= 0) {
        lv_obj_t* no_wifi_btn = lv_list_add_button(list, LV_SYMBOL_WARNING, "No Networks Found. Tap to close");
        lv_obj_add_event_cb(no_wifi_btn, [](lv_event_t* ev) {
            lv_obj_delete(lv_obj_get_parent(lv_obj_get_parent((lv_obj_t*)lv_event_get_target(ev))));
        }, LV_EVENT_CLICKED, NULL);
    } else {
        int displayCount = (networksFound > 8) ? 8 : networksFound;
        for (int i = 0; i < displayCount; i++) {
            String ssid_name = WiFi.SSID(i);
            lv_obj_t* list_btn = lv_list_add_button(list, LV_SYMBOL_WIFI, ssid_name.c_str());
            lv_obj_set_style_bg_color(list_btn, lv_palette_darken(LV_PALETTE_GREY, 3), 0);
            lv_obj_set_style_bg_opa(list_btn, LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(list_btn, lv_color_white(), 0);
            lv_obj_set_style_text_font(list_btn, &lv_font_montserrat_18, 0);
            lv_obj_set_style_pad_ver(list_btn, 14, 0); 
            lv_obj_add_event_cb(list_btn, network_option_select_cb, LV_EVENT_CLICKED, thiz);
        }
    }
    WiFi.scanDelete(); 
}

// =====================================================================
// MASTER LIFECYCLE CONTAINER PANEL SETUP
// =====================================================================
void TabNetwork::setup(lv_obj_t* tab_container) {
    if (!tab_container) return;
    lv_obj_set_layout(tab_container, 0); 
    lv_obj_set_style_pad_all(tab_container, 0, 0);

    // Row 1: Master Mode Cycle Button & Peer State Label
    l_espnow_text = createButtonWithTextHandle(tab_container, 125, 34, "MODE", espnow_cycle_cb, this, btn_espnow_mode);
    lv_obj_set_pos(btn_espnow_mode, 10, 5);
    lv_label_set_recolor(l_espnow_text, true); 

    l_peer_status = createString(tab_container, "PEER: NONE", 14, 150, 13);

    // Wi-Fi Configuration Option Sub-Container Box
    container_wifi_group = lv_obj_create(tab_container);
    lv_obj_set_size(container_wifi_group, 300, 115); lv_obj_set_pos(container_wifi_group, 10, 42);   
    lv_obj_set_style_pad_all(container_wifi_group, 2, 0); lv_obj_set_style_border_width(container_wifi_group, 0, 0); 
    lv_obj_set_style_bg_opa(container_wifi_group, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(container_wifi_group, LV_OBJ_FLAG_HIDDEN); 

    // Row 2: SSID Network Scanner Activation Selection Row
    createString(container_wifi_group, "SSID:", 14, 5, 10);
    const char* init_ssid = (sysState.wifi_ssid.length() > 0) ? sysState.wifi_ssid.c_str() : "Tap to Scan Networks";
    
    // FIXED: Formatted correctly down here inside the non-static member setup loop!
    lv_obj_t* dd_label = createButtonWithTextHandle(container_wifi_group, 190, 32, init_ssid, wifi_dropdown_click_cb, this, dd_wifi_ssids);
    lv_obj_set_pos(dd_wifi_ssids, 95, 2);

    // Row 3: Alphanumeric Password Text Area Selection Input Row
    createString(container_wifi_group, "Password:", 14, 5, 48);
    ta_wifi_pass = lv_textarea_create(container_wifi_group);
    lv_obj_set_size(ta_wifi_pass, 190, 32); lv_obj_set_pos(ta_wifi_pass, 95, 42); 
    lv_textarea_set_one_line(ta_wifi_pass, true);
    lv_textarea_set_password_mode(ta_wifi_pass, true);
    lv_textarea_set_placeholder_text(ta_wifi_pass, "Enter Key");
    lv_obj_add_event_cb(ta_wifi_pass, password_field_focus_cb, LV_EVENT_FOCUSED, this);
    
    if (sysState.wifi_pass.length() > 0) {
        lv_textarea_set_text(ta_wifi_pass, sysState.wifi_pass.c_str());
    }

    // Row 4: Security Masking Mask/Unmask Toggle Latch Checkbox
    cb_show_pass = lv_checkbox_create(container_wifi_group);
    lv_checkbox_set_text(cb_show_pass, "Show Password");
    lv_obj_set_pos(cb_show_pass, 95, 78); 
    lv_obj_set_style_text_font(cb_show_pass, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(cb_show_pass, lv_color_black(), 0); 
    lv_obj_add_event_cb(cb_show_pass, show_pass_toggle_cb, LV_EVENT_VALUE_CHANGED, this);

    // Base System Overlay Alphanumeric Keyboard Frame Layout
    kb_system_deck = lv_keyboard_create(lv_screen_active());
    lv_obj_add_flag(kb_system_deck, LV_OBJ_FLAG_HIDDEN); 
    lv_obj_set_size(kb_system_deck, 320, 110); 
    lv_obj_align(kb_system_deck, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(kb_system_deck, keyboard_action_done_cb, LV_EVENT_READY, this);
    lv_obj_add_event_cb(kb_system_deck, keyboard_action_done_cb, LV_EVENT_CANCEL, this);
}
