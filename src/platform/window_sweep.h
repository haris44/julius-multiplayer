#ifndef PLATFORM_WINDOW_SWEEP_H
#define PLATFORM_WINDOW_SWEEP_H

/**
 * Tests (automation "windowsweep", T5.3): opens and draws every window, advisor and building information panel the
 * player can reach during a game, one after the other, without running any tick, and checks after each one that the
 * state of the simulation (its checksum, mp/checksum) did not change. In a network game the interface of one player
 * must never write into the state that all players simulate.
 * @param log called with a message and a value for every window that changed the state, and once at the end
 * @return the number of windows that changed the state
 */
int platform_window_sweep(void (*log)(const char *message, const char *value));

#endif // PLATFORM_WINDOW_SWEEP_H
