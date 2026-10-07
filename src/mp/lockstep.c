#include "lockstep.h"

#include "core/buffer.h"
#include "core/dir.h"
#include "core/io.h"
#include "core/log.h"
#include "game/file.h"
#include "game/player_context.h"
#include "game/rules.h"
#include "game/time.h"
#include "mp/checksum.h"
#include "mp/command.h"
#include "mp/compose.h"
#include "mp/discovery.h"
#include "mp/mapgen.h"
#include "mp/permissions.h"
#include "mp/savegame.h"
#include "mp/session.h"
#include "scenario/property.h"
#include "platform/net.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROTOCOL_VERSION 18 // 3: the rules of the game travel with the welcome message; 4: territories; 5: fog;
                            // 6: one forest map, games alone (D-044); 7: missions at the start (D-045);
                            // 8: a caravan per resource, the empire the dearer source (D-048);
                            // 9: wide places in messages, the missionary goes to the nearest walkable tile;
                            // 10: laurels and wrath of Caesar in the state (M9.1); 11: no pond on the maps (D-055);
                            // 12: the score of Caesar in the rules, monthly laurels of the notes (M9.5);
                            // 13: the rules of the host shown in the lobby of the players who join (T4.11);
                            // 14: two players inland on the map for 4, food of the plan, build commands checked
                            // against the permissions of the city (T4.15, D-065);
                            // 15: the price of Rome and the portorium, the empire always sells (D-060);
                            // 16: the prepared map in the rules, a second map for 2 and for 4 players (T4.14);
                            // 17: stock limits of the trade, a command and a piece of the state (T4.5, D-070);
                            // 18: a monthly history of the laurels of each city (T4.1, D-071)
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
    MSG_DESYNC = 6,
    MSG_PAUSE_REQUEST = 7, // client to host: pause or resume the game
    MSG_PAUSED = 8,        // host to clients: the game is paused (1) or running (0)
    MSG_REJECT = 9,        // host to a client: refused (reason), the connection closes
    MSG_RULES = 10         // host to clients, before the start: the rules chosen in the lobby, at every change
};

enum {
    REJECT_PROTOCOL = 1,
    REJECT_GAME_DATA = 2
};

typedef struct {
    uint8_t *data;
    int size;
    int capacity;
} receive_buffer;

// rules chosen before hosting (lobby), kept across the reset of a new game
static game_rules_settings host_rules;
static int has_host_rules;

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
    game_rules_settings rules; // host: rules of the game, sent to the clients
    game_rules_settings lobby_rules; // client: the rules of the host, shown in the lobby (MSG_RULES)
    int has_lobby_rules;
    int paused;                // the host issues no turn while paused: every computer stops at the same tick
    int dropped[MP_LOCKSTEP_MAX_PLAYERS]; // host: players who left a running game, no longer waited for
    int tick_limit;            // tests: no tick runs from this absolute tick on (0: no limit)
    int accepted[MP_LOCKSTEP_MAX_PLAYERS]; // host: the client said hello with the same protocol and game data
    int manual_start;          // host: the game starts when the host asks (lobby), not as soon as all are there
    int generate_map;          // host: a generated map (mp/mapgen) using the chosen map as template
    int file_territories;      // host: territories of the multiplayer map or game chosen (0 for other files)
    int rules_from_save;       // host: a multiplayer game goes on with its saved rules, the lobby cannot change them
    unsigned int map_seed;
    int start_requested;
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

static void drop_player(int player);

static void send_to_clients(const uint8_t *payload, int size)
{
    for (int p = 1; p < data.num_players; p++) {
        if (data.sockets[p] != NET_INVALID_SOCKET && !send_message(data.sockets[p], payload, size)) {
            drop_player(p);
        }
    }
}

// Host: a player left a running game; the others go on, and the city of that player lives on without orders
static void drop_player(int player)
{
    net_close(data.sockets[player]);
    data.sockets[player] = NET_INVALID_SOCKET;
    if (data.dropped[player]) {
        return;
    }
    data.dropped[player] = 1;
    char text[128];
    snprintf(text, sizeof(text), "Le joueur %d s'est déconnecté : sa cité continue sans lui", player + 1);
    set_status(text);
}

