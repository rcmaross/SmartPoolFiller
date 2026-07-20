#pragma once
#include <lvgl.h>

class BaseTab {
public:
    virtual ~BaseTab() = default;
    
    virtual void setup(lv_obj_t* tab_container) = 0;
    virtual void update(bool force=false) = 0;

protected:
    // =====================================================================
    // UTILITY OVERLOAD 1: TEXT-ONLY RUNTIME UPDATE
    // =====================================================================
    void updateString(lv_obj_t* label_obj, const char* new_text) {
        if (!label_obj) return;
        lv_label_set_text(label_obj, new_text);
    }

    // =====================================================================
    // UTILITY OVERLOAD 2: TEXT + COLOR RUNTIME UPDATE
    // =====================================================================
    void updateString(lv_obj_t* label_obj, const char* new_text, lv_color_t color) {
        if (!label_obj) return;
        updateString(label_obj, new_text); 
        lv_obj_set_style_text_color(label_obj, color, 0); 
    }

    // =====================================================================
    // FACTORY WIDGET CONSTRUCTOR 1: RECTANGLES / SHAPES
    // =====================================================================
    lv_obj_t* createRectangle(lv_obj_t* parent, int width, int height = LV_SIZE_CONTENT, lv_color_t color = lv_color_black(), int x = 0, int y = 0) {
        lv_obj_t* rect = lv_obj_create(parent);
        lv_obj_set_width(rect, width);
        if (height != LV_SIZE_CONTENT) lv_obj_set_height(rect, height);
        if (x != 0 || y != 0)           lv_obj_set_pos(rect, x, y);
        
        lv_obj_set_style_border_width(rect, 0, 0);
        lv_obj_set_style_radius(rect, 0, 0);
        lv_obj_set_style_pad_all(rect, 0, 0);
        lv_obj_set_style_bg_opa(rect, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(rect, color, 0);
        return rect;
    }

    // =====================================================================
    // FACTORY WIDGET CONSTRUCTOR 2: TEXT LABELS
    // =====================================================================
    lv_obj_t* createString(lv_obj_t* parent, const char* default_text, int font_size, int x = 0, int y = 0, lv_color_t color = lv_color_black()) {
        lv_obj_t* lbl = lv_label_create(parent);
        lv_label_set_text(lbl, default_text);
        lv_obj_set_style_text_color(lbl, color, 0);
        if (x != 0 || y != 0) lv_obj_set_pos(lbl, x, y);
        
        if (font_size == 14)       lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        else if (font_size == 18)  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, 0);
        else if (font_size == 24)  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
        else if (font_size == 32)  lv_obj_set_style_text_font(lbl, &lv_font_montserrat_32, 0);
        else                       lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
        return lbl;
    }

    // Generates a button frame, registers standard click callbacks, mounts a 
    // centered typography label, and outputs a reference handle to the INNER text block.
    lv_obj_t* createButtonWithTextHandle(lv_obj_t* parent, int width, int height, const char* label_text, lv_event_cb_t callback, void* user_data, lv_obj_t*& out_btn_handle) {
        out_btn_handle = lv_button_create(parent);
        lv_obj_set_size(out_btn_handle, width, height);
        lv_obj_set_style_radius(out_btn_handle, 4, 0);
        
        if (callback != nullptr) {
            lv_obj_add_event_cb(out_btn_handle, callback, LV_EVENT_CLICKED, user_data);
        }
        
        lv_obj_t* inner_lbl = lv_label_create(out_btn_handle);
        lv_label_set_text(inner_lbl, label_text);
        lv_obj_set_style_text_font(inner_lbl, &lv_font_montserrat_14, 0);
        lv_obj_center(inner_lbl);
        
        return inner_lbl; // Returns reference to text element to support active updates
    }
};
