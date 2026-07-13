#include "TabSettings.h"

struct AdjusterBtnConfig {
    int x_pos;
    const char* label;
    intptr_t callback_data;
};

void TabSettings::setup(lv_obj_t* tab) {
    if (!tab) return;
    lv_obj_set_layout(tab, 0);
    lv_obj_set_style_pad_all(tab, 0, 0);

    // Master scrolling viewport container canvas setup
    scroll_panel = lv_obj_create(tab);
    lv_obj_set_size(scroll_panel, 310, 155); 
    lv_obj_align(scroll_panel, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_pad_all(scroll_panel, 5, 0);
    lv_obj_set_style_border_width(scroll_panel, 0, 0);
    lv_obj_set_style_bg_opa(scroll_panel, LV_OPA_TRANSP, 0);
    lv_obj_set_scrollbar_mode(scroll_panel, LV_SCROLLBAR_MODE_AUTO);

    build_units_row();
    build_time_mode_row();
    build_fill_window_row();
    build_fill_rest_row();
    build_clock_sync_row();
}

void TabSettings::build_units_row() {
    createString(scroll_panel, "Units:", 14, 10, 15);
    l_unit_btn_text = createButtonWithTextHandle(scroll_panel, 130, 32, "UNITS", unit_toggle_cb, this, btn_unit_toggle);
    lv_obj_set_pos(btn_unit_toggle, 150, 8);
}

void TabSettings::build_time_mode_row() {
    createString(scroll_panel, "Time Mode:", 14, 10, 55);
    l_time_format_text = createButtonWithTextHandle(scroll_panel, 130, 32, "FORMAT", time_format_toggle_cb, this, btn_time_format_toggle);
    lv_obj_set_pos(btn_time_format_toggle, 150, 48);
}

void TabSettings::build_fill_window_row() {
    createString(scroll_panel, "Allow Fill Hour Window:", 14, 10, 95);

    lv_obj_t* b_s_min = nullptr; createButtonWithTextHandle(scroll_panel, 30, 30, "-", fill_window_adjust_cb, (void*)-1, b_s_min); lv_obj_set_pos(b_s_min, 10, 120);
    lv_obj_t* b_s_pls = nullptr; createButtonWithTextHandle(scroll_panel, 30, 30, "+", fill_window_adjust_cb, (void*)+1, b_s_pls); lv_obj_set_pos(b_s_pls, 45, 120);

    l_fill_window_text = createString(scroll_panel, "00:00 to 00:00", 14, 85, 127);

    lv_obj_t* b_e_min = nullptr; createButtonWithTextHandle(scroll_panel, 30, 30, "-", fill_window_adjust_cb, (void*)-2, b_e_min); lv_obj_set_pos(b_e_min, 215, 120);
    lv_obj_t* b_e_pls = nullptr; createButtonWithTextHandle(scroll_panel, 30, 30, "+", fill_window_adjust_cb, (void*)+2, b_e_pls); lv_obj_set_pos(b_e_pls, 250, 120);
}

void TabSettings::build_fill_rest_row() {
    createString(scroll_panel, "Fill Rest (Every 30min):", 14, 10, 165);
    l_well_btn_text = createButtonWithTextHandle(scroll_panel, 250, 32, "REST", well_toggle_cb, this, btn_well_toggle);
    lv_obj_set_pos(btn_well_toggle, 10, 188);
}

void TabSettings::build_clock_sync_row() {
    createString(scroll_panel, "Clock Sync Setup:", 14, 10, 235);
    l_ntp_btn_text = createButtonWithTextHandle(scroll_panel, 130, 32, "SYNC", ntp_toggle_cb, this, btn_ntp_toggle);
    lv_obj_set_pos(btn_ntp_toggle, 150, 227);

    container_timezone = lv_obj_create(scroll_panel);
    lv_obj_set_size(container_timezone, 280, 45); lv_obj_set_pos(container_timezone, 10, 265);
    lv_obj_set_style_pad_all(container_timezone, 2, 0); lv_obj_set_style_border_width(container_timezone, 0, 0);
    lv_obj_set_style_bg_opa(container_timezone, LV_OPA_TRANSP, 0);

    lv_obj_t* b_tz_min = nullptr; createButtonWithTextHandle(container_timezone, 30, 30, "-", tz_adjust_cb, (void*)-1, b_tz_min); lv_obj_set_pos(b_tz_min, 0, 0);
    lv_obj_t* b_tz_pls = nullptr; createButtonWithTextHandle(container_timezone, 30, 30, "+", tz_adjust_cb, (void*)+1, b_tz_pls); lv_obj_set_pos(b_tz_pls, 240, 0);
    l_tz_text = createString(container_timezone, "Zone: UTC", 14, 45, 6);

    container_manual_clock = lv_obj_create(scroll_panel);
    lv_obj_set_size(container_manual_clock, 280, 75); lv_obj_set_pos(container_manual_clock, 10, 265);
    lv_obj_set_style_pad_all(container_manual_clock, 2, 0); lv_obj_set_style_border_width(container_manual_clock, 0, 0);
    lv_obj_set_style_bg_opa(container_manual_clock, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(container_manual_clock, LV_OBJ_FLAG_HIDDEN);

    AdjusterBtnConfig manual_grid[] = {
        {0,   "Y-", -1}, {24,  "Y+", 1},  
        {55,  "M-", -2}, {79,  "M+", 2},  
        {110, "D-", -3}, {134, "D+", 3},  
        {175, "H-", -4}, {199, "H+", 4},  
        {230, "m-", -5}, {254, "m+", 5}   
    };

    for (int i = 0; i < 10; i++) {
        lv_obj_t* b_handle = nullptr;
        createButtonWithTextHandle(container_manual_clock, 22, 24, manual_grid[i].label, manual_clock_adjust_cb, (void*)manual_grid[i].callback_data, b_handle);
        lv_obj_set_pos(b_handle, manual_grid[i].x_pos, 0);
    }

    l_manual_clock_text = createString(container_manual_clock, "2026-01-01 00:00", 14, 10, 38);
}
