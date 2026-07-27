#pragma once
#include <Wifi.h>
#include <lvgl.h>

extern const char* INDEX_HTML PROGMEM;
extern const char* SHARED_CSS PROGMEM; 
extern const char* HEADER_HTML PROGMEM;
extern const char* NAVIGATION_HTML PROGMEM;
extern const char* FOOTER_HTML PROGMEM;


// Direct handle to execute inside main.ino's setup() process
void initPoolWebServer(WiFiServer *server);
void handlePoolWebClient(WiFiServer *server);

// The Core Object Registry API
void registerUiObj(const char* name, lv_obj_t* obj);
lv_obj_t* getObjByName(const char* name);
