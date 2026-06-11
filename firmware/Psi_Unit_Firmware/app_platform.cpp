/*
 * app_platform.cpp — low-level Arduino Nano ESP32 platform services
 *
 * On startup, turns off all relays, configures Serial for debug, confirms
 * successful OTA update, enables WDT, and PWM for light dimming.
 */

#include "app_platform.h"
#include "config.h"
#include "storage.h"
#include "sensors.h"
#include "rtc_clock.h"
#include <RTClib.h>

#if !defined(ARDUINO_ARCH_ESP32)
#error "app_platform.cpp: Arduino Nano ESP32 only."
#endif

#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#if __has_include("USB.h")
#include "USB.h"
#define APP_USB_CDC_H 1
#endif

static bool lightPwmReady = false;
static bool serialHostSeen = false;

// Re-initialize NVS, SHT30, and RTC after I2C bus recovery.
void app_reinitPeripherals() {
  storage_begin();

  sensors_reinitSht30();
  rtc_initHardware();
  rtc_updateCache(true);
}

// All relays and light held LOW until logic initialization completes.
void app_forceOutputsOff() {
  pinMode(SSR_HEAT, OUTPUT);
  pinMode(SSR_HUM, OUTPUT);
  pinMode(SSR_INC, OUTPUT);
  pinMode(RELAY_FAN, OUTPUT);
  pinMode(RELAY_LIGHT, OUTPUT);
  digitalWrite(SSR_HEAT, LOW);
  digitalWrite(SSR_HUM, LOW);
  digitalWrite(SSR_INC, LOW);
  digitalWrite(RELAY_FAN, LOW);
  if (lightPwmReady) {
    app_setLightPwm(0);
  } else {
    digitalWrite(RELAY_LIGHT, LOW);
  }
}

// Confirm successful OTA update (cancel rollback to the previous version).
void app_confirmBootOk() {
  const esp_partition_t *running = esp_ota_get_running_partition();
  if (running == nullptr) {
    return;
  }
  esp_ota_img_states_t ota_state;
  if (esp_ota_get_state_partition(running, &ota_state) != ESP_OK) {
    return;
  }
  if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    Serial.print(F("OTA rollback cancel: "));
    Serial.println(err == ESP_OK ? F("OK") : F("FAIL"));
  }
}

void app_serialBegin() {
#if defined(APP_USB_CDC_H)
  USB.begin();
#endif
  Serial.begin(115200);
#if DEBUG_SERIAL
  const unsigned long waitMs = 3000UL;
#else
  const unsigned long waitMs = 200UL;
#endif
  unsigned long serialWaitStart = millis();
  while (!Serial && (millis() - serialWaitStart < waitMs)) {
    delay(10);
    yield();
  }
  serialHostSeen = Serial;
  delay(SERIAL_BOOT_WAIT_MS);
  Serial.flush();
}

void app_serialPollHost() {
  if (serialHostSeen || !Serial) {
    return;
  }
  serialHostSeen = true;
  Serial.println();
  Serial.println(F("=== Serial monitor connected ==="));
  Serial.println(FIRMWARE_VERSION);
  Serial.print(F("NVS settings: "));
  Serial.println(storage_isReady() ? F("OK") : F("missing"));
  Serial.print(F("Settings ver: "));
  Serial.println(settings.version);
}

void app_log(const __FlashStringHelper *msg) {
  Serial.println(msg);
}

void app_logBootInfo() {
  Serial.print(F("Build: "));
  Serial.println(__DATE__ " " __TIME__);
  Serial.print(F("Features: "));
  Serial.println(FIRMWARE_FEATURES);
  Serial.print(F("Sketch size: "));
  Serial.println(ESP.getSketchSize());
}

void app_watchdogEnable() {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
  esp_task_wdt_config_t wdtCfg = {};
  wdtCfg.timeout_ms = 8000;
  wdtCfg.idle_core_mask = 0;
  wdtCfg.trigger_panic = true;
  esp_task_wdt_init(&wdtCfg);
#else
  esp_task_wdt_init(8, true);
#endif
  esp_task_wdt_add(nullptr);
}

void app_watchdogReset() {
  esp_task_wdt_reset();
}

void app_pollHeapWarning(unsigned long nowMillis) {
#if HEAP_WARN_BYTES > 0
  static unsigned long lastPollMs = 0;
  static bool warnActive = false;

  if (nowMillis - lastPollMs < HEAP_POLL_INTERVAL_MS) {
    return;
  }
  lastPollMs = nowMillis;

  const uint32_t freeHeap = ESP.getFreeHeap();
  const uint32_t minHeap = ESP.getMinFreeHeap();
  const bool low = freeHeap < HEAP_WARN_BYTES || minHeap < HEAP_WARN_BYTES;
  if (low && !warnActive) {
    warnActive = true;
    Serial.print(F("WARN heap low: free="));
    Serial.print(freeHeap);
    Serial.print(F(" min="));
    Serial.println(minHeap);
  } else if (!low && warnActive) {
    warnActive = false;
    Serial.println(F("heap recovered"));
  }
#else
  (void)nowMillis;
#endif
}

// Light brightness PWM (LEDC on ESP32; API depends on arduino-esp32 core version).
static const uint8_t APP_LIGHT_LEDC_CH = 1;

static void app_lightPwmHwInit() {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  ledcAttach(RELAY_LIGHT, LIGHT_PWM_FREQ_HZ, LIGHT_PWM_RES_BITS);
#else
  ledcSetup(APP_LIGHT_LEDC_CH, LIGHT_PWM_FREQ_HZ, LIGHT_PWM_RES_BITS);
  ledcAttachPin(RELAY_LIGHT, APP_LIGHT_LEDC_CH);
#endif
}

static void app_lightPwmHwWrite(uint8_t duty) {
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  ledcWrite(RELAY_LIGHT, duty);
#else
  ledcWrite(APP_LIGHT_LEDC_CH, duty);
#endif
}

void app_initLightPwm() {
  pinMode(RELAY_LIGHT, OUTPUT);
  digitalWrite(RELAY_LIGHT, LOW);
  app_lightPwmHwInit();
  lightPwmReady = true;
}

void app_setLightPwm(uint8_t duty) {
  app_lightPwmHwWrite(duty);
}
