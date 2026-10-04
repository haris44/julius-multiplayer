#include "lockstep.h"

#include "core/buffer.h"
#include "core/log.h"
#include "game/file.h"
#include "game/player_context.h"
#include "game/rules.h"
#include "game/time.h"
#include "mp/checksum.h"
#include "mp/command.h"
#include "mp/compose.h"
#include "mp/savegame.h"
#include "mp/session.h"
#include "platform/net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROTOCOL_VERSION 2
#define TURN_TICKS 4
#define TURN_DELAY 2
#define HISTORY 256
#define MAX_PENDING 256
#define MAX_SAVE_SIZE (32 * 1024 * 1024) // a multiplayer game of 4 cities is about 10 MB
#define MAX_MESSAGE (MAX_SAVE_SIZE + 64)

enum {
    MSG_HELLO = 1,
    MSG_WELCOME = 2,
    MSG_COMMAND = 3,
    MSG_TURN = 4,
    MSG_DONE = 5,
    MSG_DESYNC = 6
};

typedef struct {
    uint8_t *data;
    int size;
    int capacity;
} receive_buffer;

static struct {
    mp_lockstep_state state;
    int is_host;
    int listener;
    int sockets[MP_LOCKSTEP_MAX_PLAYERS]; // host: one per client player id; client: [0] = host
    receive_buffer received[MP_LOCKSTEP_MAX_PLAYERS];
    int num_players;
    int port;
    int local_player;
    char saved_game[512];
    int separate_cities;   // one city per player (a .mpsav is sent) instead of one shared city
    int base_tick;
    int last_known_turn;   // commands of all turns up to this one are known
    int done_turn[MP_LOCKSTEP_MAX_PLAYERS];
    struct {
        int turn;
        uint64_t checksum;
    } checksums[MP_LOCKSTEP_MAX_PLAYERS][HISTORY];
    int last_verified_turn;
    mp_command pending[MAX_PENDING];
    int num_pending;
    void (*started_callback)(void);
    char status[128];
} data;

static void set_status(const char *text)
{
    snprintf(data.status, sizeof(data.status), "%s", text);
    log_info("Multiplayer:", data.status, 0);
}

// ---------- messages ----------

static int send_message(int socket, const uint8_t *payload, int size)
{
    uint8_t header[4];
    buffer buf;
    buffer_init(&buf, header, 4);
    buffer_write_i32(&buf, size);
    return net_send(socket, header, 4) && net_send(socket, payload, size);
}

static void send_to_clients(const uint8_t *payload, int size)
{
    for (int p = 1; p < data.num_players; p++) {
        if (data.sockets[p] != NET_INVALID_SOCKET && !send_message(data.sockets[p], payload, size)) {
            data.state = MP_LOCKSTEP_DISCONNECTED;
            set_status("Joueur déconnecté");
        }
    }
}

static int read_file(const char *filename, uint8_t **bytes)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        return 0;
    }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size <= 0 || size > MAX_SAVE_SIZE) {
        fclose(fp);
        return 0;
    }
    *bytes = malloc(size);
    if (!*bytes || fread(*bytes, 1, size, fp) != (size_t) size) {
        free(*bytes);
        fclose(fp);
        return 0;
    }
    fclose(fp);
    return (int) size;
}

// ---------- checksums ----------

static void store_checksum(int player, int turn, uint64_t checksum)
{
    data.checksums[player][turn % HISTORY].turn = turn;
    data.checksums[player][turn % HISTORY].checksum = checksum;
}

static void desync(int turn)
{
    if (data.state == MP_LOCKSTEP_DESYNC) {
        return;
    }
    data.state = MP_LOCKSTEP_DESYNC;
    // the clients are told first: writing the diagnostic save takes a while
    if (data.is_host) {
        uint8_t payload[8];
        buffer buf;
        buffer_init(&buf, payload, sizeof(payload));
        buffer_write_u8(&buf, MSG_DESYNC);
        buffer_write_i32(&buf, turn);
        send_to_clients(payload, buf.index);
    }
    char filename[64];
    int multiplayer_save = mp_savegame_is_needed();
    snprintf(filename, sizeof(filename), "mp-desync-%d-p%d-turn%d.%s", data.port, data.local_player, turn,
        multiplayer_save ? "mpsav" : "sav");
    if (multiplayer_save) {
        mp_savegame_write(filename);
    } else {
        game_file_write_saved_game(filename);
    }
    char text[128];
    snprintf(text, sizeof(text), "Désynchronisation au tour %d (état écrit dans %s)", turn, filename);
    set_status(text);
}

