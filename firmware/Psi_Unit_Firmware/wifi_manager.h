/*
 * wifi_manager.h — optional SoftAP + web (read: system_info/app_snapshot, write: app_commands)
 *
 * WIFI_ENABLE 0: all functions are no-op, status "off", no WiFi.h / AsyncWebServer.
 * Domain modules (sensors, rtc, logic) do not include this header.
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <stddef.h>

bool wifi_manager_isCompiledIn();
void wifi_manager_begin();
void wifi_manager_poll(unsigned long nowMillis);
bool wifi_manager_isActive();
bool wifi_manager_startSession();
void wifi_manager_stopSession();
void wifi_manager_getStatusText(char *dst, size_t dstLen);
bool wifi_manager_getApCredentials(char *ssidOut, size_t ssidLen, char *passOut, size_t passLen);
const char *wifi_manager_getApIp();
uint8_t wifi_manager_getSessionTimeoutMin();
bool wifi_manager_setSessionTimeoutMin(uint8_t minutes);
bool wifi_manager_buildJoinPayload(char *dst, size_t dstLen);
bool wifi_manager_toggleSession();
bool wifi_manager_setUserApDisabled(bool disabled);
bool wifi_manager_isUserApDisabled();

#endif // WIFI_MANAGER_H
