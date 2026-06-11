/*
 * wifi_manager.cpp — Wi-Fi access point and web server (when WIFI_ENABLE=1)
 *
 * Creates a temporary SoftAP with password from NVS, limits session duration,
 * and serves the configuration web UI. Wi-Fi settings are stored separately from
 * main SavedSettings (namespace "psi_wifi"). Display data via system_info.
 */

#include "wifi_manager.h"
#include "app_platform.h"
#include "config.h"
#include "wifi_web.h"
#include "system_info.h"
#include <stdio.h>
#include <string.h>

#if WIFI_ENABLE && defined(ARDUINO_ARCH_ESP32) && !defined(SIM_BUILD)

#include <WiFi.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include <esp_random.h>
#include <esp_wifi.h>
#include <esp_system.h>

static Preferences s_wifiPrefs;
static AsyncWebServer s_webServer(WIFI_HTTP_PORT);
static bool s_webServerRunning = false;
static bool s_webRoutesRegistered = false;

static char s_apSsid[16];
static char s_apPass[WIFI_AP_PASS_LEN + 1];
static char s_statusText[16];
static bool s_sessionActive = false;
static unsigned long s_sessionStartMs = 0;
static unsigned long s_lastStatusPollMs = 0;
static uint8_t s_clientCount = 0;
static uint8_t s_sessionTimeoutMin = WIFI_AP_SESSION_MIN_DEFAULT;
static bool s_userApDisabled = false;
static bool s_webPending = false;
static unsigned long s_lastStartRetryMs = 0;
// Retry AP start only after failed WIFI_AP_AUTO_START at boot (not after manual OFF/timeout).
static bool s_autoStartRetry = false;

static const char *WIFI_NVS_NS = "psi_wifi";
static const char *WIFI_KEY_PASS = "apPass";
static const char *WIFI_KEY_TMO = "tmoMin";

static const uint8_t kSessionTimeoutChoices[WIFI_AP_SESSION_CHOICE_COUNT] = {
    WIFI_AP_SESSION_NEVER, 30, 60, 120, WIFI_AP_SESSION_MIN_MAX};

static bool wifi_sessionTimeoutAllowed(uint8_t minutes) {
  for (uint8_t i = 0; i < WIFI_AP_SESSION_CHOICE_COUNT; i++) {
    if (kSessionTimeoutChoices[i] == minutes) {
      return true;
    }
  }
  return false;
}

static uint8_t wifi_sessionTimeoutNormalize(uint8_t minutes) {
  if (wifi_sessionTimeoutAllowed(minutes)) {
    return minutes;
  }
  if (minutes == WIFI_AP_SESSION_NEVER) {
    return WIFI_AP_SESSION_NEVER;
  }
  uint8_t best = WIFI_AP_SESSION_MIN_DEFAULT;
  uint16_t bestDist = 0xFFFF;
  for (uint8_t i = 0; i < WIFI_AP_SESSION_CHOICE_COUNT; i++) {
    const uint8_t choice = kSessionTimeoutChoices[i];
    if (choice == WIFI_AP_SESSION_NEVER) {
      continue;
    }
    const uint16_t dist = (minutes > choice) ? (uint16_t)(minutes - choice) : (uint16_t)(choice - minutes);
    if (dist < bestDist) {
      bestDist = dist;
      best = choice;
    }
  }
  return best;
}
static const char *WIFI_KEY_USER_OFF = "apOff";

bool wifi_manager_startSession();
void wifi_manager_stopSession();

static void wifi_webBegin() {
  if (!s_webRoutesRegistered) {
    wifi_web_registerRoutes(&s_webServer);
    s_webRoutesRegistered = true;
  }
  if (s_webServerRunning) {
    return;
  }
  s_webServer.begin();
  s_webServerRunning = true;
}

static void wifi_webEnd() {
  if (!s_webServerRunning) {
    return;
  }
  wifi_web_resetSession();
  s_webServer.end();
  s_webServerRunning = false;
}

static void wifi_buildSsid() {
  const uint32_t id = (uint32_t)(ESP.getEfuseMac() & 0xFFFFUL);
  snprintf(s_apSsid, sizeof(s_apSsid), "%s%04X", WIFI_AP_SSID_PREFIX, (unsigned)id);
}

