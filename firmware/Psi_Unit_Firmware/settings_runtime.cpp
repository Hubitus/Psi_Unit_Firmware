/*
 * settings_runtime.cpp — read Wi-Fi and log parameters from separate NVS storage
 */

#include "settings_runtime.h"
#include "wifi_manager.h"
#include "sensor_log.h"
#include "config.h"

bool settings_runtimeIsExternalField(SettingsFieldId field) {
  return field == SF_WIFI_TIMEOUT || field == SF_WIFI_AP_ENABLED ||
         field == SF_LOG_ENABLED || field == SF_LOG_INTERVAL;
}

uint8_t settings_runtimeGetUInt8(SettingsFieldId field) {
  switch (field) {
    case SF_WIFI_TIMEOUT:
      return wifi_manager_getSessionTimeoutMin();
    case SF_WIFI_AP_ENABLED:
      return wifi_manager_isActive() ? 1 : 0;
    case SF_LOG_ENABLED:
#if SENSOR_LOG_ENABLE
      return sensor_log_isCompiledIn() && sensor_log_isEnabled() ? 1 : 0;
#else
      return 0;
#endif
    case SF_LOG_INTERVAL:
#if SENSOR_LOG_ENABLE
      return sensor_log_isCompiledIn() ? sensor_log_getIntervalMin() : (uint8_t)5;
#else
      return 5;
#endif
    default:
      return 0;
  }
}
