#include "checksum.h"

#include "building/building.h"
#include "building/building_state.h"
#include "city/ratings.h"
#include "core/buffer.h"
#include "game/extra_state.h"
#include "mp/savegame.h"
#include "game/save_format.h"
#include "game/player_context.h"
#include "figure/figure.h"
#include "game/file_io.h"

#include <stdlib.h>
#include <string.h>

#define FNV_OFFSET_BASIS 0xcbf29ce484222325ULL
#define FNV_PRIME 0x100000001b3ULL


// Figure record: phrase_sequence_exact, phrase_id and phrase_sequence_city, set by the building info window
#define FIGURE_PHRASE_SIZE 3

// Bitfields grid: BIT_CONSTRUCTION (0x10) and BIT_DELETED (0x40) are written by construction previews
#define BITFIELDS_SIMULATION_MASK 0xaf

// Saved game pieces that only hold user interface state
static const char *EXCLUDED_PIECES[] = {
    "image_grid",          // rewritten by water animation and view orientation
    "sprite_grid",         // building animation offsets, written by rendering
    "sprite_backup_grid",
    "aqueduct_backup_grid", // undo backup, taken by the user interface when a construction starts
    "city_view_orientation",
    "city_view_camera",
    "city_graph_order",
    "city_sounds",
    "empire",              // empire map scroll position and selection
    "messages",            // read flags and popups depend on the open window
    "message_extra",
    "population_messages",
    "message_counts",
    "message_delays",
    "routing_counters",    // statistics, also incremented by the user interface
    "bookmarks",
    "end_marker",
    0
};

// Where the fields written by the user interface are, in classic and in wide records (D-024)
typedef struct {
    int building_size;
    int building_overlay;
    int building_type;
    int building_evolve_text;
    int figure_size;
    int figure_phrase;
} record_layout;

typedef struct {
    uint64_t total;
    mp_checksum_piece_callback callback;
    void *userdata;
    record_layout layout;
} checksum_context;

