/*
 * wifi_web.h — web interface HTTP routes
 *
 * Reads: app_snapshot and system_info. Settings writes: app_commands.
 * Routes registered on AsyncWebServer when a Wi-Fi session is active.
 */

#ifndef WIFI_WEB_H
#define WIFI_WEB_H

#include "config.h"

#if WIFI_ENABLE && defined(ARDUINO_ARCH_ESP32) && !defined(SIM_BUILD)

class AsyncWebServer;

void wifi_web_registerRoutes(AsyncWebServer *server);
void wifi_web_resetSession();

#else

struct AsyncWebServer;

inline void wifi_web_registerRoutes(AsyncWebServer *server) {
  (void)server;
}

inline void wifi_web_resetSession() {}

#endif

#endif // WIFI_WEB_H