// Host: compares every client checksum known for this turn with its own
static void verify_turn(int turn)
{
    if (data.checksums[0][turn % HISTORY].turn != turn) {
        return;
    }
    uint64_t own = data.checksums[0][turn % HISTORY].checksum;
    for (int p = 1; p < data.num_players; p++) {
        if (data.checksums[p][turn % HISTORY].turn != turn) {
            return;
        }
        if (data.checksums[p][turn % HISTORY].checksum != own) {
            desync(turn);
            return;
        }
    }
    if (turn > data.last_verified_turn) {
        data.last_verified_turn = turn;
    }
}

// ---------- commands ----------

static void host_sink(mp_command *command)
{
    if (data.num_pending < MAX_PENDING) {
        data.pending[data.num_pending++] = *command;
    }
}

static void client_sink(mp_command *command)
{
    uint8_t payload[1 + MP_COMMAND_SERIALIZED_SIZE];
    buffer buf;
    buffer_init(&buf, payload, sizeof(payload));
    buffer_write_u8(&buf, MSG_COMMAND);
    mp_command_write(command, &buf);
    if (!send_message(data.sockets[0], payload, buf.index)) {
        data.state = MP_LOCKSTEP_DISCONNECTED;
        set_status("Connexion à l'hôte perdue");
    }
}

// Host: the commands received so far run at the start of the given turn, everywhere
static void host_issue_turn(int turn)
{
    int size = 1 + 8 + data.num_pending * MP_COMMAND_SERIALIZED_SIZE;
    uint8_t *payload = malloc(size);
    if (!payload) {
        return;
    }
    buffer buf;
    buffer_init(&buf, payload, size);
    buffer_write_u8(&buf, MSG_TURN);
    buffer_write_i32(&buf, turn);
    buffer_write_i32(&buf, data.num_pending);
    for (int i = 0; i < data.num_pending; i++) {
        data.pending[i].tick = data.base_tick + turn * TURN_TICKS;
        mp_command_write(&data.pending[i], &buf);
        mp_command_queue_add(&data.pending[i]);
    }
    data.num_pending = 0;
    send_to_clients(payload, buf.index);
    free(payload);
    data.last_known_turn = turn;
}

// ---------- game start ----------

static void start_session(int player, int base_tick)
{
    game_rules_settings rules;
    game_rules_default_multiplayer_settings(&rules);
    game_rules_set_multiplayer(&rules);
    data.local_player = player;
    data.base_tick = base_tick;
    data.last_known_turn = TURN_DELAY - 1; // the first turns have no commands
    data.last_verified_turn = -1;
    for (int p = 0; p < MP_LOCKSTEP_MAX_PLAYERS; p++) {
        data.done_turn[p] = -1;
        for (int i = 0; i < HISTORY; i++) {
            data.checksums[p][i].turn = -1;
        }
    }
    mp_session_init_network(player, data.is_host ? host_sink : client_sink);
    data.state = MP_LOCKSTEP_RUNNING;
    if (data.started_callback) {
        data.started_callback();
    }
}

// Host: the starting game of separate cities, written to a .mpsav and loaded back as the clients will
static int host_compose_cities(void)
{
    if (!game_file_load_saved_game(data.saved_game) ||
        !mp_compose_separate_cities(data.num_players, MP_COMPOSE_CITY_GAP)) {
        return 0;
    }
    snprintf(data.saved_game, sizeof(data.saved_game), "mp-session-%d-p0.mpsav", data.port);
    return mp_savegame_write(data.saved_game) && mp_savegame_read(data.saved_game);
}

