/*
 * rgb_status.cpp — onboard RGB LED control on Nano ESP32
 *
 * Color and breathing pattern depend on sensor errors and Wi‑Fi SoftAP activity.
 * Brightness envelope: exp(sin) LUT + gamma; PWM via LEDC (not I2C).
 * poll() is short (~LUT + up to 3× ledcWrite), called from the end of loop().
 */

#include "rgb_status.h"
#include "app_state.h"
#include "sensors.h"
#include "storage.h"
#include "wifi_manager.h"

#if !defined(ARDUINO_ARCH_ESP32)
#error "rgb_status.cpp: Arduino Nano ESP32 only."
#endif

static RgbStatusMode currentMode = RGB_MODE_OFF;
static bool hwReady = false;

#if !defined(LED_RED)
#define LED_RED 14
#define LED_GREEN 15
#define LED_BLUE 16
#endif

#define RGB_STATUS_HW 1

static const uint32_t RGB_PWM_FREQ_HZ = 2000UL;
static const uint8_t RGB_PWM_RES_BITS = 10;
static const uint16_t RGB_PWM_MAX = (1U << RGB_PWM_RES_BITS) - 1U;
static const uint16_t RGB_PWM_OFF = RGB_PWM_MAX;

static const unsigned long RGB_POLL_MS = 20UL;
static const unsigned long RGB_MODE_TO_ERR_MS = 2000UL;
static const unsigned long RGB_MODE_TO_OK_MS = 500UL;

static const uint8_t RGB_PEAK_LEVEL = 32;

static const unsigned long RGB_BREATHE_PERIOD_MS = 6000UL;
static const unsigned long RGB_ERR_COLOR_MS = 6825UL;
static const uint8_t RGB_ENVELOPE_PEAK = 255;

static const uint8_t RGB_OK_G = 220;
static const uint8_t RGB_WIFI_B = 220;

static const uint8_t RGB_ERR_A_R = 220;
static const uint8_t RGB_ERR_A_G = 80;
static const uint8_t RGB_ERR_A_B = 0;
static const uint8_t RGB_ERR_B_R = 220;
static const uint8_t RGB_ERR_B_G = 20;
static const uint8_t RGB_ERR_B_B = 0;

// exp-shaped rise/fall: 0 only at phase 0 and 255, no flat pause at minimum
static const uint8_t RGB_BREATHE_EXP_SIN_LUT[256] PROGMEM = {
   0,   1,   1,   1,   2,   2,   2,   3,   3,   4,   4,   4,   5,   5,   6,   6,
   7,   7,   8,   8,   9,   9,  10,  10,  11,  11,  12,  13,  13,  14,  14,  15,
  16,  16,  17,  18,  19,  19,  20,  21,  22,  23,  24,  24,  25,  26,  27,  28,
  29,  30,  31,  32,  33,  34,  36,  37,  38,  39,  40,  42,  43,  44,  46,  47,
  49,  50,  52,  53,  55,  56,  58,  60,  62,  63,  65,  67,  69,  71,  73,  75,
  77,  79,  82,  84,  86,  89,  91,  93,  96,  99, 101, 104, 107, 110, 113, 116,
 119, 122, 125, 128, 132, 135, 139, 143, 146, 150, 154, 158, 162, 166, 171, 175,
 179, 184, 189, 194, 199, 204, 209, 214, 220, 225, 231, 237, 243, 249, 255, 255,
 255, 255, 254, 254, 254, 253, 253, 253, 252, 252, 251, 251, 251, 250, 250, 249,
 249, 248, 248, 247, 247, 246, 246, 245, 245, 244, 244, 243, 242, 242, 241, 241,
 240, 239, 239, 238, 237, 236, 236, 235, 234, 233, 232, 231, 231, 230, 229, 228,
 227, 226, 225, 224, 223, 222, 221, 219, 218, 217, 216, 215, 213, 212, 211, 209,
 208, 206, 205, 203, 202, 200, 199, 197, 195, 193, 192, 190, 188, 186, 184, 182,
 180, 178, 176, 173, 171, 169, 166, 164, 162, 159, 156, 154, 151, 148, 145, 142,
 139, 136, 133, 130, 127, 123, 120, 116, 112, 109, 105, 101,  97,  93,  89,  84,
  80,  76,  71,  66,  61,  56,  51,  46,  41,  35,  30,  24,  18,  12,   6,   0
};

