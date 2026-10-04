#include "checksum.h"

#include "building/building.h"
#include "building/building_state.h"
#include "core/buffer.h"
#include "game/extra_state.h"
#include "game/file_io.h"

#include <stdlib.h>
#include <string.h>

#define FNV_OFFSET_BASIS 0xcbf29ce484222325ULL
#define FNV_PRIME 0x100000001b3ULL

#define RECORD_SIZE 128

// Figure record: phrase_sequence_exact, phrase_id and phrase_sequence_city, set by the building info window
#define FIGURE_PHRASE_OFFSET 106
#define FIGURE_PHRASE_SIZE 3

// Bitfields grid: BIT_CONSTRUCTION (0x10) and BIT_DELETED (0x40) are written by construction previews
#define BITFIELDS_SIMULATION_MASK 0xaf

// Saved game pieces that only hold user interface state
static const char *EXCLUDED_PIECES[] = {
    "image_grid",          // rewritten by water animation and view orientation
    "sprite_grid",         // building animation offsets, written by rendering
    "sprite_backup_grid",
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

typedef struct {
    uint64_t total;
    mp_checksum_piece_callback callback;
    void *userdata;
    int overlay_flag_offset;
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

/**
 * Finds where show_on_problem_overlay is stored in a building record: its position depends
 * on the type-specific data written before it, so we let the real serializer tell us.
 */
static int find_overlay_flag_offset(void)
{
    static int offset = -2;
    if (offset != -2) {
        return offset;
    }
    uint8_t first[RECORD_SIZE];
    uint8_t second[RECORD_SIZE];
    buffer buf;
    building b;
    memset(&b, 0, sizeof(b));

    buffer_init(&buf, first, RECORD_SIZE);
    building_state_save_to_buffer(&buf, &b);
    b.show_on_problem_overlay = 1;
    buffer_init(&buf, second, RECORD_SIZE);
    building_state_save_to_buffer(&buf, &b);

    offset = -1;
    for (int i = 0; i < RECORD_SIZE; i++) {
        if (first[i] != second[i]) {
            offset = i;
            break;
        }
    }
    return offset;
}

static uint64_t hash_piece(const char *name, const uint8_t *data, int size, int overlay_flag_offset)
{
    uint64_t hash = FNV_OFFSET_BASIS;
    if (strcmp(name, "figures") == 0) {
        for (int i = 0; i < size; i++) {
            int field = i % RECORD_SIZE;
            int masked = field >= FIGURE_PHRASE_OFFSET && field < FIGURE_PHRASE_OFFSET + FIGURE_PHRASE_SIZE;
            hash = hash_byte(hash, masked ? 0 : data[i]);
        }
    } else if (strcmp(name, "buildings") == 0) {
        for (int i = 0; i < size; i++) {
            // Record 0 is the null building, whose fields are scribbled on by the user interface
            int masked = i < RECORD_SIZE || i % RECORD_SIZE == overlay_flag_offset;
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
    uint64_t piece_hash = hash_piece(name, data, size, ctx->overlay_flag_offset);
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
    static int size;
    if (!data) {
        size = game_extra_state_size();
        data = malloc(size);
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
    checksum_context ctx = { FNV_OFFSET_BASIS, callback, userdata, find_overlay_flag_offset() };
    game_file_io_visit_saved_game(visit_piece, &ctx);
    visit_extra_state(&ctx);
    return ctx.total;
}

uint64_t mp_checksum_state(void)
{
    return mp_checksum_state_pieces(0, 0);
}