static void host_start_game(void)
{
    if (data.separate_cities && !host_compose_cities()) {
        set_status("Impossible de composer les cités des joueurs");
        data.state = MP_LOCKSTEP_DISCONNECTED;
        return;
    }
    uint8_t *save;
    int save_size = read_file(data.saved_game, &save);
    if (!save_size || (!data.separate_cities && !game_file_load_saved_game(data.saved_game))) {
        set_status("Impossible de charger la sauvegarde de départ");
        data.state = MP_LOCKSTEP_DISCONNECTED;
        return;
    }
    // rules must be set before the checksum: clients compare after doing the same
    data.is_host = 1;
    start_session(0, game_time_absolute_tick());
    uint64_t checksum = mp_checksum_state();

    int size = 64 + save_size;
    uint8_t *payload = malloc(size);
    for (int p = 1; p < data.num_players && payload; p++) {
        buffer buf;
        buffer_init(&buf, payload, size);
        buffer_write_u8(&buf, MSG_WELCOME);
        buffer_write_i32(&buf, p);
        buffer_write_i32(&buf, data.num_players);
        buffer_write_i32(&buf, data.base_tick);
        buffer_write_u8(&buf, data.separate_cities);
        buffer_write_u32(&buf, (uint32_t) checksum);
        buffer_write_u32(&buf, (uint32_t) (checksum >> 32));
        buffer_write_i32(&buf, save_size);
        buffer_write_raw(&buf, save, save_size);
        if (!send_message(data.sockets[p], payload, buf.index)) {
            data.state = MP_LOCKSTEP_DISCONNECTED;
        }
    }
    free(payload);
    free(save);
    set_status("Partie lancée");
}

static void client_welcome(buffer *buf)
{
    int player = buffer_read_i32(buf);
    data.num_players = buffer_read_i32(buf);
    int base_tick = buffer_read_i32(buf);
    data.separate_cities = buffer_read_u8(buf);
    uint64_t checksum = buffer_read_u32(buf);
    checksum |= ((uint64_t) buffer_read_u32(buf)) << 32;
    int save_size = buffer_read_i32(buf);
    if (save_size <= 0 || save_size > buf->size - buf->index) {
        set_status("Message de l'hôte invalide");
        data.state = MP_LOCKSTEP_DISCONNECTED;
        return;
    }
    // the port keeps the files of several games on one computer apart (tests)
    snprintf(data.saved_game, sizeof(data.saved_game), "mp-session-%d-p%d.%s", data.port, player,
        data.separate_cities ? "mpsav" : "sav");
    FILE *fp = fopen(data.saved_game, "wb");
    if (!fp || fwrite(&buf->data[buf->index], 1, save_size, fp) != (size_t) save_size) {
        if (fp) {
            fclose(fp);
        }
        set_status("Impossible d'écrire la sauvegarde reçue");
        data.state = MP_LOCKSTEP_DISCONNECTED;
        return;
    }
    fclose(fp);
    int loaded = data.separate_cities ? mp_savegame_read(data.saved_game) : game_file_load_saved_game(data.saved_game);
    if (!loaded || (data.separate_cities && player >= player_context_num_players())) {
        set_status("Impossible de charger la sauvegarde reçue");
        data.state = MP_LOCKSTEP_DISCONNECTED;
        return;
    }
    // the interface shows the city of this player
    player_context_switch(data.separate_cities ? player : 0);
    start_session(player, base_tick);
    if (game_time_absolute_tick() != base_tick || mp_checksum_state() != checksum) {
        desync(-1);
        return;
    }
    char text[64];
    snprintf(text, sizeof(text), "Partie rejointe : joueur %d sur %d", player + 1, data.num_players);
    set_status(text);
}

// ---------- receiving ----------

static void handle_message(int from, uint8_t *payload, int size)
{
    buffer buf;
    buffer_init(&buf, payload, size);
    int type = buffer_read_u8(&buf);
    if (data.is_host) {
        if (type == MSG_COMMAND) {
            mp_command command;
            if (mp_command_read(&command, &buf) && data.num_pending < MAX_PENDING) {
                command.player_id = from; // never trust the sender about who it is
                data.pending[data.num_pending++] = command;
            }
        } else if (type == MSG_DONE) {
            int turn = buffer_read_i32(&buf);
            uint64_t checksum = buffer_read_u32(&buf);
            checksum |= ((uint64_t) buffer_read_u32(&buf)) << 32;
            store_checksum(from, turn, checksum);
            if (turn > data.done_turn[from]) {
                data.done_turn[from] = turn;
            }
            verify_turn(turn);
        }
    } else {
        if (type == MSG_WELCOME && data.state == MP_LOCKSTEP_WAITING_FOR_PLAYERS) {
            client_welcome(&buf);
        } else if (type == MSG_TURN) {
            int turn = buffer_read_i32(&buf);
            int count = buffer_read_i32(&buf);
            for (int i = 0; i < count; i++) {
                mp_command command;
                if (mp_command_read(&command, &buf)) {
                    mp_command_queue_add(&command);
                }
            }
            data.last_known_turn = turn;
        } else if (type == MSG_DESYNC) {
            desync(buffer_read_i32(&buf));
        }
    }
}

