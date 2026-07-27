#pragma once
#include <WiFi.h>

// Handle to pass the incoming network traffic down to the download routers
bool handleDownloadWebRoutes(WiFiClient& client, const String& requestLine);

// Flash string variable tracking the Download tab HTML sub-fragment
extern const char* INDEX_DOWNLOAD_HTML;
