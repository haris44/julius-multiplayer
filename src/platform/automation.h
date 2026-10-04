#ifndef PLATFORM_AUTOMATION_H
#define PLATFORM_AUTOMATION_H

#include "core/time.h"

/**
 * @file
 * Scripted automation for tests and AI-driven development.
 *
 * Replays a script of synthetic inputs, fast-forwards the simulation and saves screenshots,
 * using a virtual clock so that every run is reproducible. Activated with the
 * --automation SCRIPT command line option. See doc/mp/TESTING.md for the script syntax.
 */

/**
 * Loads an automation script. Must be called before the data directory is set,
 * because relative paths in the script are resolved against the current directory.
 * @param script_file Path to the script
 * @return 1 on success, 0 on error
 */
int platform_automation_init(const char *script_file);

/**
 * @return Whether an automation script is running
 */
int platform_automation_is_active(void);

/**
 * Advances the virtual clock and executes the script commands for the current frame.
 * Called once per frame, before the game runs.
 */
void platform_automation_before_frame(void);

/**
 * Executes the commands that need a drawn frame (screenshots).
 * Called once per frame, after the frame has been drawn.
 */
void platform_automation_after_frame(void);

/**
 * @return Virtual time of the current frame
 */
time_millis platform_automation_time(void);

/**
 * @return Process exit code: 0 when the script ran fine, non-zero when a command failed
 */
int platform_automation_exit_code(void);

#endif // PLATFORM_AUTOMATION_H
