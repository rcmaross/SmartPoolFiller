#pragma once
#include "BaseTab.h"
#include "SystemState.h"
#include "StorageDisk.h"
#include <stdio.h>

class TabCalibration : public BaseTab {
private:
    StorageDisk *storageDisk;
    lv_obj_t* l_empty = nullptr;
    lv_obj_t* l_full = nullptr;
    lv_obj_t* l_offset = nullptr;
    lv_obj_t* l_pressure_comp = nullptr;

    lv_obj_t* btn_coarse_minus = nullptr;
    lv_obj_t* btn_fine_minus = nullptr;
    lv_obj_t* btn_fine_plus = nullptr;
    lv_obj_t* btn_coarse_plus = nullptr;

    lv_obj_t* l_btn_coarse_minus = nullptr;
    lv_obj_t* l_btn_fine_minus = nullptr;
    lv_obj_t* l_btn_fine_plus = nullptr;
    lv_obj_t* l_btn_coarse_plus = nullptr;

    lv_obj_t* l_btn_pressure_coarse_minus = nullptr;
    lv_obj_t* l_btn_pressure_fine_minus   = nullptr;
    lv_obj_t* l_btn_pressure_fine_plus    = nullptr;
    lv_obj_t* l_btn_pressure_coarse_plus  = nullptr;

    lv_obj_t* btn_pressure_coarse_minus = nullptr;
    lv_obj_t* btn_pressure_fine_minus   = nullptr;
    lv_obj_t* btn_pressure_fine_plus    = nullptr;
    lv_obj_t* btn_pressure_coarse_plus  = nullptr;

    lv_obj_t* btn_erase_sd = nullptr;
    lv_obj_t* l_sd_info = nullptr;

    bool last_known_unit_mode = false;

    static void empty_cb(lv_event_t* e) {
        sysState->empty_volts = sysState->sim_voltage;
        sysState->saveToFlash();
    }

    static void full_cb(lv_event_t* e) {
        sysState->full_volts = sysState->sim_voltage;
        sysState->full_pressure_inHg = sysState->pressure_inHg;
        sysState->saveToFlash();
    }

    static void pressure_cb(lv_event_t* e) {
        union {
            void* ptr;
            float val;
        } castLink;
        castLink.ptr = lv_event_get_user_data(e);
        float inputAmt = castLink.val;

        sysState->pressure_compensation += inputAmt;
        if (sysState->pressure_compensation > 1.0f) sysState->pressure_compensation = 1.0f;
        if (sysState->pressure_compensation < 0.0f) sysState->pressure_compensation = 0.0f;
        sysState->saveToFlash();
    }

    static void offset_cb(lv_event_t* e) {
        float increment = 0.0f;
        union {
            void* ptr;
            float val;
        } castLink;
        castLink.ptr = lv_event_get_user_data(e);
        float inputAmt = castLink.val;

        if (!sysState->use_metric) {
            increment = inputAmt;
        } else {
            increment = inputAmt / 2.54f;
        }
        sysState->offset_in += increment;
        if (sysState->offset_in < 1.0f) sysState->offset_in = 1.0f;
        sysState->saveToFlash();
    }

    static void erase_sd_cb(lv_event_t* e) {
        TabCalibration* instance = (TabCalibration*)lv_event_get_user_data(e);
        if (instance != nullptr && instance->storageDisk != nullptr) {
            if (instance->storageDisk->eraseAppDirectory()) {
                Serial.println("[CALIBRATION] Storage folder successfully wiped via screen button.");
            }
        }
    }
    void bindPressureButtonPayload(lv_obj_t* btn, float amount) {
        if (!btn) return;
        lv_obj_remove_event_cb(btn, pressure_cb);
        union {
            float val;
            void* ptr;
        } dsc;
        dsc.val = amount;
        lv_obj_add_event_cb(btn, pressure_cb, LV_EVENT_CLICKED, dsc.ptr);
    }

