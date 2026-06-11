/*
 * config_sim.h — force-included when building the simulator (CMake -include)
 */
#ifndef CONFIG_SIM_H
#define CONFIG_SIM_H

#undef SERIAL_CLI_ENABLE
#define SERIAL_CLI_ENABLE 0

#define SIM_BUILD 1

/* Do not #define SPLASH_DURATION_MS — it is a variable name in config.h; a macro breaks MSVC builds. */

#endif
