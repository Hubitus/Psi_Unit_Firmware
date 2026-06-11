/*
 * app_effects.h — apply side effects after settings changes
 *
 * After app_dispatch, updates display, RGB indicator, sleep timeout, and
 * system_info according to AppCommandEffects flags.
 */

#ifndef APP_EFFECTS_H
#define APP_EFFECTS_H

#include "app_commands.h"

void app_effects_apply(const AppCommandEffects &effects);
void app_applyPersistedSettings();
bool app_dispatchApply(AppCommand cmd, const void *payload);

#endif // APP_EFFECTS_H
