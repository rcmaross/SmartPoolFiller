#pragma once
#include "BaseTab.h"
#include "SystemState.h"
#include <stdio.h>

class TabCalibration : public BaseTab {
private:
    lv_obj_t* l_empty = nullptr;
    lv_obj_t* l_full = nullptr;
    lv_obj_t* l_offset = nullptr;

    // Interactive element container mapping handles
    lv_obj_t* btn_coarse_minus = nullptr;
    lv_obj_t* btn_fine_minus = nullptr;
    lv_obj_t* btn_fine_plus = nullptr;
    lv_obj_t* btn_coarse_plus = nullptr;

    // Text label layer mapping handles
    lv_obj_t* l_btn_coarse_minus = nullptr;
    lv_obj_t* l_btn_fine_minus = nullptr;
    lv_obj_t* l_btn_fine_plus = nullptr;
    lv_obj_t* l_btn_coarse_plus = nullptr;

    bool last_known_unit_mode = false;

    static void empty_cb(lv_event_t* e) {
        sysState.empty_volts = sysState.sim_voltage;
        sysState.saveToFlash(); 
    }
    
    static void full_cb(lv_event_t* e) {
        sysState.full_volts = sysState.sim_voltage;
        sysState.saveToFlash(); 
    }
    
    static void offset_cb(lv_event_t* e) {
        float increment = 0.0f;
        union { void* ptr; float val; } castLink;
        castLink.ptr = lv_event_get_user_data(e);
        float inputAmt = castLink.val;

        if (!sysState.use_metric) {
            increment = inputAmt;
        } else {
            increment = inputAmt / 2.54f;
        }

        sysState.offset_in += increment;
        if (sysState.offset_in < 1.0f) sysState.offset_in = 1.0f; 
        sysState.saveToFlash(); 
    }

