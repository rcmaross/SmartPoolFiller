#include "TabSettings.h"
#include <WiFi.h>
#include <M5Unified.h> 

// --- Static Callback Logical Routines ---
void TabSettings::unit_toggle_cb(lv_event_t* e) {
    sysState.use_metric = !sysState.use_metric;
    sysState.saveToFlash();
}

void TabSettings::fill_window_adjust_cb(lv_event_t* e) {
    intptr_t mod = (intptr_t)lv_event_get_user_data(e);
    if (mod == 1)       sysState.allow_fill_start_hour = (sysState.allow_fill_start_hour + 1) % 24;
    else if (mod == -1) sysState.allow_fill_start_hour = (sysState.allow_fill_start_hour == 0) ? 23 : sysState.allow_fill_start_hour - 1;
    else if (mod == 2)  sysState.allow_fill_end_hour = (sysState.allow_fill_end_hour + 1) % 24;
    else if (mod == -2) sysState.allow_fill_end_hour = (sysState.allow_fill_end_hour == 0) ? 23 : sysState.allow_fill_end_hour - 1;
    sysState.saveToFlash();
}

void TabSettings::well_toggle_cb(lv_event_t* e) {
    sysState.well_rest_selection = (sysState.well_rest_selection + 1) % 4;
    sysState.well_mode_active = (sysState.well_rest_selection > 0);
    sysState.saveToFlash();
}

void TabSettings::ntp_toggle_cb(lv_event_t* e) {
    sysState.ntp_sync_active = !sysState.ntp_sync_active;
    sysState.saveToFlash();
}

void TabSettings::tz_adjust_cb(lv_event_t* e) {
    intptr_t mod = (intptr_t)lv_event_get_user_data(e);
    sysState.timezone_offset_hours += (int)mod;
    if (sysState.timezone_offset_hours > 14)  sysState.timezone_offset_hours = 14;
    if (sysState.timezone_offset_hours < -12) sysState.timezone_offset_hours = -12;
    sysState.saveToFlash();
}

void TabSettings::manual_clock_adjust_cb(lv_event_t* e) {
    intptr_t mod = (intptr_t)lv_event_get_user_data(e);
    auto tm = M5.Rtc.getDateTime(); 
    
    if (mod == 1)       tm.date.year++;
    else if (mod == -1) tm.date.year--;
    else if (mod == 2)  { tm.date.month++; if(tm.date.month > 12) tm.date.month = 1; }
    else if (mod == -2) { tm.date.month--; if(tm.date.month < 1)  tm.date.month = 12; }
    else if (mod == 3)  { tm.date.date++;  if(tm.date.date > 31)  tm.date.date = 1; }
    else if (mod == -3) { tm.date.date--;  if(tm.date.date < 1)   tm.date.date = 31; }
    else if (mod == 4)  { tm.time.hours++; if(tm.time.hours > 23) tm.time.hours = 0; }
    else if (mod == -4) { tm.time.hours--; if(tm.time.hours < 0)  tm.time.hours = 23; }
    else if (mod == 5)  { tm.time.minutes++; if(tm.time.minutes > 59) tm.time.minutes = 0; }
    else if (mod == -5) { tm.time.minutes--; if(tm.time.minutes < 0)  tm.time.minutes = 59; }

    M5.Rtc.setDateTime(tm); 
}

void TabSettings::time_format_toggle_cb(lv_event_t* e) {
    sysState.use_24hr_format = !sysState.use_24hr_format;
    sysState.saveToFlash();
} 

// --- Real-Time Interface Loop updates ---
void TabSettings::update() {
    if (!btn_unit_toggle || !l_unit_btn_text || !l_fill_window_text || !btn_well_toggle) return;

    if (!sysState.use_metric) {
        updateString(l_unit_btn_text, "UNITS: INCHES");
        lv_obj_set_style_bg_color(btn_unit_toggle, lv_palette_main(LV_PALETTE_GREY), 0);
    } else {
        updateString(l_unit_btn_text, "UNITS: METRIC (cm)");
        lv_obj_set_style_bg_color(btn_unit_toggle, lv_palette_main(LV_PALETTE_TEAL), 0);
    }
    
    if (btn_time_format_toggle && l_time_format_text) {
        if (!sysState.use_24hr_format) {
            updateString(l_time_format_text, "FORMAT: 12 HR");
            lv_obj_set_style_bg_color(btn_time_format_toggle, lv_palette_main(LV_PALETTE_GREY), 0);
        } else {
            updateString(l_time_format_text, "FORMAT: 24 HR");
            lv_obj_set_style_bg_color(btn_time_format_toggle, lv_palette_main(LV_PALETTE_TEAL), 0);
        }
    }

    char f_buf[32] = {0}; 
    snprintf(f_buf, sizeof(f_buf), "%02d:00 to %02d:00", sysState.allow_fill_start_hour, sysState.allow_fill_end_hour);
    updateString(l_fill_window_text, f_buf);

    if (sysState.well_rest_selection == 0) {
        updateString(l_well_btn_text, "REST: NONE (CITY)");
        lv_obj_set_style_bg_color(btn_well_toggle, lv_palette_main(LV_PALETTE_GREY), 0);
    } else {
        char w_buf[32] = {0};
        snprintf(w_buf, sizeof(w_buf), "REST: %d MINS", sysState.well_rest_selection * 5);
        updateString(l_well_btn_text, w_buf);
        lv_obj_set_style_bg_color(btn_well_toggle, lv_palette_main(LV_PALETTE_BROWN), 0);
    }

    if (!sysState.ntp_sync_active) {
        updateString(l_ntp_btn_text, "LOCAL RTC");
        lv_obj_set_style_bg_color(btn_ntp_toggle, lv_palette_main(LV_PALETTE_GREY), 0);
        lv_obj_add_flag(container_timezone, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(container_manual_clock, LV_OBJ_FLAG_HIDDEN);

        auto dt = M5.Rtc.getDateTime();
        char c_buf[32] = {0};
        snprintf(c_buf, sizeof(c_buf), "%04d-%02d-%02d  %02d:%02d", dt.date.year, dt.date.month, dt.date.date, dt.time.hours, dt.time.minutes);
        updateString(l_manual_clock_text, c_buf);
    } else {
        updateString(l_ntp_btn_text, "NTP SERVER");
        lv_obj_set_style_bg_color(btn_ntp_toggle, lv_palette_main(LV_PALETTE_GREEN), 0);
        lv_obj_remove_flag(container_timezone, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(container_manual_clock, LV_OBJ_FLAG_HIDDEN);

        char t_buf[32] = {0}; 
        snprintf(t_buf, sizeof(t_buf), "Zone: UTC %+d hr", sysState.timezone_offset_hours);
        updateString(l_tz_text, t_buf);
    }
}
