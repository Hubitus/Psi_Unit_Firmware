/*
 * serial_cli.cpp — text commands in Serial Monitor for diagnostics
 *
 * Commands (115200 baud, line + Enter):
 *   help
 *   dump | dump all
 *   dump settings | dump sensors | dump outputs | dump ui | dump system | dump wifi
 *   wifi start | wifi stop   (only when WIFI_ENABLE=1)
 */

#include "serial_cli.h"
#include "config.h"
#include "types.h"
#include "storage.h"
#include "sensors.h"
#include "logic.h"
#include "display_ui.h"
#include "rtc_clock.h"
#include "i2c_bus.h"
#include "app_state.h"
#include "app_snapshot.h"
#include "wifi_manager.h"
#include <stdio.h>
#include <string.h>

#if !defined(SERIAL_CLI_ENABLE)
#define SERIAL_CLI_ENABLE 1
#endif

#if SERIAL_CLI_ENABLE

static char s_lineBuf[80];
static uint8_t s_lineLen = 0;

static void cli_printDeciTemp(const __FlashStringHelper *key, int16_t deciC) {
  if (!sensor_hasValue(deciC)) {
    Serial.print(key);
    Serial.println(F("=invalid"));
    return;
  }
  Serial.print(key);
  Serial.print(F("="));
  Serial.print(deciC / 10);
  Serial.print(F("."));
  Serial.print(abs(deciC % 10));
  Serial.println(F("\xC2\xB0""C"));
}

static const char *cli_modeName(uint8_t mode) {
  switch ((Mode)mode) {
    case MODE_AUTO: return "AUTO";
    case MODE_FORCE_ON: return "ON";
    case MODE_FORCE_OFF: return "OFF";
    case MODE_MANUAL: return "MAN";
    default: return "?";
  }
}

static const char *cli_uiStateName(UiState state) {
  switch (state) {
    case UI_MAIN: return "MAIN";
    case UI_MAIN_MENU: return "MAIN_MENU";
    case UI_SUB_MENU: return "SUB_MENU";
    case UI_EDIT: return "EDIT";
    case UI_CLOCK_EDIT: return "CLOCK_EDIT";
    case UI_CONFIRM_RESET: return "CONFIRM_RESET";
    case UI_SYSTEM_INFO: return "SYSTEM_INFO";
    default: return "?";
  }
}

static void cli_printSectionHeader(const __FlashStringHelper *title) {
  Serial.println();
  Serial.println(title);
}

static void cli_dumpSettings() {
  cli_printSectionHeader(F("--- settings ---"));
  Serial.print(F("nvs_ready="));
  Serial.println(storage_isReady() ? F("1") : F("0"));
  Serial.print(F("settings_ver="));
  Serial.println(settings.version);
  Serial.print(F("heat_mode="));
  Serial.println(cli_modeName(settings.heatMode));
  cli_printDeciTemp(F("t_target"), settings.tTarget);
  Serial.print(F("t_hyst="));
  Serial.println(settings.tHyst);
  Serial.print(F("hum_mode="));
  Serial.println(cli_modeName(settings.humMode));
  Serial.print(F("h_target="));
  Serial.println(settings.hTarget);
  Serial.print(F("h_hyst="));
  Serial.println(settings.hHyst);
  Serial.print(F("hum_max="));
  Serial.println(settings.humMax);
  Serial.print(F("hum_work="));
  Serial.println(settings.humWork);
  Serial.print(F("hum_rest="));
  Serial.println(settings.humRest);
  Serial.print(F("hum_work_unit="));
  Serial.println(settings.humWorkUnit == UNIT_SECONDS ? F("sec") : F("min"));
  Serial.print(F("hum_rest_unit="));
  Serial.println(settings.humRestUnit == UNIT_SECONDS ? F("sec") : F("min"));
  Serial.print(F("fan_mode="));
  Serial.println(cli_modeName(settings.fanMode));
  Serial.print(F("fan_work="));
  Serial.println(settings.fanWork);
  Serial.print(F("fan_rest="));
  Serial.println(settings.fanRest);
  Serial.print(F("fan_work_unit="));
  Serial.println(settings.fanWorkUnit == UNIT_SECONDS ? F("sec") : F("min"));
  Serial.print(F("fan_rest_unit="));
  Serial.println(settings.fanRestUnit == UNIT_SECONDS ? F("sec") : F("min"));
  Serial.print(F("light_mode="));
  Serial.println(cli_modeName(settings.lightMode));
  Serial.print(F("light_hour="));
  Serial.println(settings.lightHour);
  Serial.print(F("light_duration="));
  Serial.println(settings.lightDuration);
  Serial.print(F("light_brightness="));
  Serial.println(settings.lightBrightness);
  Serial.print(F("inc_mode="));
  Serial.println(cli_modeName(settings.incMode));
  cli_printDeciTemp(F("inc_target"), settings.incTarget);
  Serial.print(F("inc_hyst="));
  Serial.println(settings.incHyst);
  Serial.print(F("sleep_min="));
  Serial.println(settings.sleepMin);
  Serial.print(F("brightness_raw="));
  Serial.println(settings.brightness);
  Serial.print(F("display_rotated="));
  Serial.println(settings.displayRotated);
  Serial.print(F("display_inverted="));
  Serial.println(settings.displayInverted);
  Serial.print(F("rgb_led_enabled="));
  Serial.println(settings.rgbLedEnabled);
  Serial.print(F("rgb_brightness_raw="));
  Serial.println(settings.rgbBrightness);
}

