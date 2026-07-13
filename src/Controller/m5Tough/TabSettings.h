#pragma once
#include "BaseTab.h"
#include "SystemState.h"

class TabSettings : public BaseTab {
private:
    // Master Scroll Panel Viewport Canvas
    lv_obj_t* scroll_panel = nullptr;

    // Interactive Widget Content Handles
    lv_obj_t* btn_unit_toggle = nullptr;
    lv_obj_t* l_unit_btn_text = nullptr;
    
    lv_obj_t* btn_time_format_toggle = nullptr; 
    lv_obj_t* l_time_format_text = nullptr;     
    
    lv_obj_t* l_fill_window_text = nullptr;

    lv_obj_t* btn_well_toggle = nullptr;
    lv_obj_t* l_well_btn_text = nullptr;

    lv_obj_t* btn_ntp_toggle = nullptr;
    lv_obj_t* l_ntp_btn_text = nullptr;
    lv_obj_t* container_timezone = nullptr;
    lv_obj_t* l_tz_text = nullptr;

    lv_obj_t* container_manual_clock = nullptr;
    lv_obj_t* l_manual_clock_text = nullptr;

    // Static Event Callbacks
    static void unit_toggle_cb(lv_event_t* e);
    static void time_format_toggle_cb(lv_event_t* e);
    static void fill_window_adjust_cb(lv_event_t* e);
    static void well_toggle_cb(lv_event_t* e);
    static void ntp_toggle_cb(lv_event_t* e);
    static void tz_adjust_cb(lv_event_t* e);
    static void manual_clock_adjust_cb(lv_event_t* e);

    // Modular OOP Row Builders
    void build_units_row();
    void build_time_mode_row();
    void build_fill_window_row();
    void build_fill_rest_row();
    void build_clock_sync_row();

public:
    void setup(lv_obj_t* tab) override;
    void update() override;
};
