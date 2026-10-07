#include "savegame.h"

#include "building/building.h"
#include "core/buffer.h"
#include "core/log.h"
#include "game/extra_state.h"
#include "mp/caesar.h"
#include "mp/endgame.h"
#include "game/file.h"
#include "game/file_io.h"
#include "game/player_context.h"
#include "game/rules.h"
#include "map/data.h"
#include "map/grid.h"
#include "map/owner.h"
#include "mp/command.h"
#include "mp/fog.h"
#include "mp/territory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAGIC 0x504d3343 // "C3MP"
#define VERSION 3 // 2: rules for AI invasions and the end of the game; 3: wide places in messages
#define MAX_NAME 64

static void visit_buffer(mp_savegame_visitor visitor, void *userdata, const char *name,
    void (*save)(buffer *buf), int capacity)
{
    uint8_t *data = malloc(capacity);
    if (!data) {
        return;
    }
    buffer buf;
    buffer_init(&buf, data, capacity);
    save(&buf);
    visitor(name, data, buf.index, userdata);
    free(data);
}

static void save_header(buffer *buf)
{
    buffer_write_i32(buf, map_grid_stride);
    buffer_write_i32(buf, map_data.width);
    buffer_write_i32(buf, map_data.height);
    buffer_write_i32(buf, map_data.start_offset);
    buffer_write_i32(buf, map_data.border_size);
    buffer_write_i32(buf, player_context_num_players());
    game_rules_save_state(buf);
}

typedef struct {
    mp_savegame_visitor visitor;
    void *userdata;
} forward;

static void forward_piece(const char *name, const unsigned char *data, int size, void *userdata)
{
    forward *f = userdata;
    f->visitor(name, data, size, f->userdata);
}

void mp_savegame_visit(mp_savegame_visitor visitor, void *userdata)
{
    visit_buffer(visitor, userdata, "mp_header", save_header, 256);
    visit_buffer(visitor, userdata, "owner_grid", map_owner_save_state, GRID_MAX_TILES);
    visit_buffer(visitor, userdata, "mp_endgame", mp_endgame_save_state, 64);
    visit_buffer(visitor, userdata, "mp_caesar", mp_caesar_save_state, 1024);
    visit_buffer(visitor, userdata, "caesar_buildings", building_save_caesar_state, MAX_BUILDINGS * 256);
    visit_buffer(visitor, userdata, "territory_grid", mp_territory_save_state, GRID_MAX_TILES);
    visit_buffer(visitor, userdata, "fog_grid", mp_fog_save_state, GRID_MAX_TILES);
    int previous = player_context_current();
    forward f = { visitor, userdata };
    for (int p = 0; p < player_context_num_players(); p++) {
        player_context_switch(p);
        // map grids are shared: written once, with the first city
        game_file_io_visit_wide_state(forward_piece, &f, p == 0);
        visit_buffer(visitor, userdata, "extra_state", game_extra_state_save, game_extra_state_size());
    }
    player_context_switch(previous);
}

int mp_savegame_is_needed(void)
{
    return player_context_num_players() > 1 || map_grid_stride != 162;
}

// ---------- writing ----------

static void write_piece(const char *name, const unsigned char *data, int size, void *userdata)
{
    FILE *fp = userdata;
    uint8_t header[MAX_NAME + 8];
    buffer buf;
    buffer_init(&buf, header, sizeof(header));
    int name_length = (int) strlen(name);
    buffer_write_u8(&buf, name_length);
    buffer_write_raw(&buf, name, name_length);
    buffer_write_i32(&buf, size);
    fwrite(header, 1, buf.index, fp);
    fwrite(data, 1, size, fp);
}

int mp_savegame_write(const char *filename)
{
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        log_error("Unable to write multiplayer saved game", filename, 0);
        return 0;
    }
    uint8_t header[8];
    buffer buf;
    buffer_init(&buf, header, sizeof(header));
    buffer_write_u32(&buf, MAGIC);
    buffer_write_u32(&buf, VERSION);
    fwrite(header, 1, buf.index, fp);
    mp_savegame_visit(write_piece, fp);
    int ok = !ferror(fp);
    fclose(fp);
    return ok;
}

// ---------- reading ----------