static void cli_dumpSensors() {
  cli_printSectionHeader(F("--- sensors ---"));
  cli_printDeciTemp(F("inc_temp"), tDS);
  Serial.print(F("inc_valid="));
  Serial.println(ds18State.dataValid ? F("1") : F("0"));
  Serial.print(F("inc_error="));
  Serial.println(dsError ? F("1") : F("0"));
  cli_printDeciTemp(F("grh_temp"), tSHT);
  Serial.print(F("grh_temp_valid="));
  Serial.println(shtTempState.dataValid ? F("1") : F("0"));
  Serial.print(F("temp_error="));
  Serial.println(tempError ? F("1") : F("0"));
  Serial.print(F("grh_hum="));
  if (sensor_hasValue(hSHT)) {
    Serial.println(hSHT);
  } else {
    Serial.println(F("invalid"));
  }
  Serial.print(F("grh_hum_valid="));
  Serial.println(shtHumState.dataValid ? F("1") : F("0"));
  Serial.print(F("hum_error="));
  Serial.println(humError ? F("1") : F("0"));
}

static void cli_dumpOutputs() {
  cli_printSectionHeader(F("--- outputs ---"));
  Serial.print(F("heat_on="));
  Serial.println(heatOn ? F("1") : F("0"));
  Serial.print(F("hum_on="));
  Serial.println(humOn ? F("1") : F("0"));
  Serial.print(F("fan_on="));
  Serial.println(fanOn ? F("1") : F("0"));
  Serial.print(F("light_on="));
  Serial.println(lightOn ? F("1") : F("0"));
  Serial.print(F("inc_on="));
  Serial.println(incOn ? F("1") : F("0"));
  Serial.print(F("light_override="));
  Serial.println(logic_isLightOverrideActive() ? F("1") : F("0"));
  Serial.print(F("hum_phase="));
  Serial.println(humPhase ? F("work") : F("rest"));
  Serial.print(F("fan_phase="));
  Serial.println(fanPhase ? F("work") : F("rest"));
}

static void cli_dumpUi() {
  cli_printSectionHeader(F("--- ui ---"));
  Serial.print(F("ui_state="));
  Serial.println(cli_uiStateName(uiState));
  Serial.print(F("main_menu_index="));
  Serial.println(mainMenuIndex);
  Serial.print(F("current_section="));
  Serial.println(currentSection);
  Serial.print(F("sub_menu_index="));
  Serial.println(subMenuIndex);
  Serial.print(F("is_sleep="));
  Serial.println(isSleep ? F("1") : F("0"));
  Serial.print(F("splash_active="));
  Serial.println(splashActive ? F("1") : F("0"));
  Serial.print(F("need_redraw="));
  Serial.println(ui_needsRedraw() ? F("1") : F("0"));
}

static void cli_dumpSnapshot() {
  AppSnapshot snap;
  app_buildSnapshot(snap);
  cli_printSectionHeader(F("--- snapshot ---"));
  Serial.print(F("nvs_ready="));
  Serial.println(snap.nvsReady ? F("1") : F("0"));
  Serial.print(F("rtc_ok="));
  Serial.println(snap.rtc.ok ? F("1") : F("0"));
  cli_printDeciTemp(F("inc_temp"), snap.incTemp.valid ? snap.incTemp.value : (int16_t)SENSOR_INVALID);
  cli_printDeciTemp(F("grh_temp"), snap.grhTemp.valid ? snap.grhTemp.value : (int16_t)SENSOR_INVALID);
  Serial.print(F("grh_hum="));
  if (snap.grhHum.valid) {
    Serial.println(snap.grhHum.value);
  } else {
    Serial.println(F("invalid"));
  }
  Serial.print(F("heat_on="));
  Serial.println(snap.outputs.heatOn ? F("1") : F("0"));
  Serial.print(F("light_override="));
  Serial.println(snap.outputs.lightOverride ? F("1") : F("0"));
}

static void cli_dumpSystem() {
  cli_printSectionHeader(F("--- system ---"));
  Serial.print(F("firmware="));
  Serial.println(FIRMWARE_VERSION);
  Serial.print(F("features="));
  Serial.println(FIRMWARE_FEATURES);
  Serial.print(F("uptime_ms="));
  Serial.println(millis());
  Serial.print(F("i2c_healthy="));
  Serial.println(i2c_isHealthy() ? F("1") : F("0"));
  Serial.print(F("i2c_fail_count="));
  Serial.println(i2c_failureCount());
  Serial.print(F("oled_present="));
  Serial.println(i2c_isOledPresent() ? F("1") : F("0"));
  Serial.print(F("rtc_present="));
  Serial.println(i2c_isRtcPresent() ? F("1") : F("0"));
  Serial.print(F("rtc_error="));
  Serial.println(rtcError ? F("1") : F("0"));
  Serial.print(F("rtc_valid="));
  Serial.println(rtc_hasValidNow() ? F("1") : F("0"));
  if (rtc_hasValidNow()) {
    const DateTime &now = rtc_getNow();
    char buf[20];
    snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u:%02u",
             now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
    Serial.print(F("rtc_now="));
    Serial.println(buf);
  }
#if defined(ARDUINO_ARCH_ESP32)
  Serial.print(F("free_heap="));
  Serial.println(ESP.getFreeHeap());
#endif
}

