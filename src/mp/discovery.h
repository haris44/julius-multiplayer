#ifndef MP_DISCOVERY_H
#define MP_DISCOVERY_H

// announced instead of a file name by a host starting a new game on the prepared map; the lobbies translate it
#define MP_DISCOVERY_NEW_GAME "new-game"

/**
 * @file
 * Finding the games hosted on the local network: the host announces its game every second on a
 * UDP port, the players looking for a game listen to these announcements (doc/mp/ROADMAP.md M5.4).
 * Nothing here touches the simulation.
 */

#define MP_DISCOVERY_PORT 27401
#define MP_DISCOVERY_MAX_GAMES 8

typedef struct {
    char address[16];
    int port;
    int num_players;
    int joined_players;
    char map_name[64];
} mp_discovered_game;

/**
 * Host: announces the game, at most once a second (called on every poll while waiting for players)
 */
void mp_discovery_announce(int port, int num_players, int joined_players, const char *map_name);

/**
 * Starts listening to the announcements; the list starts empty
 */
void mp_discovery_start(void);

void mp_discovery_stop(void);

/**
 * Reads the waiting announcements and forgets the games not heard for 3 seconds
 */
void mp_discovery_poll(void);

int mp_discovery_count(void);

const mp_discovered_game *mp_discovery_get(int index);

#endif // MP_DISCOVERY_H