static void wifi_generatePassword() {
  static const char charset[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghjkmnpqrstuvwxyz23456789";
  const size_t n = sizeof(charset) - 1;
  for (uint8_t i = 0; i < WIFI_AP_PASS_LEN; i++) {
    s_apPass[i] = charset[esp_random() % n];
  }
  s_apPass[WIFI_AP_PASS_LEN] = '\0';
}

static void wifi_loadOrCreatePassword() {
  s_apPass[0] = '\0';
  if (!s_wifiPrefs.isKey(WIFI_KEY_PASS)) {
    wifi_generatePassword();
    s_wifiPrefs.putString(WIFI_KEY_PASS, s_apPass);
    return;
  }
  const size_t storedLen = s_wifiPrefs.getBytesLength(WIFI_KEY_PASS);
  if (storedLen < WIFI_AP_PASS_LEN) {
    wifi_generatePassword();
    s_wifiPrefs.putString(WIFI_KEY_PASS, s_apPass);
    return;
  }
  const size_t n = s_wifiPrefs.getBytes(WIFI_KEY_PASS, s_apPass, WIFI_AP_PASS_LEN + 1);
  if (n < WIFI_AP_PASS_LEN) {
    wifi_generatePassword();
    s_wifiPrefs.putString(WIFI_KEY_PASS, s_apPass);
    return;
  }
  s_apPass[WIFI_AP_PASS_LEN] = '\0';
}

static void wifi_loadUserApDisabled() {
  // AP is off by default until the user explicitly enables it in the menu.
  s_userApDisabled = s_wifiPrefs.getUChar(WIFI_KEY_USER_OFF, 1) != 0;
}

static void wifi_saveUserApDisabled(bool disabled) {
  s_userApDisabled = disabled;
  s_wifiPrefs.putUChar(WIFI_KEY_USER_OFF, disabled ? 1 : 0);
}

static void wifi_loadTimeoutMin() {
  const uint8_t tmo = s_wifiPrefs.getUChar(WIFI_KEY_TMO, WIFI_AP_SESSION_MIN_DEFAULT);
  s_sessionTimeoutMin = wifi_sessionTimeoutNormalize(tmo);
}

static void wifi_refreshStatusText() {
  if (!s_sessionActive) {
    strncpy(s_statusText, "off", sizeof(s_statusText));
    s_statusText[sizeof(s_statusText) - 1] = '\0';
    return;
  }
  if (s_clientCount == 0) {
    snprintf(s_statusText, sizeof(s_statusText), "on");
  } else {
    snprintf(s_statusText, sizeof(s_statusText), "on %ucl", (unsigned)s_clientCount);
  }
}

static void wifi_logSessionInfo() {
  Serial.println(F("[WiFi] SoftAP active"));
  Serial.print(F("[WiFi] SSID="));
  Serial.println(s_apSsid);
#if DEBUG_SERIAL
  Serial.print(F("[WiFi] Pass="));
  Serial.println(s_apPass);
#endif
  Serial.print(F("[WiFi] URL=http://"));
  Serial.println(WiFi.softAPIP());
}

static bool wifi_startAp() {
  wifi_buildSsid();

  WiFi.persistent(false);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);
  app_watchdogReset();

  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);

  const IPAddress apIp(192, 168, 4, 1);
  const IPAddress apGw(192, 168, 4, 1);
  const IPAddress apMask(255, 255, 255, 0);
  if (!WiFi.softAPConfig(apIp, apGw, apMask)) {
    Serial.println(F("[WiFi] softAPConfig failed"));
    return false;
  }

  if (!WiFi.softAP(s_apSsid, s_apPass, WIFI_AP_CHANNEL, false, WIFI_AP_MAX_CLIENTS)) {
    Serial.println(F("[WiFi] softAP failed"));
    return false;
  }

  WiFi.softAPsetHostname(WIFI_AP_HOSTNAME);
  delay(200);
  app_watchdogReset();
  wifi_logSessionInfo();
  return true;
}

static void wifi_stopAp() {
  wifi_webEnd();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
}

bool wifi_manager_isCompiledIn() {
  return true;
}