static void cli_dumpWifi() {
  cli_printSectionHeader(F("--- wifi ---"));
  Serial.print(F("compiled_in="));
  Serial.println(wifi_manager_isCompiledIn() ? F("1") : F("0"));
  Serial.print(F("active="));
  Serial.println(wifi_manager_isActive() ? F("1") : F("0"));
  char status[16];
  wifi_manager_getStatusText(status, sizeof(status));
  Serial.print(F("status="));
  Serial.println(status);
  if (wifi_manager_isCompiledIn()) {
    char ssid[16];
    char pass[WIFI_AP_PASS_LEN + 1];
    if (wifi_manager_getApCredentials(ssid, sizeof(ssid), pass, sizeof(pass))) {
      Serial.print(F("ssid="));
      Serial.println(ssid);
      Serial.print(F("pass="));
      Serial.println(pass);
      Serial.print(F("url=http://"));
      Serial.println(wifi_manager_getApIp());
    }
#if defined(ARDUINO_ARCH_ESP32)
    Serial.print(F("free_heap="));
    Serial.println(ESP.getFreeHeap());
#endif
  } else {
    Serial.println(F("hint=WIFI_ENABLE 0 in config.h"));
  }
}

static void cli_wifiStart() {
  if (!wifi_manager_isCompiledIn()) {
    Serial.println(F("WiFi disabled in build (WIFI_ENABLE=0)"));
    return;
  }
  if (wifi_manager_startSession()) {
    cli_dumpWifi();
  } else {
    Serial.println(F("wifi start failed"));
  }
}

static void cli_wifiStop() {
  if (!wifi_manager_isCompiledIn()) {
    Serial.println(F("WiFi disabled in build (WIFI_ENABLE=0)"));
    return;
  }
  wifi_manager_stopSession();
  wifi_manager_setUserApDisabled(true);
  Serial.println(F("wifi stopped"));
}

static void cli_dumpAll() {
  Serial.println(F("=== dump begin ==="));
  cli_dumpSystem();
  cli_dumpSettings();
  cli_dumpSensors();
  cli_dumpOutputs();
  cli_dumpSnapshot();
  cli_dumpUi();
  Serial.println(F("=== dump end ==="));
}

static void cli_printHelp() {
  Serial.println(F("Serial CLI (migration baseline)"));
  Serial.println(F("  help"));
  Serial.println(F("  dump | dump all"));
  Serial.println(F("  dump settings | dump sensors | dump outputs | dump snapshot | dump ui | dump system | dump wifi"));
  Serial.println(F("  wifi start | wifi stop"));
}

static void cli_handleLine(char *line) {
  while (*line == ' ' || *line == '\t') {
    line++;
  }
  if (*line == '\0') {
    return;
  }

  if (strcmp(line, "help") == 0 || strcmp(line, "?") == 0) {
    cli_printHelp();
    return;
  }

  if (strcmp(line, "dump") == 0 || strcmp(line, "dump all") == 0) {
    cli_dumpAll();
    return;
  }
  if (strcmp(line, "dump settings") == 0) {
    cli_dumpSettings();
    return;
  }
  if (strcmp(line, "dump sensors") == 0) {
    cli_dumpSensors();
    return;
  }
  if (strcmp(line, "dump outputs") == 0) {
    cli_dumpOutputs();
    return;
  }
  if (strcmp(line, "dump snapshot") == 0) {
    cli_dumpSnapshot();
    return;
  }
  if (strcmp(line, "dump ui") == 0) {
    cli_dumpUi();
    return;
  }
  if (strcmp(line, "dump system") == 0) {
    cli_dumpSystem();
    return;
  }
  if (strcmp(line, "dump wifi") == 0) {
    cli_dumpWifi();
    return;
  }
  if (strcmp(line, "wifi start") == 0) {
    cli_wifiStart();
    return;
  }
  if (strcmp(line, "wifi stop") == 0) {
    cli_wifiStop();
    return;
  }

  Serial.print(F("Unknown command: "));
  Serial.println(line);
  Serial.println(F("Type help"));
}

void serial_cli_poll() {
  while (Serial.available() > 0) {
    const char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (s_lineLen > 0) {
        s_lineBuf[s_lineLen] = '\0';
        cli_handleLine(s_lineBuf);
        s_lineLen = 0;
      }
      continue;
    }
    if (s_lineLen < sizeof(s_lineBuf) - 1) {
      s_lineBuf[s_lineLen++] = c;
    }
  }
}

#else

void serial_cli_poll() {}

#endif // SERIAL_CLI_ENABLE
