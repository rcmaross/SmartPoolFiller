#pragma once
#include <WiFi.h>

// Handle to process history web routing requests
bool handleHistoryWebRoutes(WiFiClient& client, const String& requestLine);

extern const char* INDEX_HISTORY_HTML;