typedef struct {
    char name[MAX_NAME + 1];
    int size;
    const uint8_t *data;
} piece;

static struct {
    uint8_t *file;
    int file_size;
    piece *pieces;
    int num_pieces;
    int city_start;   // first piece of the city being loaded
    int city_end;
    int world_start;  // first piece of the first city, which holds the grids
    int world_end;
} reading;

static const piece *find_piece(const char *name, int from, int to)
{
    for (int i = from; i < to; i++) {
        if (strcmp(reading.pieces[i].name, name) == 0) {
            return &reading.pieces[i];
        }
    }
    return 0;
}

static int provide_piece(const char *name, unsigned char *data, int capacity, void *userdata)
{
    const piece *p = find_piece(name, reading.city_start, reading.city_end);
    if (!p) {
        p = find_piece(name, reading.world_start, reading.world_end); // shared grids
    }
    if (!p || p->size > capacity) {
        log_error("Multiplayer saved game: missing or invalid piece", name, 0);
        return 0;
    }
    memcpy(data, p->data, p->size);
    return 1;
}

static int parse(void)
{
    buffer buf;
    buffer_init(&buf, reading.file, reading.file_size);
    if (buffer_read_u32(&buf) != MAGIC || buffer_read_u32(&buf) != VERSION) {
        return 0;
    }
    int capacity = 64;
    reading.pieces = malloc(capacity * sizeof(piece));
    reading.num_pieces = 0;
    while (buf.index < buf.size) {
        if (reading.num_pieces == capacity) {
            capacity *= 2;
            reading.pieces = realloc(reading.pieces, capacity * sizeof(piece));
        }
        piece *p = &reading.pieces[reading.num_pieces];
        int name_length = buffer_read_u8(&buf);
        if (name_length > MAX_NAME) {
            return 0;
        }
        buffer_read_raw(&buf, p->name, name_length);
        p->name[name_length] = 0;
        p->size = buffer_read_i32(&buf);
        if (buf.overflow || p->size < 0 || p->size > buf.size - buf.index) {
            return 0;
        }
        p->data = &reading.file[buf.index];
        buffer_skip(&buf, p->size);
        reading.num_pieces++;
    }
    return 1;
}