    void bindOffsetButtonPaylod(lv_obj_t* btn, float amount) {
        if (!btn) return;
        lv_obj_remove_event_cb(btn, offset_cb);
        union {
            float val;
            void* ptr;
        } dsc;
        dsc.val = amount;
        lv_obj_add_event_cb(btn, offset_cb, LV_EVENT_CLICKED, dsc.ptr);
    }

    void updatePressureCompButtons() {
        float v = sysState->pressure_compensation;

        lv_obj_set_state(l_btn_pressure_coarse_minus,
                        LV_STATE_DISABLED, v < 0.10f);
        bindPressureButtonPayload(btn_pressure_coarse_minus, -0.10f)
        ;lv_obj_set_state(l_btn_pressure_fine_minus,
                        LV_STATE_DISABLED, v < 0.01f);
        bindPressureButtonPayload(btn_pressure_fine_minus, -0.01f);
        lv_obj_set_state(l_btn_pressure_fine_plus,
                        LV_STATE_DISABLED, v > 0.99f);
        bindPressureButtonPayload(btn_pressure_fine_plus, +0.01f);
        lv_obj_set_state(l_btn_pressure_coarse_plus,
                        LV_STATE_DISABLED, v > 0.90f);
        bindPressureButtonPayload(btn_pressure_coarse_plus, +0.10f);
    }
public:
    TabCalibration(StorageDisk *storageDisk) {
        this->storageDisk = storageDisk;
    }

