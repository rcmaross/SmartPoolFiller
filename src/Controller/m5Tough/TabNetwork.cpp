#include "TabNetwork.h"
#include "SystemState.h"
#include <WiFi.h>
#include <esp_now.h> 
#include <esp_wifi.h>

void TabNetwork::password_field_focus_cb(lv_event_t* e) {
    TabNetwork* thiz = (TabNetwork*)lv_event_get_user_data(e);
    if (!thiz || !thiz->kb_system_deck || !thiz->ta_wifi_pass) return;

    lv_keyboard_set_textarea(thiz->kb_system_deck, thiz->ta_wifi_pass);
    lv_obj_remove_flag(thiz->kb_system_deck, LV_OBJ_FLAG_HIDDEN);
}

void TabNetwork::keyboard_action_done_cb(lv_event_t* e) {
    TabNetwork* thiz = (TabNetwork*)lv_event_get_user_data(e);
    if (!thiz) return;
    
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) {
        const char* enteredPass = lv_textarea_get_text(thiz->ta_wifi_pass);
        sysState->wifi_pass = enteredPass ? String(enteredPass) : String("");
        sysState->saveToFlash();
        thiz->connectNetwork(); 
    }

    if (thiz->kb_system_deck) lv_obj_add_flag(thiz->kb_system_deck, LV_OBJ_FLAG_HIDDEN);
    if (thiz->ta_wifi_pass) lv_obj_clear_state(thiz->ta_wifi_pass, LV_STATE_FOCUSED);
}

void TabNetwork::update(bool force) {
    if (!btn_espnow_mode || !l_espnow_text || !l_peer_status || !container_wifi_group) return;

    char m_str[32] = {0};
    char status_str[64] = {0}; // Allocating 64 bytes to accommodate color tags

    // 1. Maintain clean, original text layouts for your Master Mode button
    if (sysState->connection_type == 0) {
        snprintf(m_str, sizeof(m_str), "Mode: OFF");
        lv_obj_add_flag(container_wifi_group, LV_OBJ_FLAG_HIDDEN);
        
        snprintf(status_str, sizeof(status_str), "PEER: NONE");
    } 
    else if (sysState->connection_type == 1) { 
        snprintf(m_str, sizeof(m_str), "Mode: WIFI");
        lv_obj_remove_flag(container_wifi_group, LV_OBJ_FLAG_HIDDEN);

        // 2. Shift the color-coded RSSI calculation to the peer status text field instead
        if (WiFi.status() == WL_CONNECTED) {
            int32_t rssi = WiFi.RSSI();
            const char* color_hex;

            if (rssi >= -60) {
                color_hex = "00FF00"; // Green
            } else if (rssi >= -75) {
                color_hex = "FFFF00"; // Yellow
            } else {
                color_hex = "FF0000"; // Red
            }

            // Formats status text to display: "RSSI: -54 dBm" with localized color tags
            lv_label_set_recolor(l_peer_status, true);
            snprintf(status_str, sizeof(status_str), "RSSI: #%s %d dBm#", color_hex, rssi);
        } else {
            lv_label_set_recolor(l_peer_status, true);
            snprintf(status_str, sizeof(status_str), "RSSI: #FF0000 SEARCHING#");
        }
    } 
    else if (sysState->connection_type == 2) {
        snprintf(m_str, sizeof(m_str), "Mode: AUTO");
        lv_obj_add_flag(container_wifi_group, LV_OBJ_FLAG_HIDDEN);
        
        snprintf(status_str, sizeof(status_str), "PEER: AUTO");
    } 
    else {
        snprintf(m_str, sizeof(m_str), "Mode: CH %d", sysState->espnow_channel);
        lv_obj_add_flag(container_wifi_group, LV_OBJ_FLAG_HIDDEN);

        // For ESP-NOW channels, revert back to using your helper icon logic
        lv_label_set_recolor(l_peer_status, false);
        lv_color_t active_status_color;
        const char* active_icon_symbol = getWifiStatusIcon(active_status_color);
        lv_label_set_text(l_peer_status, active_icon_symbol);
        lv_obj_set_style_text_color(l_peer_status, active_status_color, 0);
    }

    // Write your clean "Mode: WIFI" string to the main button
    lv_label_set_text(l_espnow_text, m_str); 

    // Write the color-coded RSSI to the secondary status label (only applied in WIFI mode)
    if (sysState->connection_type <= 2) {
        lv_label_set_text(l_peer_status, status_str);
        // Reset default text color to black/white so it doesn't leak out of the color tags
        lv_obj_set_style_text_color(l_peer_status, lv_color_black(), 0); 
    }
}

void TabNetwork::show_pass_toggle_cb(lv_event_t* e) {
    lv_obj_t* cb = (lv_obj_t*)lv_event_get_target(e);
    TabNetwork* thiz = (TabNetwork*)lv_event_get_user_data(e);
    if (!cb || !thiz || !thiz->ta_wifi_pass) return;

    bool is_checked = lv_obj_has_state(cb, LV_STATE_CHECKED);
    if (is_checked) {
        lv_textarea_set_password_mode(thiz->ta_wifi_pass, false);
    } else {
        lv_textarea_set_password_mode(thiz->ta_wifi_pass, true);
    }
}

const char* TabNetwork::getWifiStatusIcon(lv_color_t& out_color) {
    out_color = lv_palette_main(LV_PALETTE_GREY);
    if (sysState->connection_type == 0) return "OFF";
    if (sysState->connection_type == 2) { out_color = lv_palette_main(LV_PALETTE_BLUE); return "AUTO"; }
    if (sysState->connection_type >= 3) { out_color = lv_palette_main(LV_PALETTE_BLUE); return "MESH"; }

    wl_status_t wifi_status = WiFi.status();
    if (wifi_status == WL_CONNECTED) {
        int32_t rssi = WiFi.RSSI();
        if (rssi >= -60)       out_color = lv_palette_main(LV_PALETTE_GREEN);
        else if (rssi >= -75)  out_color = lv_palette_main(LV_PALETTE_YELLOW);
        else                   out_color = lv_palette_main(LV_PALETTE_RED);
        return LV_SYMBOL_WIFI;
    } 
    else if (wifi_status == WL_CONNECT_FAILED || wifi_status == WL_NO_SSID_AVAIL || wifi_status == WL_CONNECTION_LOST) {
        out_color = lv_palette_main(LV_PALETTE_RED);
        return "X ERR";
    } 
    out_color = lv_palette_main(LV_PALETTE_ORANGE);
    return "... LINK";
}