static int load_pieces(void)
{
    const piece *header = find_piece("mp_header", 0, reading.num_pieces);
    if (!header) {
        return 0;
    }
    buffer buf;
    buffer_init(&buf, (uint8_t *) header->data, header->size);
    int stride = buffer_read_i32(&buf);
    int width = buffer_read_i32(&buf);
    int height = buffer_read_i32(&buf);
    int start = buffer_read_i32(&buf);
    int border = buffer_read_i32(&buf);
    int num_players = buffer_read_i32(&buf);
    if (stride <= 0 || stride > GRID_MAX_SIZE || num_players < 1 || num_players > PLAYER_CONTEXT_MAX_PLAYERS) {
        return 0;
    }

    // start from the state of a new process, with as many cities as saved
    if (player_context_num_players() > 1) {
        player_context_switch(0);
        player_context_set_num_players(1);
    }
    game_extra_state_reset();
    mp_command_queue_clear();
    map_grid_set_stride(stride);
    map_grid_init(width, height, start, border);
    player_context_set_num_players(num_players);

    // cities: from one "extra_state" piece to the next
    int city = 0;
    int from = 0;
    for (int i = 0; i < reading.num_pieces && city < num_players; i++) {
        if (strcmp(reading.pieces[i].name, "extra_state") != 0) {
            continue;
        }
        reading.city_start = from;
        reading.city_end = i;
        if (city == 0) {
            reading.world_start = from;
            reading.world_end = i;
        }
        player_context_switch(city);
        if (!game_file_io_load_wide_state(provide_piece, 0)) {
            return 0;
        }
        buffer extra;
        buffer_init(&extra, (uint8_t *) reading.pieces[i].data, reading.pieces[i].size);
        game_extra_state_load(&extra);
        from = i + 1;
        city++;
    }
    if (city != num_players) {
        return 0;
    }
    player_context_switch(0);

    // rules and map position: loading the cities set them from their scenario
    buffer_init(&buf, (uint8_t *) header->data, header->size);
    buffer_skip(&buf, 6 * 4);
    game_rules_load_state(&buf);
    map_grid_init(width, height, start, border);
    const piece *endgame = find_piece("mp_endgame", 0, reading.num_pieces);
    if (endgame) {
        buffer endgame_buf;
        buffer_init(&endgame_buf, (uint8_t *) endgame->data, endgame->size);
        mp_endgame_load_state(&endgame_buf);
    } else {
        mp_endgame_reset();
    }
    const piece *judge = find_piece("mp_caesar", 0, reading.num_pieces);
    if (judge) {
        buffer judge_buf;
        buffer_init(&judge_buf, (uint8_t *) judge->data, judge->size);
        mp_caesar_load_state(&judge_buf);
    } else {
        mp_caesar_reset();
    }
    const piece *caesar = find_piece("caesar_buildings", 0, reading.num_pieces);
    if (caesar) {
        buffer caesar_buf;
        buffer_init(&caesar_buf, (uint8_t *) caesar->data, caesar->size);
        building_load_caesar_state(&caesar_buf);
    } else {
        building_clear_caesar_state();
    }
    const piece *zones = find_piece("territory_grid", 0, reading.num_pieces);
    if (zones) {
        buffer zones_buf;
        buffer_init(&zones_buf, (uint8_t *) zones->data, zones->size);
        mp_territory_load_state(&zones_buf);
    } else {
        mp_territory_clear();
    }
    const piece *fog = find_piece("fog_grid", 0, reading.num_pieces);
    if (fog) {
        buffer fog_buf;
        buffer_init(&fog_buf, (uint8_t *) fog->data, fog->size);
        mp_fog_load_state(&fog_buf);
    } else {
        mp_fog_clear();
    }
    const piece *owner = find_piece("owner_grid", 0, reading.num_pieces);
    if (owner) {
        buffer owner_buf;
        buffer_init(&owner_buf, (uint8_t *) owner->data, owner->size);
        map_owner_load_state(&owner_buf);
    }
    game_file_initialize_multiplayer_state();
    return 1;
}

static int read_whole_file(const char *filename)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        return 0;
    }
    fseek(fp, 0, SEEK_END);
    reading.file_size = (int) ftell(fp);
    fseek(fp, 0, SEEK_SET);
    reading.file = malloc(reading.file_size);
    int ok = reading.file && fread(reading.file, 1, reading.file_size, fp) == (size_t) reading.file_size;
    fclose(fp);
    return ok;
}

int mp_savegame_num_players(const char *filename)
{
    int num_players = 0;
    if (read_whole_file(filename) && parse()) {
        const piece *header = find_piece("mp_header", 0, reading.num_pieces);
        if (header && header->size >= 6 * 4) {
            buffer buf;
            buffer_init(&buf, (uint8_t *) header->data, header->size);
            buffer_skip(&buf, 5 * 4);
            num_players = buffer_read_i32(&buf);
        }
    }
    free(reading.file);
    free(reading.pieces);
    reading.file = 0;
    reading.pieces = 0;
    return num_players;
}

int mp_savegame_read_rules(const char *filename, game_rules_settings *rules)
{
    int ok = 0;
    if (read_whole_file(filename) && parse()) {
        const piece *header = find_piece("mp_header", 0, reading.num_pieces);
        if (header && header->size >= 6 * 4) {
            buffer buf;
            buffer_init(&buf, (uint8_t *) header->data, header->size);
            buffer_skip(&buf, 6 * 4);
            // as when loading the game: the rules missing from an older header read as 0
            int mode = game_rules_read_state(&buf, rules);
            ok = mode == GAME_MODE_MULTIPLAYER && game_rules_settings_valid(rules);
        }
    }
    free(reading.file);
    free(reading.pieces);
    reading.file = 0;
    reading.pieces = 0;
    return ok;
}

int mp_savegame_read(const char *filename)
{
    int ok = read_whole_file(filename);
    ok = ok && parse() && load_pieces();
    free(reading.file);
    free(reading.pieces);
    reading.file = 0;
    reading.pieces = 0;
    if (!ok) {
        log_error("Unable to load multiplayer saved game", filename, 0);
    }
    return ok;
}
