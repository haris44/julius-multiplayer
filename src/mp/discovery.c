#include "discovery.h"

#include "core/time.h"
#include "platform/net.h"

#include <stdio.h>
#include <string.h>

#define ANNOUNCE_INTERVAL 1000
#define FORGET_AFTER 3000
#define MAGIC "C3MP-GAME"
#define VERSION 2 // network protocol of mp/lockstep: games of another version are not listed

static struct {
    int announce_socket;
    time_millis last_announce;
    int listen_socket;
    mp_discovered_game games[MP_DISCOVERY_MAX_GAMES];
    time_millis last_seen[MP_DISCOVERY_MAX_GAMES];
    int num_games;
} data = { NET_INVALID_SOCKET, 0, NET_INVALID_SOCKET };

void mp_discovery_announce(int port, int num_players, int joined_players, const char *map_name)
{
    time_millis now = time_get_millis();
    if (data.last_announce && now - data.last_announce < ANNOUNCE_INTERVAL) {
        return;
    }
    data.last_announce = now;
    if (data.announce_socket == NET_INVALID_SOCKET) {
        data.announce_socket = net_udp_open(0);
        if (data.announce_socket == NET_INVALID_SOCKET) {
            return;
        }
    }
    // the address of the host identifies the game: it is received twice on the host's own computer
    char address[16];
    net_local_address(address);
    char message[128];
    int length = snprintf(message, sizeof(message), "%s %d %s %d %d %d %.60s", MAGIC, VERSION, address, port,
        num_players, joined_players, map_name);
    net_udp_broadcast(data.announce_socket, MP_DISCOVERY_PORT, message, length);
}

void mp_discovery_start(void)
{
    mp_discovery_stop();
    data.listen_socket = net_udp_open(MP_DISCOVERY_PORT);
}

void mp_discovery_stop(void)
{
    net_close(data.listen_socket);
    data.listen_socket = NET_INVALID_SOCKET;
    data.num_games = 0;
}

static void remember(const mp_discovered_game *game)
{
    int index = 0;
    while (index < data.num_games && (strcmp(data.games[index].address, game->address) != 0 ||
        data.games[index].port != game->port)) {
        index++;
    }
    if (index == MP_DISCOVERY_MAX_GAMES) {
        return;
    }
    if (index == data.num_games) {
        data.num_games++;
    }
    data.games[index] = *game;
    data.last_seen[index] = time_get_millis();
}

static void forget_silent_games(void)
{
    time_millis now = time_get_millis();
    int kept = 0;
    for (int i = 0; i < data.num_games; i++) {
        if (now - data.last_seen[i] < FORGET_AFTER) {
            data.games[kept] = data.games[i];
            data.last_seen[kept] = data.last_seen[i];
            kept++;
        }
    }
    data.num_games = kept;
}

void mp_discovery_poll(void)
{
    if (data.listen_socket == NET_INVALID_SOCKET) {
        return;
    }
    char message[129];
    char address[16];
    int length;
    while ((length = net_udp_receive(data.listen_socket, message, sizeof(message) - 1, address)) > 0) {
        message[length] = 0;
        mp_discovered_game game;
        memset(&game, 0, sizeof(game));
        char magic[16];
        char host_address[16];
        int version;
        if (sscanf(message, "%15s %d %15s %d %d %d %63[^\n]", magic, &version, host_address, &game.port,
                &game.num_players, &game.joined_players, game.map_name) >= 6 &&
            strcmp(magic, MAGIC) == 0 && version == VERSION) {
            // without a network, the host does not know its address: the sender's one is then loopback
            snprintf(game.address, sizeof(game.address), "%s", strcmp(host_address, "?") ? host_address : address);
            remember(&game);
        }
    }
    forget_silent_games();
}

int mp_discovery_count(void)
{
    return data.num_games;
}

const mp_discovered_game *mp_discovery_get(int index)
{
    return index >= 0 && index < data.num_games ? &data.games[index] : 0;
}