    void bindButtonPayload(lv_obj_t* btn, float amount) {
        if (!btn) return;
        lv_obj_remove_event_cb(btn, offset_cb);
        union { float val; void* ptr; } dsc;
        dsc.val = amount;
        lv_obj_add_event_cb(btn, offset_cb, LV_EVENT_CLICKED, dsc.ptr);
    }

public:
    void setup(lv_obj_t* tab) override {
        lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(tab, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(tab, 4, 0);
        lv_obj_set_style_pad_row(tab, 4, 0); 

        // -------------------------------------------------------------
        // ROW 1: SET EMPTY CALIBRATION (Restored to exactly 3 items)
        // -------------------------------------------------------------
        lv_obj_t* r1 = lv_obj_create(tab); lv_obj_set_size(r1, LV_PCT(100), 42);
        lv_obj_set_flex_flow(r1, LV_FLEX_FLOW_ROW); lv_obj_set_flex_align(r1, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_opa(r1, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(r1, 0, 0); lv_obj_set_style_pad_all(r1, 0, 0);

        createString(r1, "         ", 14); // Item 1: Spacer
        
        // FIXED: Drop dummy handles. Create the button, let its inner text center automatically,
        // and assign the master button container pointer straight to your class handle!
        lv_obj_t* b1 = lv_button_create(r1); lv_obj_set_size(b1, 100, 32); // Item 2: Button
        lv_obj_add_event_cb(b1, empty_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t* b1_l = lv_label_create(b1); lv_label_set_text(b1_l, "Set Empty"); lv_obj_center(b1_l);
        
        l_empty = createString(r1, "0.000V", 14); // Item 3: Reading Label

        // -------------------------------------------------------------
        // ROW 2: SET FULL CALIBRATION (Restored to exactly 3 items)
        // -------------------------------------------------------------
        lv_obj_t* r2 = lv_obj_create(tab); lv_obj_set_size(r2, LV_PCT(100), 42);
        lv_obj_set_flex_flow(r2, LV_FLEX_FLOW_ROW); lv_obj_set_flex_align(r2, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_opa(r2, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(r2, 0, 0); lv_obj_set_style_pad_all(r2, 0, 0);

        createString(r2, "         ", 14); // Item 1: Spacer
        
        lv_obj_t* b2 = lv_button_create(r2); lv_obj_set_size(b2, 100, 32); // Item 2: Button
        lv_obj_add_event_cb(b2, full_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t* b2_l = lv_label_create(b2); lv_label_set_text(b2_l, "Set Full"); lv_obj_center(b2_l);
        
        l_full = createString(r2, "0.000V", 14); // Item 3: Reading Label

        // -------------------------------------------------------------
        // ROW 3: PRECISION TUNING DECK CONFIGURATION (Kept as 5 items)
        // -------------------------------------------------------------
        lv_obj_t* r3 = lv_obj_create(tab); lv_obj_set_size(r3, LV_PCT(100), 42);
        lv_obj_set_flex_flow(r3, LV_FLEX_FLOW_ROW); lv_obj_set_flex_align(r3, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_bg_opa(r3, LV_OPA_TRANSP, 0); lv_obj_set_style_border_width(r3, 0, 0); lv_obj_set_style_pad_all(r3, 0, 0);
        
        // This row continues to map beautifully because your layout array matches 5 items exactly
        l_btn_coarse_minus  = createButtonWithTextHandle(r3, 50, 32, "-", NULL, NULL, btn_coarse_minus);
        l_btn_fine_minus    = createButtonWithTextHandle(r3, 45, 32, "-", NULL, NULL, btn_fine_minus);
        
        l_offset            = createString(r3, "0.0 in", 14);

        l_btn_fine_plus     = createButtonWithTextHandle(r3, 45, 32, "+", NULL, NULL, btn_fine_plus);
        l_btn_coarse_plus   = createButtonWithTextHandle(r3, 50, 32, "+", NULL, NULL, btn_coarse_plus);

        last_known_unit_mode = !sysState.use_metric; 
    }

    void update(bool force) override {
        if (!l_empty || !l_full || !l_offset ||
            !btn_coarse_minus || !btn_fine_minus || !btn_fine_plus || !btn_coarse_plus ||
            !l_btn_coarse_minus || !l_btn_fine_minus || !l_btn_fine_plus || !l_btn_coarse_plus) return;

        char b_emp[32], b_ful[32], b_off[32];
        snprintf(b_emp, sizeof(b_emp), "E: %.3fV", sysState.empty_volts);
        snprintf(b_ful, sizeof(b_ful), "F: %.3fV", sysState.full_volts);

        if (sysState.use_metric != last_known_unit_mode) {
            last_known_unit_mode = sysState.use_metric;

            if (!sysState.use_metric) {
                updateString(l_btn_coarse_minus, "-1'");
                updateString(l_btn_fine_minus, "-1\"");
                updateString(l_btn_fine_plus, "+1\"");
                updateString(l_btn_coarse_plus, "+1'");

                bindButtonPayload(btn_coarse_minus, -12.0f); 
                bindButtonPayload(btn_fine_minus, -1.0f);    
                bindButtonPayload(btn_fine_plus, 1.0f);      
                bindButtonPayload(btn_coarse_plus, 12.0f);   
            } else {
                updateString(l_btn_coarse_minus, "-10c");
                updateString(l_btn_fine_minus, "-1c");
                updateString(l_btn_fine_plus, "+1c");
                updateString(l_btn_coarse_plus, "+10c");

                bindButtonPayload(btn_coarse_minus, -10.0f); 
                bindButtonPayload(btn_fine_minus, -1.0f);    
                bindButtonPayload(btn_fine_plus, 1.0f);      
                bindButtonPayload(btn_coarse_plus, 10.0f);   
            }
        }

        if (!sysState.use_metric) {
            snprintf(b_off, sizeof(b_off), "%.1f in", sysState.offset_in);
        } else {
            snprintf(b_off, sizeof(b_off), "%.1f cm", sysState.offset_in * 2.54f);
        }

        updateString(l_empty, b_emp);
        updateString(l_full, b_ful);
        updateString(l_offset, b_off);
    }
};
