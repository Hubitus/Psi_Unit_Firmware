/*
 * app_effects.cpp — update display, RGB, and diagnostics after commands
 *
 * app_dispatchApply() wraps dispatch and effects in one call (used from ui_input
 * and wifi_web).
 */

#include "app_effects.h"
#include "display_ui.h"
#include "rgb_status.h"
#include "system_info.h"
#include "app_snapshot.h"
#include "app_state.h"
#include "settings_field.h"

static void app_applySleepFromSettings() {
  const uint8_t sleepMin = settings_fieldGetUInt8(SF_SLEEP_MIN);
  if (sleepMin == 0) {
    sleepTimeout = 0xFFFFFFFF;
  } else {
    sleepTimeout = (unsigned long)sleepMin * 60000UL;
  }
}

void app_applyPersistedSettings() {
  app_applySleepFromSettings();
  ui_applyDisplaySettings();
  rgb_status_applySettings();
}

void app_effects_apply(const AppCommandEffects &effects) {
  if (effects.applyPersistedSettings) {
    app_applyPersistedSettings();
  } else if (effects.applyRgbSettings) {
    rgb_status_applySettings();
  }
  if (effects.refreshSystemInfo) {
    system_info_refresh();
  }
  if (effects.redrawMainIfVisible) {
    ui_requestRedrawIfMainVisible();
  }
  if (effects.markMainDirty) {
    app_markMainScreenDirty();
  }
}

bool app_dispatchApply(AppCommand cmd, const void *payload) {
  const AppCommandResult result = app_dispatch(cmd, payload);
  if (result.ok) {
    app_effects_apply(result.effects);
  }
  return result.ok;
}