// Adafruit gamma 2.8 — perceptually linear PWM
static const uint8_t RGB_GAMMA8_LUT[256] PROGMEM = {
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   1,
  1,   1,   1,   1,   1,   1,   1,   1,   1,   2,   2,   2,   2,   2,   2,   2,
  2,   3,   3,   3,   3,   3,   3,   3,   4,   4,   4,   4,   4,   5,   5,   5,
  5,   6,   6,   6,   6,   7,   7,   7,   7,   8,   8,   8,   9,   9,   9,  10,
  10,  10,  11,  11,  11,  12,  12,  13,  13,  13,  14,  14,  15,  15,  16,  16,
  17,  17,  18,  18,  19,  19,  20,  20,  21,  21,  22,  22,  23,  24,  24,  25,
  25,  26,  27,  27,  28,  29,  29,  30,  31,  32,  32,  33,  34,  35,  35,  36,
  37,  38,  39,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  50,
  51,  52,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,  66,  67,  68,
  69,  70,  72,  73,  74,  75,  77,  78,  79,  81,  82,  83,  85,  86,  87,  89,
  90,  92,  93,  95,  96,  98,  99, 101, 102, 104, 105, 107, 109, 110, 112, 114,
  115, 117, 119, 120, 122, 124, 126, 127, 129, 131, 133, 135, 137, 138, 140, 142,
  144, 146, 148, 150, 152, 154, 156, 158, 160, 162, 164, 167, 169, 171, 173, 175,
  177, 180, 182, 184, 186, 189, 191, 193, 196, 198, 200, 203, 205, 208, 210, 213,
  215, 218, 220, 223, 225, 228, 231, 233, 236, 239, 241, 244, 247, 249, 252, 255
};

enum RgbEnvelopeId : uint8_t {
  RGB_ENVELOPE_EXP_SIN = 0,
  RGB_ENVELOPE_COUNT
};

struct RgbEnvelopeDef {
  const uint8_t *lut;
  unsigned long periodMs;
};