static void receive_from(int player)
{
    receive_buffer *rb = &data.received[player];
    int closed = 0;
    for (;;) {
        if (rb->capacity - rb->size < 65536) {
            int capacity = rb->capacity ? rb->capacity * 2 : 262144;
            uint8_t *bigger = realloc(rb->data, capacity);
            if (!bigger) {
                return;
            }
            rb->data = bigger;
            rb->capacity = capacity;
        }
        int received = net_receive(data.sockets[player], rb->data + rb->size, rb->capacity - rb->size);
        if (received < 0) {
            // the messages received before the connection closed are handled first
            net_close(data.sockets[player]);
            data.sockets[player] = NET_INVALID_SOCKET;
            closed = 1;
            break;
        }
        if (received == 0) {
            break;
        }
        rb->size += received;
    }
    int offset = 0;
    while (rb->size - offset >= 4) {
        buffer header;
        buffer_init(&header, rb->data + offset, 4);
        int length = buffer_read_i32(&header);
        if (length <= 0 || length > MAX_MESSAGE) {
            data.state = MP_LOCKSTEP_DISCONNECTED;
            set_status("Message réseau invalide");
            return;
        }
        if (rb->size - offset - 4 < length) {
            break;
        }
        handle_message(player, rb->data + offset + 4, length);
        offset += 4 + length;
    }
    memmove(rb->data, rb->data + offset, rb->size - offset);
    rb->size -= offset;
    if (closed && data.state != MP_LOCKSTEP_DESYNC) {
        data.state = MP_LOCKSTEP_DISCONNECTED;
        set_status(data.is_host ? "Un joueur s'est déconnecté" : "Connexion à l'hôte perdue");
    }
}

// ---------- public ----------

static void reset(void)
{
    mp_lockstep_stop();
    memset(&data, 0, sizeof(data));
    data.listener = NET_INVALID_SOCKET;
    for (int p = 0; p < MP_LOCKSTEP_MAX_PLAYERS; p++) {
        data.sockets[p] = NET_INVALID_SOCKET;
    }
}

int mp_lockstep_host(int port, int num_players, const char *saved_game, int separate_cities)
{
    void (*callback)(void) = data.started_callback;
    reset();
    data.started_callback = callback;
    if (num_players < 1 || num_players > MP_LOCKSTEP_MAX_PLAYERS) {
        return 0;
    }
    data.is_host = 1;
    data.num_players = num_players;
    data.port = port;
    data.separate_cities = separate_cities;
    snprintf(data.saved_game, sizeof(data.saved_game), "%s", saved_game);
    if (num_players > 1) {
        data.listener = net_listen(port);
        if (data.listener == NET_INVALID_SOCKET) {
            set_status("Impossible d'ouvrir le port réseau");
            return 0;
        }
    }
    data.state = MP_LOCKSTEP_WAITING_FOR_PLAYERS;
    char text[64];
    snprintf(text, sizeof(text), "En attente de %d joueur(s) sur le port %d", num_players - 1, port);
    set_status(text);
    return 1;
}

int mp_lockstep_join(const char *address, int port)
{
    void (*callback)(void) = data.started_callback;
    reset();
    data.started_callback = callback;
    data.port = port;
    // the host may not be listening yet: try for a few seconds
    for (int attempt = 0; attempt < 20; attempt++) {
        data.sockets[0] = net_connect(address, port, 2000);
        if (data.sockets[0] != NET_INVALID_SOCKET) {
            break;
        }
        net_sleep(250);
    }
    if (data.sockets[0] == NET_INVALID_SOCKET) {
        set_status("Impossible de joindre l'hôte");
        return 0;
    }
    uint8_t payload[8];
    buffer buf;
    buffer_init(&buf, payload, sizeof(payload));
    buffer_write_u8(&buf, MSG_HELLO);
    buffer_write_i32(&buf, PROTOCOL_VERSION);
    send_message(data.sockets[0], payload, buf.index);
    data.state = MP_LOCKSTEP_WAITING_FOR_PLAYERS;
    set_status("Connecté, en attente du lancement par l'hôte");
    return 1;
}