    void setup(lv_obj_t* tab) override {

        // Overall tab layout
        lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(tab,
                            LV_FLEX_ALIGN_START,
                            LV_FLEX_ALIGN_CENTER,
                            LV_FLEX_ALIGN_CENTER);

        lv_obj_set_style_pad_all(tab, 4, 0);
        lv_obj_set_style_pad_row(tab, 4, 0);


        // ------------------------------------------------------------
        // EMPTY CALIBRATION
        // ------------------------------------------------------------

        lv_obj_t* r1 = lv_obj_create(tab);
        lv_obj_set_size(r1, LV_PCT(100), 42);

        lv_obj_set_flex_flow(r1, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(r1,
                            LV_FLEX_ALIGN_START,
                            LV_FLEX_ALIGN_CENTER,
                            LV_FLEX_ALIGN_CENTER);

        lv_obj_set_style_bg_opa(r1, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(r1, 0, 0);

        // Fixed left margin and fixed gap between button/value.
        lv_obj_set_style_pad_left(r1, 20, 0);
        lv_obj_set_style_pad_right(r1, 0, 0);
        lv_obj_set_style_pad_column(r1, 20, 0);
        lv_obj_set_style_pad_top(r1, 0, 0);
        lv_obj_set_style_pad_bottom(r1, 0, 0);

        lv_obj_t* b1 = lv_button_create(r1);
        lv_obj_set_size(b1, 100, 32);
        lv_obj_add_event_cb(b1, empty_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t* b1_l = lv_label_create(b1);
        lv_label_set_text(b1_l, "Set Empty");
        lv_obj_center(b1_l);

        l_empty = createString(r1, "0.000V", 14);


        // ------------------------------------------------------------
        // FULL CALIBRATION
        // ------------------------------------------------------------

        lv_obj_t* r2 = lv_obj_create(tab);
        lv_obj_set_size(r2, LV_PCT(100), 42);

        lv_obj_set_flex_flow(r2, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(r2,
                            LV_FLEX_ALIGN_START,
                            LV_FLEX_ALIGN_CENTER,
                            LV_FLEX_ALIGN_CENTER);

        lv_obj_set_style_bg_opa(r2, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(r2, 0, 0);

        // Same geometry as r1 so button/value columns line up.
        lv_obj_set_style_pad_left(r2, 20, 0);
        lv_obj_set_style_pad_right(r2, 0, 0);
        lv_obj_set_style_pad_column(r2, 20, 0);
        lv_obj_set_style_pad_top(r2, 0, 0);
        lv_obj_set_style_pad_bottom(r2, 0, 0);

        lv_obj_t* b2 = lv_button_create(r2);
        lv_obj_set_size(b2, 100, 32);
        lv_obj_add_event_cb(b2, full_cb, LV_EVENT_CLICKED, NULL);

        lv_obj_t* b2_l = lv_label_create(b2);
        lv_label_set_text(b2_l, "Set Full");
        lv_obj_center(b2_l);

        l_full = createString(r2, "0.000V  29.92inHg", 14);


        // ------------------------------------------------------------
        // OFFSET ADJUSTMENT
        // ------------------------------------------------------------

        lv_obj_t* r3 = lv_obj_create(tab);
        lv_obj_set_size(r3, LV_PCT(100), 42);

        lv_obj_set_flex_flow(r3, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(r3,
                            LV_FLEX_ALIGN_SPACE_BETWEEN,
                            LV_FLEX_ALIGN_CENTER,
                            LV_FLEX_ALIGN_CENTER);

        lv_obj_set_style_bg_opa(r3, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(r3, 0, 0);
        lv_obj_set_style_pad_all(r3, 0, 0);

        l_btn_coarse_minus =
            createButtonWithTextHandle(r3, 50, 32, "-", NULL, NULL, btn_coarse_minus);

        l_btn_fine_minus =
            createButtonWithTextHandle(r3, 45, 32, "-", NULL, NULL, btn_fine_minus);

        l_offset =
            createString(r3, "0.0 in", 14);

        l_btn_fine_plus =
            createButtonWithTextHandle(r3, 45, 32, "+", NULL, NULL, btn_fine_plus);

        l_btn_coarse_plus =
            createButtonWithTextHandle(r3, 50, 32, "+", NULL, NULL, btn_coarse_plus);

        // ------------------------------------------------------------
        // PRESSURE COMPENSATION
        // ------------------------------------------------------------

        lv_obj_t* r4 = lv_obj_create(tab);
        lv_obj_set_size(r4, LV_PCT(100), 42);

        lv_obj_set_flex_flow(r4, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(r4,
                            LV_FLEX_ALIGN_SPACE_BETWEEN,
                            LV_FLEX_ALIGN_CENTER,
                            LV_FLEX_ALIGN_CENTER);

        lv_obj_set_style_bg_opa(r4, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(r4, 0, 0);
        lv_obj_set_style_pad_all(r4, 0, 0);

        l_btn_pressure_coarse_minus =
            createButtonWithTextHandle(
                r4, 50, 32, "-10", NULL, NULL, btn_pressure_coarse_minus);

        l_btn_pressure_fine_minus =
            createButtonWithTextHandle(
                r4, 45, 32, "-1", NULL, NULL, btn_pressure_fine_minus);

        l_pressure_comp =
            createString(r4, "100%", 14);

        l_btn_pressure_fine_plus =
            createButtonWithTextHandle(
                r4, 45, 32, "+1", NULL, NULL, btn_pressure_fine_plus);

        l_btn_pressure_coarse_plus =
            createButtonWithTextHandle(
                r4, 50, 32, "+10", NULL, NULL, btn_pressure_coarse_plus);

        // ------------------------------------------------------------
        // SD / ERASE HISTORY
        // ------------------------------------------------------------

        lv_obj_t* r5 = lv_obj_create(tab);
        lv_obj_set_size(r5, LV_PCT(100), 42);

        lv_obj_set_flex_flow(r5, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(r5,
                            LV_FLEX_ALIGN_CENTER,
                            LV_FLEX_ALIGN_CENTER,
                            LV_FLEX_ALIGN_CENTER);

        lv_obj_set_style_bg_opa(r5, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(r5, 0, 0);
        lv_obj_set_style_pad_all(r5, 0, 0);

        btn_erase_sd = lv_button_create(r5);
        lv_obj_set_size(btn_erase_sd, 140, 32);
        lv_obj_set_style_bg_color(
            btn_erase_sd,
            lv_palette_main(LV_PALETTE_RED),
            0);

        lv_obj_add_event_cb(
            btn_erase_sd,
            erase_sd_cb,
            LV_EVENT_CLICKED,
            this);

        lv_obj_t* erase_lbl = lv_label_create(btn_erase_sd);
        lv_label_set_text(erase_lbl, "Erase History");
        lv_obj_center(erase_lbl);

        l_sd_info = createString(r5, "Checking SD...", 13);

        last_known_unit_mode = !sysState->use_metric;
    }

    void update(bool force) override {
        if (!l_empty || !l_full || !l_offset || !btn_erase_sd || !l_sd_info ||
            !btn_coarse_minus || !btn_fine_minus || !btn_fine_plus || !btn_coarse_plus ||
            !l_btn_coarse_minus || !l_btn_fine_minus || !l_btn_fine_plus || !l_btn_coarse_plus) return;

        char b_emp[32], b_ful[32], b_off[32], b_sd[48];
        snprintf(b_emp, sizeof(b_emp), "%.3fV", sysState->empty_volts);
        snprintf(b_ful, sizeof(b_ful), "%.3fV    %.2f", sysState->full_volts, sysState->convertFromInHg(sysState->full_pressure_inHg));

        if (storageDisk != nullptr && storageDisk->isPresent()) {
            lv_obj_remove_state(btn_erase_sd, LV_STATE_DISABLED);
            float free_gb = storageDisk->getCardFreeSpaceGB();
            float total_gb = storageDisk->getCardSizeGB();
            snprintf(b_sd, sizeof(b_sd), "Free: %.1f/%.1f GB", free_gb, total_gb);
        } else {
            lv_obj_add_state(btn_erase_sd, LV_STATE_DISABLED);
            snprintf(b_sd, sizeof(b_sd), "SD Card Offline");
        }
        updateString(l_sd_info, b_sd);

        if (sysState->use_metric != last_known_unit_mode) {
            last_known_unit_mode = sysState->use_metric;
            if (!sysState->use_metric) {
                updateString(l_btn_coarse_minus, "-1'");
                updateString(l_btn_fine_minus, "-1\"");
                updateString(l_btn_fine_plus, "+1\"");
                updateString(l_btn_coarse_plus, "+1'");
                bindOffsetButtonPaylod(btn_coarse_minus, -12.0f);
                bindOffsetButtonPaylod(btn_fine_minus, -1.0f);
                bindOffsetButtonPaylod(btn_fine_plus, 1.0f);
                bindOffsetButtonPaylod(btn_coarse_plus, 12.0f);
            } else {
                updateString(l_btn_coarse_minus, "-10c");
                updateString(l_btn_fine_minus, "-1c");
                updateString(l_btn_fine_plus, "+1c");
                updateString(l_btn_coarse_plus, "+10c");
                bindOffsetButtonPaylod(btn_coarse_minus, -10.0f);
                bindOffsetButtonPaylod(btn_fine_minus, -1.0f);
                bindOffsetButtonPaylod(btn_fine_plus, 1.0f);
                bindOffsetButtonPaylod(btn_coarse_plus, 10.0f);
            }
        }
        if (!sysState->use_metric) {
            snprintf(b_off, sizeof(b_off), "%.1f in", sysState->offset_in);
        } else {
            snprintf(b_off, sizeof(b_off), "%.1f cm", sysState->offset_in * 2.54f);
        }
        char b_pressure_comp[16];

        snprintf(
            b_pressure_comp,
            sizeof(b_pressure_comp),
            "%.0f%%",
            sysState->pressure_compensation * 100.0f);

        updatePressureCompButtons();
        updateString(l_pressure_comp, b_pressure_comp);
        updateString(l_empty, b_emp);
        updateString(l_full, b_ful);
        updateString(l_offset, b_off);
    }
};