struct RgbColor {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

static const RgbEnvelopeDef RGB_ENVELOPE_TABLE[RGB_ENVELOPE_COUNT] = {
    {RGB_BREATHE_EXP_SIN_LUT, RGB_BREATHE_PERIOD_MS},
};

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
static void rgb_ledcAttach(uint8_t pin) {
  ledcAttach(pin, RGB_PWM_FREQ_HZ, RGB_PWM_RES_BITS);
}

static void rgb_ledcWrite(uint8_t pin, uint16_t duty) {
  ledcWrite(pin, duty);
}
#else
static const uint8_t RGB_LED_LEDC_CH_R = 4;
static const uint8_t RGB_LED_LEDC_CH_G = 5;
static const uint8_t RGB_LED_LEDC_CH_B = 6;

static void rgb_ledcAttach(uint8_t pin, uint8_t channel) {
  ledcSetup(channel, RGB_PWM_FREQ_HZ, RGB_PWM_RES_BITS);
  ledcAttachPin(pin, channel);
}

static void rgb_ledcWrite(uint8_t channel, uint16_t duty) {
  ledcWrite(channel, duty);
}
#endif

static unsigned long rgb_lastPollMs = 0;
static RgbStatusMode rgb_pendingMode = RGB_MODE_OK;
static unsigned long rgb_modePendingSinceMs = 0;
static uint16_t rgb_lastDutyR = RGB_PWM_OFF;
static uint16_t rgb_lastDutyG = RGB_PWM_OFF;
static uint16_t rgb_lastDutyB = RGB_PWM_OFF;
static bool rgb_brightnessPreviewActive = false;
static uint8_t rgb_brightnessPreviewRaw = 255;

static uint8_t rgb_effectiveBrightnessRaw() {
  return rgb_brightnessPreviewActive ? rgb_brightnessPreviewRaw : settings.rgbBrightness;
}

static uint8_t rgb_triangleU8(unsigned long nowMillis, unsigned long periodMs) {
  if (periodMs == 0) {
    return 0;
  }
  const unsigned long t = nowMillis % periodMs;
  const unsigned long half = periodMs / 2UL;
  if (half == 0) {
    return 0;
  }
  if (t < half) {
    return (uint8_t)((t * 255UL) / half);
  }
  return (uint8_t)(((periodMs - t) * 255UL) / half);
}

static uint8_t rgb_lerpU8(uint8_t a, uint8_t b, uint8_t t) {
  return (uint8_t)(((uint16_t)a * (255U - t) + (uint16_t)b * t + 127U) / 255U);
}

static const RgbEnvelopeDef *rgb_envelopeDef(RgbEnvelopeId id) {
  if (id >= RGB_ENVELOPE_COUNT) {
    return &RGB_ENVELOPE_TABLE[RGB_ENVELOPE_EXP_SIN];
  }
  return &RGB_ENVELOPE_TABLE[id];
}

// Envelope pattern used for each indication mode.
static RgbEnvelopeId rgb_envelopeForMode(RgbStatusMode mode) {
  (void)mode;
  return RGB_ENVELOPE_EXP_SIN;
}

static uint8_t rgb_envelopeSample(const RgbEnvelopeDef *def, unsigned long nowMillis) {
  if (def == nullptr || def->lut == nullptr || def->periodMs == 0) {
    return 0;
  }
  const unsigned long t = nowMillis % def->periodMs;
  const uint8_t phase = (uint8_t)((t * 256UL) / def->periodMs);
  return pgm_read_byte(&def->lut[phase]);
}

// gamma → peak → user brightness → 10-bit on amount (before per-channel scaling).
static uint16_t rgb_envelopeToOnDuty(uint8_t envelopeRaw) {
  const uint8_t perceptual = pgm_read_byte(&RGB_GAMMA8_LUT[envelopeRaw]);
  const uint32_t scaled =
      (uint32_t)perceptual * RGB_PEAK_LEVEL * rgb_effectiveBrightnessRaw();
  return (uint16_t)((scaled * RGB_PWM_MAX) / (255UL * 255UL * 255UL));
}

static uint16_t rgb_channelDuty(uint8_t base, uint16_t envelopeOnDuty) {
  if (base == 0 || envelopeOnDuty == 0) {
    return RGB_PWM_OFF;
  }
  const uint32_t on = ((uint32_t)base * envelopeOnDuty) / 255UL;
  if (on >= RGB_PWM_MAX) {
    return 0;
  }
  return (uint16_t)(RGB_PWM_OFF - on);
}

static void rgb_hwOff() {
  rgb_lastDutyR = RGB_PWM_OFF;
  rgb_lastDutyG = RGB_PWM_OFF;
  rgb_lastDutyB = RGB_PWM_OFF;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  rgb_ledcWrite(LED_RED, RGB_PWM_OFF);
  rgb_ledcWrite(LED_GREEN, RGB_PWM_OFF);
  rgb_ledcWrite(LED_BLUE, RGB_PWM_OFF);
#else
  rgb_ledcWrite(RGB_LED_LEDC_CH_R, RGB_PWM_OFF);
  rgb_ledcWrite(RGB_LED_LEDC_CH_G, RGB_PWM_OFF);
  rgb_ledcWrite(RGB_LED_LEDC_CH_B, RGB_PWM_OFF);
#endif
}

static void rgb_hwWriteDuty(uint16_t dutyR, uint16_t dutyG, uint16_t dutyB) {
  if (dutyR == rgb_lastDutyR && dutyG == rgb_lastDutyG && dutyB == rgb_lastDutyB) {
    return;
  }
  rgb_lastDutyR = dutyR;
  rgb_lastDutyG = dutyG;
  rgb_lastDutyB = dutyB;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  rgb_ledcWrite(LED_RED, dutyR);
  rgb_ledcWrite(LED_GREEN, dutyG);
  rgb_ledcWrite(LED_BLUE, dutyB);
#else
  rgb_ledcWrite(RGB_LED_LEDC_CH_R, dutyR);
  rgb_ledcWrite(RGB_LED_LEDC_CH_G, dutyG);
  rgb_ledcWrite(RGB_LED_LEDC_CH_B, dutyB);
#endif
}

static void rgb_hwInit() {
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  rgb_ledcAttach(LED_RED);
  rgb_ledcAttach(LED_GREEN);
  rgb_ledcAttach(LED_BLUE);
#else
  rgb_ledcAttach(LED_RED, RGB_LED_LEDC_CH_R);
  rgb_ledcAttach(LED_GREEN, RGB_LED_LEDC_CH_G);
  rgb_ledcAttach(LED_BLUE, RGB_LED_LEDC_CH_B);
#endif
  rgb_hwOff();
  hwReady = true;
}

static bool rgb_isDisplayAwake() {
  return !isSleep;
}

static void rgb_renderColor(const RgbColor &base, uint8_t envelopeRaw) {
  const uint16_t onDuty = rgb_envelopeToOnDuty(envelopeRaw);
  rgb_hwWriteDuty(rgb_channelDuty(base.r, onDuty), rgb_channelDuty(base.g, onDuty),
                  rgb_channelDuty(base.b, onDuty));
}

static void rgb_renderBreathing(unsigned long nowMillis, RgbEnvelopeId envelopeId, const RgbColor &base) {
  const RgbEnvelopeDef *def = rgb_envelopeDef(envelopeId);
  const uint8_t envelope = rgb_envelopeSample(def, nowMillis);
  rgb_renderColor(base, envelope);
}

static void rgb_renderSteady(const RgbColor &base) {
  rgb_renderColor(base, RGB_ENVELOPE_PEAK);
}

static bool rgb_sensorHasErrors() {
  return tempError || humError || dsError;
}

static RgbStatusMode rgb_resolveMode() {
  if (rgb_sensorHasErrors()) {
    return RGB_MODE_ERROR;
  }
  if (wifi_manager_isCompiledIn() && wifi_manager_isActive()) {
    return RGB_MODE_WIFI;
  }
  return RGB_MODE_OK;
}

static void rgb_applyModeDebounce(RgbStatusMode target, unsigned long nowMillis) {
  if (target != rgb_pendingMode) {
    rgb_pendingMode = target;
    rgb_modePendingSinceMs = nowMillis;
    return;
  }
  if (target == currentMode) {
    return;
  }
  unsigned long holdMs = (target == RGB_MODE_ERROR) ? RGB_MODE_TO_ERR_MS : RGB_MODE_TO_OK_MS;
  if (nowMillis - rgb_modePendingSinceMs >= holdMs) {
    currentMode = target;
  }
}

static RgbColor rgb_errorColor(unsigned long nowMillis) {
  const uint8_t colorT = rgb_triangleU8(nowMillis, RGB_ERR_COLOR_MS);
  return {rgb_lerpU8(RGB_ERR_A_R, RGB_ERR_B_R, colorT), rgb_lerpU8(RGB_ERR_A_G, RGB_ERR_B_G, colorT),
          rgb_lerpU8(RGB_ERR_A_B, RGB_ERR_B_B, colorT)};
}

static void rgb_renderOk(unsigned long nowMillis) {
  const RgbColor base = {0, RGB_OK_G, 0};
  if (rgb_isDisplayAwake()) {
    rgb_renderSteady(base);
  } else {
    rgb_renderBreathing(nowMillis, rgb_envelopeForMode(RGB_MODE_OK), base);
  }
}

static void rgb_renderWifi(unsigned long nowMillis) {
  const RgbColor base = {0, 0, RGB_WIFI_B};
  if (rgb_isDisplayAwake()) {
    rgb_renderSteady(base);
  } else {
    rgb_renderBreathing(nowMillis, rgb_envelopeForMode(RGB_MODE_WIFI), base);
  }
}

static void rgb_renderError(unsigned long nowMillis) {
  const RgbColor base = rgb_errorColor(nowMillis);
  if (rgb_isDisplayAwake()) {
    rgb_renderSteady(base);
  } else {
    rgb_renderBreathing(nowMillis, rgb_envelopeForMode(RGB_MODE_ERROR), base);
  }
}

static void rgb_renderMode(RgbStatusMode mode, unsigned long nowMillis) {
  if (mode == RGB_MODE_ERROR) {
    rgb_renderError(nowMillis);
  } else if (mode == RGB_MODE_WIFI) {
    rgb_renderWifi(nowMillis);
  } else {
    rgb_renderOk(nowMillis);
  }
}

void rgb_status_setBrightnessPreview(uint8_t raw, bool active) {
  rgb_brightnessPreviewActive = active;
  rgb_brightnessPreviewRaw = raw;
}

void rgb_status_applySettings() {
#if RGB_STATUS_HW
  if (!hwReady) {
    return;
  }
  if (!settings.rgbLedEnabled) {
    rgb_hwOff();
    return;
  }
  rgb_lastPollMs = 0;
  rgb_status_poll(millis());
#else
  (void)0;
#endif
}

void rgb_status_begin() {
#if RGB_STATUS_HW
  rgb_hwInit();
  currentMode = RGB_MODE_OK;
  rgb_pendingMode = RGB_MODE_OK;
  rgb_modePendingSinceMs = millis();
  rgb_lastPollMs = 0;
  rgb_status_applySettings();
#else
  currentMode = RGB_MODE_OFF;
#endif
}

RgbStatusMode rgb_status_getMode() {
  return currentMode;
}

void rgb_status_poll(unsigned long nowMillis) {
#if RGB_STATUS_HW
  if (!hwReady) {
    return;
  }
  if (!settings.rgbLedEnabled) {
    rgb_hwOff();
    return;
  }
  if (nowMillis - rgb_lastPollMs < RGB_POLL_MS) {
    return;
  }
  rgb_lastPollMs = nowMillis;

  RgbStatusMode target = rgb_resolveMode();
  rgb_applyModeDebounce(target, nowMillis);
  rgb_renderMode(currentMode, nowMillis);
#else
  (void)nowMillis;
#endif
}