static int is_excluded(const char *name)
{
    for (int i = 0; EXCLUDED_PIECES[i]; i++) {
        if (strcmp(name, EXCLUDED_PIECES[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

static uint64_t hash_byte(uint64_t hash, uint8_t value)
{
    return (hash ^ value) * FNV_PRIME;
}

static int serialize_building(uint8_t *data, building *b)
{
    buffer buf;
    buffer_init(&buf, data, 512);
    building_state_save_to_buffer(&buf, b);
    return buf.index;
}

static int first_difference(const uint8_t *a, const uint8_t *b, int size)
{
    for (int i = 0; i < size; i++) {
        if (a[i] != b[i]) {
            return i;
        }
    }
    return -1;
}

static int building_field_offset(int size, void (*set)(building *b, int value))
{
    uint8_t first[512], second[512];
    building b;
    memset(&b, 0, sizeof(b));
    set(&b, 0);
    serialize_building(first, &b);
    set(&b, 1);
    serialize_building(second, &b);
    return first_difference(first, second, size);
}

static void set_overlay(building *b, int value)
{
    b->show_on_problem_overlay = value;
}

static void set_type(building *b, int value)
{
    b->type = value ? BUILDING_HOUSE_LARGE_TENT : BUILDING_HOUSE_SMALL_TENT;
}

static void set_evolve_text(building *b, int value)
{
    b->type = BUILDING_HOUSE_SMALL_TENT;
    b->data.house.evolve_text_id = value;
}

static const record_layout *get_layout(void)
{
    static record_layout layouts[2];
    static int known[2];
    int mode = save_format_wide ? 1 : 0;
    if (!known[mode]) {
        record_layout *l = &layouts[mode];
        uint8_t first[512], second[512];
        building b;
        memset(&b, 0, sizeof(b));
        l->building_size = serialize_building(first, &b);
        l->building_overlay = building_field_offset(l->building_size, set_overlay);
        l->building_type = building_field_offset(l->building_size, set_type);
        l->building_evolve_text = building_field_offset(l->building_size, set_evolve_text);

        figure f;
        memset(&f, 0, sizeof(f));
        buffer buf;
        buffer_init(&buf, first, 512);
        figure_save_record(&buf, &f);
        l->figure_size = buf.index;
        f.phrase_sequence_exact = 1;
        buffer_init(&buf, second, 512);
        figure_save_record(&buf, &f);
        l->figure_phrase = first_difference(first, second, l->figure_size);
        known[mode] = 1;
    }
    return &layouts[mode];
}

static int record_type(const uint8_t *record, int type_offset)
{
    // building types are written as 16 bit little endian values
    return type_offset >= 0 ? record[type_offset] | (record[type_offset + 1] << 8) : 0;
}

static uint64_t hash_piece(const char *name, const uint8_t *data, int size, const checksum_context *ctx)
{
    const record_layout *l = &ctx->layout;
    uint64_t hash = FNV_OFFSET_BASIS;
    if (strcmp(name, "figures") == 0) {
        for (int i = 0; i < size; i++) {
            int field = i % l->figure_size;
            // record 0 is the null figure: a scratch record written to when no figure exists, shared by all cities;
            // phrases are chosen by the building info window
            int masked = i < l->figure_size ||
                (field >= l->figure_phrase && field < l->figure_phrase + FIGURE_PHRASE_SIZE);
            hash = hash_byte(hash, masked ? 0 : data[i]);
        }
    } else if (strcmp(name, "buildings") == 0) {
        int is_house = 0;
        for (int i = 0; i < size; i++) {
            int field = i % l->building_size;
            if (field == 0) {
                is_house = building_is_house(record_type(&data[i], l->building_type));
            }
            // record 0 is the null building, whose fields are scribbled on by the user interface;
            // the overlay flag and the evolution text of houses are set by windows
            int masked = i < l->building_size || field == l->building_overlay ||
                (is_house && field == l->building_evolve_text);
            hash = hash_byte(hash, masked ? 0 : data[i]);
        }
    } else if (strcmp(name, "bitfields_grid") == 0) {
        for (int i = 0; i < size; i++) {
            hash = hash_byte(hash, data[i] & BITFIELDS_SIMULATION_MASK);
        }
    } else {
        for (int i = 0; i < size; i++) {
            hash = hash_byte(hash, data[i]);
        }
    }
    return hash;
}

static void visit_piece(const char *name, const unsigned char *data, int size, void *userdata)
{
    checksum_context *ctx = userdata;
    if (is_excluded(name)) {
        return;
    }
    uint64_t piece_hash = hash_piece(name, data, size, ctx);
    if (ctx->callback) {
        ctx->callback(name, piece_hash, ctx->userdata);
    }
    // Fold the piece hash into the total, in file order
    for (int i = 0; i < 8; i++) {
        ctx->total = hash_byte(ctx->total, (uint8_t) (piece_hash >> (8 * i)));
    }
}

// State that classic saved games do not store, hashed as an extra piece
static void visit_extra_state(checksum_context *ctx)
{
    static uint8_t *data;
    static int capacity;
    // its size follows the size of the grid
    int size = game_extra_state_size();
    if (capacity < size) {
        free(data);
        data = malloc(size);
        capacity = data ? size : 0;
        if (!data) {
            return;
        }
    }
    buffer buf;
    buffer_init(&buf, data, size);
    game_extra_state_save(&buf);
    visit_piece("extra_state", data, size, ctx);
}

uint64_t mp_checksum_state_pieces(mp_checksum_piece_callback callback, void *userdata)
{
    checksum_context ctx = { FNV_OFFSET_BASIS, callback, userdata };
    // the rating selected in the ratings advisor is a choice of the local player
    int previous = player_context_current();
    selected_rating ratings[PLAYER_CONTEXT_MAX_PLAYERS];
    for (int p = 0; p < player_context_num_players(); p++) {
        player_context_switch(p);
        ratings[p] = city_rating_selected();
        city_rating_select(SELECTED_RATING_NONE);
    }
    player_context_switch(previous);
    if (mp_savegame_is_needed()) {
        // several cities or a large map: every city, wide fields
        save_format_wide = 1;
        ctx.layout = *get_layout();
        save_format_wide = 0;
        mp_savegame_visit(visit_piece, &ctx);
    } else {
        ctx.layout = *get_layout();
        game_file_io_visit_saved_game(visit_piece, &ctx);
        visit_extra_state(&ctx);
    }
    for (int p = 0; p < player_context_num_players(); p++) {
        player_context_switch(p);
        city_rating_select(ratings[p]);
    }
    player_context_switch(previous);
    return ctx.total;
}

uint64_t mp_checksum_state(void)
{
    return mp_checksum_state_pieces(0, 0);
}