void wifi_manager_begin() {
  s_sessionActive = false;
  s_clientCount = 0;
  s_statusText[0] = '\0';
  wifi_buildSsid();
  s_apPass[0] = '\0';

  if (!s_wifiPrefs.begin(WIFI_NVS_NS, false)) {
    wifi_refreshStatusText();
    return;
  }
  wifi_loadOrCreatePassword();
  wifi_loadTimeoutMin();
  wifi_loadUserApDisabled();
  s_wifiPrefs.end();
  wifi_refreshStatusText();

#if WIFI_AP_AUTO_START
  if (s_userApDisabled) {
    s_userApDisabled = false;
  }
#endif

  if (!s_userApDisabled) {
    if (!wifi_manager_startSession()) {
      Serial.println(F("[WiFi] AP start failed"));
#if WIFI_AP_AUTO_START
      s_autoStartRetry = true;
      s_lastStartRetryMs = millis();
#endif
    }
  }
}

void wifi_manager_poll(unsigned long nowMillis) {
  if (!s_sessionActive) {
#if WIFI_AP_AUTO_START
    if (s_autoStartRetry && !s_userApDisabled) {
      if (millisIntervalElapsed(nowMillis, s_lastStartRetryMs, 5000UL)) {
        s_lastStartRetryMs = nowMillis;
        if (wifi_manager_startSession()) {
          s_autoStartRetry = false;
        }
      }
    }
#endif
    return;
  }
  if (s_webPending) {
    wifi_webBegin();
    s_webPending = false;
  }
  if (millisIntervalElapsed(nowMillis, s_lastStatusPollMs, 1000UL)) {
    s_lastStatusPollMs = nowMillis;
    s_clientCount = (uint8_t)WiFi.softAPgetStationNum();
    wifi_refreshStatusText();
  }
  if (s_sessionTimeoutMin != WIFI_AP_SESSION_NEVER) {
    const unsigned long sessionMs = (unsigned long)s_sessionTimeoutMin * 60000UL;
    const unsigned long upMs = nowMillis - s_sessionStartMs;
    if (upMs >= WIFI_SESSION_MIN_MS && upMs >= sessionMs) {
      wifi_manager_stopSession();
      wifi_manager_setUserApDisabled(true);
    }
  }
}

bool wifi_manager_isActive() {
  return s_sessionActive;
}

bool wifi_manager_startSession() {
  if (s_sessionActive) {
    return true;
  }
  if (!s_wifiPrefs.begin(WIFI_NVS_NS, false)) {
    return false;
  }
  wifi_loadOrCreatePassword();
  wifi_loadTimeoutMin();
  s_wifiPrefs.end();

  if (!wifi_startAp()) {
    return false;
  }
  s_autoStartRetry = false;
  s_webPending = true;
  s_sessionActive = true;
  s_userApDisabled = false;
  if (s_wifiPrefs.begin(WIFI_NVS_NS, false)) {
    wifi_saveUserApDisabled(false);
    s_wifiPrefs.end();
  }
  s_sessionStartMs = millis();
  s_lastStatusPollMs = s_sessionStartMs;
  s_lastStartRetryMs = s_sessionStartMs;
  s_clientCount = 0;
  wifi_refreshStatusText();
  system_info_refresh();
  return true;
}

void wifi_manager_stopSession() {
  if (!s_sessionActive) {
    return;
  }
  s_sessionActive = false;
  s_webPending = false;
  s_clientCount = 0;
  s_sessionStartMs = 0;
  wifi_stopAp();
  wifi_refreshStatusText();
  system_info_refresh();
}

void wifi_manager_getStatusText(char *dst, size_t dstLen) {
  if (dst == nullptr || dstLen == 0) {
    return;
  }
  if (s_statusText[0] == '\0') {
    strncpy(dst, "off", dstLen);
  } else {
    strncpy(dst, s_statusText, dstLen);
  }
  dst[dstLen - 1] = '\0';
}

bool wifi_manager_getApCredentials(char *ssidOut, size_t ssidLen, char *passOut, size_t passLen) {
  if (ssidOut == nullptr || ssidLen == 0 || passOut == nullptr || passLen == 0) {
    return false;
  }
  if (s_apSsid[0] == '\0') {
    wifi_buildSsid();
  }
  if (s_apPass[0] == '\0') {
    if (!s_wifiPrefs.begin(WIFI_NVS_NS, true)) {
      return false;
    }
    wifi_loadOrCreatePassword();
    s_wifiPrefs.end();
  }
  strncpy(ssidOut, s_apSsid, ssidLen);
  ssidOut[ssidLen - 1] = '\0';
  strncpy(passOut, s_apPass, passLen);
  passOut[passLen - 1] = '\0';
  return true;
}