void mp_lockstep_set_started_callback(void (*callback)(void))
{
    data.started_callback = callback;
}

void mp_lockstep_poll(void)
{
    if (data.state == MP_LOCKSTEP_OFF) {
        return;
    }
    if (data.is_host && data.state == MP_LOCKSTEP_WAITING_FOR_PLAYERS) {
        int connected = 1;
        for (int p = 1; p < data.num_players; p++) {
            if (data.sockets[p] == NET_INVALID_SOCKET) {
                data.sockets[p] = net_accept(data.listener);
            }
            connected += data.sockets[p] != NET_INVALID_SOCKET;
        }
        if (connected == data.num_players) {
            host_start_game();
        }
    }
    int first = data.is_host ? 1 : 0;
    int last = data.is_host ? data.num_players : 1;
    for (int p = first; p < last; p++) {
        if (data.sockets[p] != NET_INVALID_SOCKET) {
            receive_from(p);
        }
    }
}

int mp_lockstep_can_run_tick(void)
{
    if (data.state != MP_LOCKSTEP_RUNNING) {
        return 0;
    }
    int ticks = game_time_absolute_tick() - data.base_tick;
    int turn = ticks / TURN_TICKS;
    if (data.is_host && ticks % TURN_TICKS == 0 && turn + TURN_DELAY > data.last_known_turn) {
        // starting a turn: clients must not lag behind, then announce the commands of turn + delay
        for (int p = 1; p < data.num_players; p++) {
            if (data.done_turn[p] < turn - TURN_DELAY) {
                return 0;
            }
        }
        host_issue_turn(turn + TURN_DELAY);
    }
    return turn <= data.last_known_turn;
}

void mp_lockstep_after_tick(void)
{
    if (data.state != MP_LOCKSTEP_RUNNING) {
        return;
    }
    int ticks = game_time_absolute_tick() - data.base_tick;
    if (ticks <= 0 || ticks % TURN_TICKS != 0) {
        return;
    }
    int turn = ticks / TURN_TICKS - 1;
    uint64_t checksum = mp_checksum_state();
    if (data.is_host) {
        store_checksum(0, turn, checksum);
        verify_turn(turn);
        // a client may already have reported later turns
        for (int t = data.last_verified_turn + 1; t <= turn; t++) {
            verify_turn(t);
        }
    } else {
        uint8_t payload[16];
        buffer buf;
        buffer_init(&buf, payload, sizeof(payload));
        buffer_write_u8(&buf, MSG_DONE);
        buffer_write_i32(&buf, turn);
        buffer_write_u32(&buf, (uint32_t) checksum);
        buffer_write_u32(&buf, (uint32_t) (checksum >> 32));
        if (!send_message(data.sockets[0], payload, buf.index)) {
            data.state = MP_LOCKSTEP_DISCONNECTED;
            set_status("Connexion à l'hôte perdue");
        }
    }
}

int mp_lockstep_is_active(void)
{
    return data.state != MP_LOCKSTEP_OFF;
}

mp_lockstep_state mp_lockstep_get_state(void)
{
    return data.state;
}

const char *mp_lockstep_status(void)
{
    return data.status;
}

int mp_lockstep_last_verified_turn(void)
{
    return data.last_verified_turn;
}

void mp_lockstep_stop(void)
{
    if (data.state == MP_LOCKSTEP_OFF) {
        return; // nothing opened (sockets of a never used session are 0, not invalid)
    }
    net_close(data.listener);
    data.listener = NET_INVALID_SOCKET;
    for (int p = 0; p < MP_LOCKSTEP_MAX_PLAYERS; p++) {
        net_close(data.sockets[p]);
        data.sockets[p] = NET_INVALID_SOCKET;
        free(data.received[p].data);
        data.received[p].data = 0;
        data.received[p].size = data.received[p].capacity = 0;
    }
    mp_session_init_offline();
    data.state = MP_LOCKSTEP_OFF;
}