static void set_paused(int paused)
{
    data.paused = paused;
    set_status(paused ? "Partie en pause" : "Partie en cours");
    if (data.is_host) {
        uint8_t payload[2] = { MSG_PAUSED, (uint8_t) paused };
        send_to_clients(payload, 2);
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

// ---------- rules shown in the lobby ----------

static void write_rules(buffer *buf, const game_rules_settings *rules)
{
    buffer_write_i32(buf, rules->difficulty);
    buffer_write_i32(buf, rules->gods_enabled);
    buffer_write_i32(buf, rules->fix_immigration_bug);
    buffer_write_i32(buf, rules->fix_100_year_ghosts);
    buffer_write_i32(buf, rules->ai_invasions);
    buffer_write_i32(buf, rules->end_condition);
    buffer_write_i32(buf, rules->score_years);
    buffer_write_i32(buf, rules->territories);
    buffer_write_i32(buf, rules->fog_of_war);
    buffer_write_i32(buf, rules->caesar_score);
    buffer_write_i32(buf, rules->prepared_map);
}

// Client: the numbers come from the network, the lobby draws them (a translation per difficulty): rules out of
// their bounds are refused
static int read_rules(buffer *buf, game_rules_settings *rules)
{
    rules->difficulty = buffer_read_i32(buf);
    rules->gods_enabled = buffer_read_i32(buf);
    rules->fix_immigration_bug = buffer_read_i32(buf);
    rules->fix_100_year_ghosts = buffer_read_i32(buf);
    rules->ai_invasions = buffer_read_i32(buf);
    rules->end_condition = buffer_read_i32(buf);
    rules->score_years = buffer_read_i32(buf);
    rules->territories = buffer_read_i32(buf);
    rules->fog_of_war = buffer_read_i32(buf);
    rules->caesar_score = buffer_read_i32(buf);
    rules->prepared_map = buffer_read_i32(buf);
    return !buf->overflow && game_rules_settings_valid(rules);
}

// Host: the rules of the game to come, for the lobby of a player (they travel again with the welcome message)
static void send_rules(int player)
{
    uint8_t payload[1 + 11 * 4];
    buffer buf;
    buffer_init(&buf, payload, sizeof(payload));
    buffer_write_u8(&buf, MSG_RULES);
    write_rules(&buf, &data.rules);
    if (data.sockets[player] != NET_INVALID_SOCKET && !send_message(data.sockets[player], payload, buf.index)) {
        net_close(data.sockets[player]); // the place is free again (mp_lockstep_poll)
        data.sockets[player] = NET_INVALID_SOCKET;
        data.accepted[player] = 0;
    }
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
        if (data.dropped[p]) {
            continue;
        }
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

static void start_session(int player, int base_tick, const game_rules_settings *rules)
{
    game_rules_set_multiplayer(rules);
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
static int is_scenario(const char *filename)
{
    size_t length = strlen(filename);
    return length > 4 && (strcmp(filename + length - 4, ".map") == 0 || strcmp(filename + length - 4, ".MAP") == 0);
}

// .mpsav: a multiplayer game going on; .mpmap: a multiplayer map, the same format at the start of a game
static int is_multiplayer_save(const char *filename)
{
    size_t length = strlen(filename);
    return (length > 6 && strcmp(filename + length - 6, ".mpsav") == 0) ||
        (length > 6 && strcmp(filename + length - 6, ".mpmap") == 0);
}

// .mpsav: a multiplayer game going on, which keeps its saved rules (D-073)
static int is_game_going_on(const char *filename)
{
    size_t length = strlen(filename);
    return length > 6 && strcmp(filename + length - 6, ".mpsav") == 0;
}

static int host_generate_map(void)
{
    // the prepared map for this number of players (D-033), the one chosen in the lobby or drawn by lot with its seed
    // (T4.14, D-069); the rules of the game keep the map played, which the welcome message brings to the players.
    // The seed of the lobby also draws the arrival points
    int map = mp_mapgen_choose_prepared_map(data.rules.prepared_map, data.map_seed);
    data.rules.prepared_map = map;
    if (!mp_mapgen_create_prepared_map(data.saved_game, data.num_players, map, data.map_seed)) {
        return 0;
    }
    snprintf(data.saved_game, sizeof(data.saved_game), "mp-session-%d-p0.mpsav", data.port);
    return mp_savegame_write(data.saved_game) && mp_savegame_read(data.saved_game);
}

static int host_compose_cities(void)
{
    if (data.generate_map) {
        return host_generate_map();
    }
    if (is_multiplayer_save(data.saved_game)) {
        // a multiplayer game goes on: its cities are already there
        return mp_savegame_read(data.saved_game) && player_context_num_players() == data.num_players;
    }
    // a map of the free game (.map) starts a new city; a saved game (.sav) goes on with its city
    int loaded;
    if (is_scenario(data.saved_game)) {
        scenario_set_custom(2); // a map of the free game, not a mission of the campaign
        loaded = game_file_start_scenario(data.saved_game);
    } else {
        loaded = game_file_load_saved_game(data.saved_game);
    }
    if (!loaded ||
        !mp_compose_separate_cities(data.num_players, MP_COMPOSE_CITY_GAP)) {
        return 0;
    }
    // players may build between their cities and join them (D-018)
    mp_compose_open_land_between_cities(MP_COMPOSE_CITY_GAP);
    // each arrival point may exploit different resources (D-020)
    mp_permissions_share_out();
    snprintf(data.saved_game, sizeof(data.saved_game), "mp-session-%d-p0.mpsav", data.port);
    return mp_savegame_write(data.saved_game) && mp_savegame_read(data.saved_game);
}

static void host_start_game(void)
{
    if (is_multiplayer_save(data.saved_game)) {
        data.separate_cities = 1;
    }
    if (data.separate_cities && !host_compose_cities()) {
        set_status(data.generate_map && mp_mapgen_lacks_trade_routes() ?
            "La carte choisie ne commerce pas par terre et par mer : choisissez-en une autre" :
            "Impossible de composer les cités des joueurs");
        data.state = MP_LOCKSTEP_DISCONNECTED;
        return;
    }
    if (data.rules_from_save) {
        data.rules = *game_rules_multiplayer_settings(); // as loaded with the game (D-073)
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
    start_session(0, game_time_absolute_tick(), &data.rules);
    uint64_t checksum = mp_checksum_state();

    int size = 128 + save_size;
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
        game_rules_save_state(&buf);
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
    int num_players = buffer_read_i32(buf);
    int base_tick = buffer_read_i32(buf);
    data.separate_cities = buffer_read_u8(buf);
    uint64_t checksum = buffer_read_u32(buf);
    checksum |= ((uint64_t) buffer_read_u32(buf)) << 32;
    game_rules_settings rules; // the rules of the host
    int mode = game_rules_read_state(buf, &rules);
    int save_size = buffer_read_i32(buf);
    // no number of the network is trusted: the player index and the number of players bound the arrays
    if (buf->overflow || mode != GAME_MODE_MULTIPLAYER || !game_rules_settings_valid(&rules) ||
        num_players < 2 || num_players > MP_LOCKSTEP_MAX_PLAYERS || player < 0 || player >= num_players ||
        save_size <= 0 || save_size > buf->size - buf->index) {
        set_status("Message de l'hôte invalide");
        data.state = MP_LOCKSTEP_DISCONNECTED;
        return;
    }
    data.num_players = num_players;
    game_rules_set_multiplayer(&rules);
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
    start_session(player, base_tick, &rules);
    if (game_time_absolute_tick() != base_tick || mp_checksum_state() != checksum) {
        desync(-1);
        return;
    }
    char text[64];
    snprintf(text, sizeof(text), "Partie rejointe : joueur %d sur %d", player + 1, data.num_players);
    set_status(text);
}

// ---------- receiving ----------

// Game data that changes the simulation: buildings (c3_model.txt) and empires. Players must have the same.
static uint64_t altered_fingerprint; // tests: game data different from the host's

static uint64_t game_data_fingerprint(void)
{
    static const char *FILES[] = { "c3_model.txt", "c3.emp", "c32.emp" };
    static uint64_t fingerprint;
    if (altered_fingerprint) {
        return altered_fingerprint;
    }
    if (fingerprint) {
        return fingerprint;
    }
    uint64_t hash = 0xcbf29ce484222325ULL;
    int capacity = 2 * 1024 * 1024;
    uint8_t *bytes = malloc(capacity);
    for (int f = 0; f < 3 && bytes; f++) {
        int size = io_read_file_into_buffer(FILES[f], NOT_LOCALIZED, bytes, capacity);
        hash = (hash ^ (uint64_t) size) * 0x100000001b3ULL;
        for (int i = 0; i < size; i++) {
            hash = (hash ^ bytes[i]) * 0x100000001b3ULL;
        }
    }
    free(bytes);
    fingerprint = hash ? hash : 1;
    return fingerprint;
}

static void reject(int player, int reason)
{
    uint8_t payload[2] = { MSG_REJECT, (uint8_t) reason };
    send_message(data.sockets[player], payload, 2);
    net_close(data.sockets[player]);
    data.sockets[player] = NET_INVALID_SOCKET;
    data.accepted[player] = 0;
    set_status(reason == REJECT_GAME_DATA ? "Joueur refusé : données du jeu différentes" :
        "Joueur refusé : version du jeu différente");
}

static void handle_message(int from, uint8_t *payload, int size)
{
    buffer buf;
    buffer_init(&buf, payload, size);
    int type = buffer_read_u8(&buf);
    if (data.is_host) {
        if (type == MSG_HELLO && data.state == MP_LOCKSTEP_WAITING_FOR_PLAYERS) {
            int version = buffer_read_i32(&buf);
            uint64_t fingerprint = buffer_read_u32(&buf);
            fingerprint |= ((uint64_t) buffer_read_u32(&buf)) << 32;
            if (version != PROTOCOL_VERSION) {
                reject(from, REJECT_PROTOCOL);
            } else if (fingerprint != game_data_fingerprint()) {
                reject(from, REJECT_GAME_DATA);
            } else {
                data.accepted[from] = 1;
                send_rules(from);
            }
        } else if (type == MSG_COMMAND) {
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
        } else if (type == MSG_PAUSE_REQUEST && data.state == MP_LOCKSTEP_RUNNING) {
            int paused = buffer_read_u8(&buf);
            if (paused != data.paused) {
                set_paused(paused);
            }
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
        } else if (type == MSG_RULES && data.state == MP_LOCKSTEP_WAITING_FOR_PLAYERS) {
            game_rules_settings rules;
            if (read_rules(&buf, &rules)) {
                data.lobby_rules = rules;
                data.has_lobby_rules = 1;
            } else {
                log_error("Multiplayer: rules of the host out of their bounds, ignored", 0, 0);
            }
        } else if (type == MSG_DESYNC) {
            desync(buffer_read_i32(&buf));
        } else if (type == MSG_PAUSED) {
            set_paused(buffer_read_u8(&buf));
        } else if (type == MSG_REJECT) {
            data.state = MP_LOCKSTEP_DISCONNECTED;
            set_status(buffer_read_u8(&buf) == REJECT_GAME_DATA ?
                "Refusé par l'hôte : données du jeu différentes (c3_model.txt, empire)" :
                "Refusé par l'hôte : version du jeu différente");
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
    // a desynchronisation or a refusal already says why the connection ends
    if (closed && data.state != MP_LOCKSTEP_DESYNC && data.state != MP_LOCKSTEP_DISCONNECTED) {
        if (data.is_host && data.state == MP_LOCKSTEP_RUNNING) {
            drop_player(player);
        } else if (data.is_host) {
            set_status("Un joueur est parti avant le lancement"); // its place is free again
        } else {
            data.state = MP_LOCKSTEP_DISCONNECTED;
            set_status("Connexion à l'hôte perdue");
        }
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

void mp_lockstep_test_alter_game_data(void)
{
    altered_fingerprint = game_data_fingerprint() ^ 0x5a5a5a5a5a5a5a5aULL;
}

void mp_lockstep_set_generated_map(int generate, unsigned int seed)
{
    data.generate_map = generate;
    data.map_seed = seed;
    if (generate && data.rules_from_save) {
        // a new map after all: the rules of the lobby
        data.rules_from_save = 0;
        if (has_host_rules) {
            data.rules = host_rules;
        } else {
            game_rules_default_multiplayer_settings(&data.rules);
        }
    }
    // the prepared maps come with territories (D-036), a multiplayer map or game keeps those it was saved with
    data.rules.territories = generate ? 1 : data.file_territories;
}

void mp_lockstep_set_manual_start(int manual)
{
    data.manual_start = manual;
}

void mp_lockstep_start_game(void)
{
    data.start_requested = 1;
}

void mp_lockstep_set_rules(const game_rules_settings *rules)
{
    host_rules = *rules;
    has_host_rules = 1;
    if (data.is_host && data.state == MP_LOCKSTEP_WAITING_FOR_PLAYERS && !data.rules_from_save) {
        // changed after hosting (lobby): the game starts with these, and the players already there see them;
        // the map decides the territories (mp_lockstep_set_generated_map)
        int territories = data.rules.territories;
        data.rules = *rules;
        data.rules.territories = territories;
        for (int p = 1; p < data.num_players; p++) {
            if (data.accepted[p]) {
                send_rules(p);
            }
        }
    }
}

int mp_lockstep_rules_from_saved_game(void)
{
    return data.state != MP_LOCKSTEP_OFF && data.is_host && data.rules_from_save;
}

const game_rules_settings *mp_lockstep_lobby_rules(void)
{
    if (data.state == MP_LOCKSTEP_OFF) {
        return 0;
    }
    if (data.is_host) {
        return &data.rules;
    }
    return data.has_lobby_rules ? &data.lobby_rules : 0;
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
    if (has_host_rules) {
        data.rules = host_rules;
    } else {
        game_rules_default_multiplayer_settings(&data.rules);
    }
    snprintf(data.saved_game, sizeof(data.saved_game), "%s", saved_game);
    game_rules_settings saved;
    if (is_multiplayer_save(saved_game) && mp_savegame_read_rules(saved_game, &saved)) {
        data.file_territories = saved.territories;
        if (is_game_going_on(saved_game)) {
            // a game goes on with its rules: the lobby shows them and cannot change them (D-073)
            data.rules = saved;
            data.rules_from_save = 1;
        } else {
            data.rules.territories = saved.territories; // a multiplayer map: its territories, the lobby the rest
        }
    }
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
    uint8_t payload[32];
    buffer buf;
    buffer_init(&buf, payload, sizeof(payload));
    buffer_write_u8(&buf, MSG_HELLO);
    buffer_write_i32(&buf, PROTOCOL_VERSION);
    uint64_t fingerprint = game_data_fingerprint();
    buffer_write_u32(&buf, (uint32_t) fingerprint);
    buffer_write_u32(&buf, (uint32_t) (fingerprint >> 32));
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
        for (int p = 1; p < data.num_players; p++) {
            if (data.sockets[p] == NET_INVALID_SOCKET) {
                data.accepted[p] = 0;
                data.sockets[p] = net_accept(data.listener);
            }
        }
    }
    int first = data.is_host ? 1 : 0;
    int last = data.is_host ? data.num_players : 1;
    for (int p = first; p < last; p++) {
        if (data.sockets[p] != NET_INVALID_SOCKET) {
            receive_from(p);
        }
    }
    if (data.is_host && data.state == MP_LOCKSTEP_WAITING_FOR_PLAYERS) {
        int connected = mp_lockstep_connected_players();
        if (connected == data.num_players && (!data.manual_start || data.start_requested)) {
            host_start_game();
        } else {
            // players looking for a game on the local network see this one (mp/discovery)
            const char *name = strrchr(data.saved_game, '/');
            mp_discovery_announce(data.port, data.num_players, connected, data.generate_map ?
                MP_DISCOVERY_NEW_GAME : name ? name + 1 : data.saved_game);
        }
    }
}

int mp_lockstep_can_run_tick(void)
{
    if (data.state != MP_LOCKSTEP_RUNNING) {
        return 0;
    }
    if (data.tick_limit && game_time_absolute_tick() >= data.tick_limit) {
        return 0;
    }
    int ticks = game_time_absolute_tick() - data.base_tick;
    int turn = ticks / TURN_TICKS;
    if (data.is_host && ticks % TURN_TICKS == 0 && turn + TURN_DELAY > data.last_known_turn) {
        if (data.paused) {
            return 0;
        }
        // starting a turn: clients must not lag behind, then announce the commands of turn + delay
        for (int p = 1; p < data.num_players; p++) {
            if (!data.dropped[p] && data.done_turn[p] < turn - TURN_DELAY) {
                return 0;
            }
        }
        host_issue_turn(turn + TURN_DELAY);
    }
    return turn <= data.last_known_turn;
}

// Diagnosis of a desynchronisation: MP_TRACE_TURNS=FIRST-LAST logs the checksum of every piece of these turns,
// to compare between the logs of two computers
static void trace_piece(const char *name, uint64_t checksum, void *userdata)
{
    char text[128];
    snprintf(text, sizeof(text), "turn %d %s %016llx", *(int *) userdata, name, (unsigned long long) checksum);
    log_info("Piece checksum:", text, 0);
}

static uint64_t turn_checksum(int turn)
{
    static int first = -1, last = -2;
    if (first == -1) {
        const char *range = getenv("MP_TRACE_TURNS");
        if (!range || sscanf(range, "%d-%d", &first, &last) != 2) {
            first = -2;
        }
    }
    if (turn >= first && turn <= last) {
        return mp_checksum_state_pieces(trace_piece, &turn);
    }
    return mp_checksum_state();
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
    uint64_t checksum = turn_checksum(turn);
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

void mp_lockstep_set_tick_limit(int ticks_in_game)
{
    data.tick_limit = ticks_in_game > 0 ? data.base_tick + ticks_in_game : 0;
}

int mp_lockstep_base_tick(void)
{
    return data.base_tick;
}

int mp_lockstep_ticks_available(void)
{
    if (data.state != MP_LOCKSTEP_RUNNING) {
        return 0;
    }
    int ticks = game_time_absolute_tick() - data.base_tick;
    int available = (data.last_known_turn + 1) * TURN_TICKS - ticks;
    return available > 0 ? available : 0;
}

void mp_lockstep_request_pause(int paused)
{
    if (data.state != MP_LOCKSTEP_RUNNING) {
        return;
    }
    if (data.is_host) {
        if (paused != data.paused) {
            set_paused(paused);
        }
    } else {
        uint8_t payload[2] = { MSG_PAUSE_REQUEST, (uint8_t) paused };
        if (!send_message(data.sockets[0], payload, 2)) {
            data.state = MP_LOCKSTEP_DISCONNECTED;
            set_status("Connexion à l'hôte perdue");
        }
    }
}

void mp_lockstep_toggle_pause(void)
{
    mp_lockstep_request_pause(!data.paused);
}

int mp_lockstep_is_paused(void)
{
    return data.paused;
}

int mp_lockstep_is_host(void)
{
    return data.is_host;
}

int mp_lockstep_connected_players(void)
{
    if (!data.is_host) {
        return data.state == MP_LOCKSTEP_OFF ? 0 : data.num_players;
    }
    int connected = 1;
    for (int p = 1; p < data.num_players; p++) {
        connected += data.sockets[p] != NET_INVALID_SOCKET && (data.accepted[p] || data.state != MP_LOCKSTEP_WAITING_FOR_PLAYERS);
    }
    return connected;
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