const char *wifi_manager_getApIp() {
  return WIFI_AP_IP;
}

static void wifi_escapeQrField(char *dst, size_t dstLen, const char *src) {
  if (dst == nullptr || dstLen == 0) {
    return;
  }
  size_t w = 0;
  if (src == nullptr) {
    dst[0] = '\0';
    return;
  }
  for (; *src != '\0' && (w + 2) < dstLen; src++) {
    const char c = *src;
    if (c == '\\' || c == ';' || c == ',' || c == ':') {
      dst[w++] = '\\';
      if (w >= dstLen - 1) {
        break;
      }
    }
    dst[w++] = c;
  }
  dst[w] = '\0';
}

bool wifi_manager_buildJoinPayload(char *dst, size_t dstLen) {
  if (dst == nullptr || dstLen < 32) {
    return false;
  }
  char ssid[16];
  char pass[WIFI_AP_PASS_LEN + 1];
  if (!wifi_manager_getApCredentials(ssid, sizeof(ssid), pass, sizeof(pass))) {
    return false;
  }
  char escSsid[24];
  char escPass[20];
  wifi_escapeQrField(escSsid, sizeof(escSsid), ssid);
  wifi_escapeQrField(escPass, sizeof(escPass), pass);
  snprintf(dst, dstLen, "WIFI:T:WPA;S:%s;P:%s;;", escSsid, escPass);
  return true;
}

uint8_t wifi_manager_getSessionTimeoutMin() {
  return s_sessionTimeoutMin;
}

bool wifi_manager_setSessionTimeoutMin(uint8_t minutes) {
  const uint8_t normalized = wifi_sessionTimeoutNormalize(minutes);
  if (!wifi_sessionTimeoutAllowed(normalized)) {
    return false;
  }
  s_sessionTimeoutMin = normalized;
  if (!s_wifiPrefs.begin(WIFI_NVS_NS, false)) {
    return false;
  }
  s_wifiPrefs.putUChar(WIFI_KEY_TMO, normalized);
  s_wifiPrefs.end();
  return true;
}

bool wifi_manager_toggleSession() {
  if (s_sessionActive) {
    wifi_manager_stopSession();
    wifi_manager_setUserApDisabled(true);
    system_info_refresh();
    return false;
  }
  wifi_manager_setUserApDisabled(false);
  return wifi_manager_startSession();
}

bool wifi_manager_setUserApDisabled(bool disabled) {
  s_userApDisabled = disabled;
  if (!s_wifiPrefs.begin(WIFI_NVS_NS, false)) {
    return false;
  }
  wifi_saveUserApDisabled(disabled);
  s_wifiPrefs.end();
  return true;
}

bool wifi_manager_isUserApDisabled() {
  return s_userApDisabled;
}

#else // stubs — WIFI_ENABLE 0, sim, or non-ESP32

bool wifi_manager_isCompiledIn() {
#if WIFI_ENABLE
  return true;
#else
  return false;
#endif
}

void wifi_manager_begin() {}

void wifi_manager_poll(unsigned long nowMillis) {
  (void)nowMillis;
}

bool wifi_manager_isActive() {
  return false;
}

bool wifi_manager_startSession() {
  return false;
}

void wifi_manager_stopSession() {}

void wifi_manager_getStatusText(char *dst, size_t dstLen) {
  if (dst == nullptr || dstLen == 0) {
    return;
  }
  strncpy(dst, "off", dstLen);
  dst[dstLen - 1] = '\0';
}

bool wifi_manager_getApCredentials(char *ssidOut, size_t ssidLen, char *passOut, size_t passLen) {
  (void)ssidOut;
  (void)ssidLen;
  (void)passOut;
  (void)passLen;
  return false;
}

const char *wifi_manager_getApIp() {
  return "0.0.0.0";
}

bool wifi_manager_buildJoinPayload(char *dst, size_t dstLen) {
  (void)dst;
  (void)dstLen;
  return false;
}

uint8_t wifi_manager_getSessionTimeoutMin() {
  return 30;
}

bool wifi_manager_setSessionTimeoutMin(uint8_t minutes) {
  (void)minutes;
  return false;
}

bool wifi_manager_toggleSession() {
  return false;
}

bool wifi_manager_setUserApDisabled(bool disabled) {
  (void)disabled;
  return false;
}

bool wifi_manager_isUserApDisabled() {
  return false;
}

#endif
