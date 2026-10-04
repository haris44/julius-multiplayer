// Headless simulation tool: loads saved games, runs ticks exactly like the autopilot and
// prints state checksums. See doc/mp/TESTING.md.
#include "building/construction.h"
#include "building/type.h"
#include "city/finance.h"
#include "core/time.h"
#include "game/file.h"
#include "game/game.h"
#include "game/rules.h"
#include "game/settings.h"
#include "map/bridge.h"
#include "map/grid.h"
#include "mp/checksum.h"
#include "mp/session.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SAMPLES 100000

static time_millis clock_millis;

static struct {
    int local_difficulty; // -1: keep the c3.inf value
    int local_gods;       // -1: keep the c3.inf value
    int multiplayer;      // apply the default multiplayer rules after each load
} options = { -1, -1, 0 };

static int usage(void)
{
    printf("Usage: simtool [OPTIONS] COMMAND ...\n");
    printf("Options (before the command):\n");
    printf("  --difficulty N    local difficulty setting (0 very easy .. 4 very hard), as if set in c3.inf\n");
    printf("  --gods 0|1        local gods setting, as if set in c3.inf\n");
    printf("  --mp              play with the default multiplayer rules instead of the local settings\n");
    printf("Commands:\n");
    printf("  simtool checksum SAVE                  checksum of a loaded saved game\n");
    printf("  simtool run SAVE TICKS OUTPUT          runs TICKS ticks and writes the saved game OUTPUT\n");
    printf("  simtool trace SAVE TICKS [STEP]        checksum every STEP ticks (default 1)\n");
    printf("  simtool pieces SAVE [TICKS]            checksum of every saved game piece after TICKS\n");
    printf("  simtool buildequiv SAVE                placing buildings through commands gives the same\n");
    printf("                                         state as the former direct user interface calls\n");
    printf("  simtool idempotence SAVE TICKS [STEP]  loads and runs SAVE twice in one process,\n");
    printf("                                         fails if the two traces differ\n");
    printf("  simtool diffpieces SAVE TICKS [CHECK]  runs SAVE for TICKS, reloads it and runs CHECK ticks\n");
    printf("                                         (default TICKS), lists the pieces that differ at CHECK\n");
    return 2;
}

static int load(const char *file)
{
    if (!game_file_load_saved_game(file)) {
        printf("Unable to load saved game %s\n", file);
        return 0;
    }
    if (options.multiplayer) {
        game_rules_settings rules;
        game_rules_default_multiplayer_settings(&rules);
        game_rules_set_multiplayer(&rules);
    }
    return 1;
}

static void apply_local_settings(void)
{
    if (options.local_difficulty >= 0) {
        for (int i = 0; i < 5; i++) {
            setting_decrease_difficulty();
        }
        for (int i = 0; i < options.local_difficulty; i++) {
            setting_increase_difficulty();
        }
    }
    if (options.local_gods >= 0 && setting_gods_enabled() != options.local_gods) {
        setting_toggle_gods_enabled();
    }
}

// Same stepping as test/sav/run.c: 2 ms per call at speed 500 gives exactly one tick per game_run().
// The clock never goes back, so several runs can follow each other in one process.
static void run_one_tick(void)
{
    clock_millis += 2;
    time_set_millis(clock_millis);
    game_run();
}

static int run_trace(int ticks, int step, uint64_t *samples, FILE *out)
{
    int num_samples = 0;
    setting_reset_speeds(500, setting_scroll_speed());
    uint64_t checksum = mp_checksum_state();
    if (out) {
        fprintf(out, "%d %016" PRIx64 "\n", 0, checksum);
    }
    if (samples) {
        samples[num_samples] = checksum;
    }
    num_samples++;
    for (int tick = 1; tick <= ticks; tick++) {
        run_one_tick();
        if (tick % step == 0 || tick == ticks) {
            checksum = mp_checksum_state();
            if (out) {
                fprintf(out, "%d %016" PRIx64 "\n", tick, checksum);
            }
            if (samples && num_samples < MAX_SAMPLES) {
                samples[num_samples] = checksum;
            }
            num_samples++;
        }
    }
    return num_samples;
}

static void print_piece(const char *name, uint64_t checksum, void *userdata)
{
    printf("%-34s %016" PRIx64 "\n", name, checksum);
}

static int command_checksum(const char *file)
{
    if (!load(file)) {
        return 2;
    }
    printf("%016" PRIx64 "\n", mp_checksum_state());
    return 0;
}

static int command_run(const char *file, int ticks, const char *output)
{
    if (!load(file)) {
        return 2;
    }
    run_trace(ticks, ticks > 0 ? ticks : 1, 0, 0);
    if (!game_file_write_saved_game(output)) {
        printf("Unable to write %s\n", output);
        return 2;
    }
    printf("%016" PRIx64 "\n", mp_checksum_state());
    return 0;
}

// Former user interface path (widget/city.c before M2.2): select the type, press, drag, release
static void build_directly(const building_construction_placement *p)
{
    building_construction_set_type(p->type);
    building_construction_start(p->x_start, p->y_start, map_grid_offset(p->x_start, p->y_start));
    if (!building_construction_in_progress()) {
        return;
    }
    building_construction_update(p->x_end, p->y_end, map_grid_offset(p->x_end, p->y_end));
    if (p->type == BUILDING_LOW_BRIDGE) {
        int length, direction;
        map_bridge_calculate_length_direction(p->x_end, p->y_end, &length, &direction);
    }
    building_construction_place();
    building_construction_clear_type();
}

static void build_with_command(const building_construction_placement *p)
{
    mp_command command = { .type = MP_COMMAND_BUILD, .args = {
        p->type, p->sub_type, p->x_start, p->y_start, p->x_end, p->y_end, p->road_orientation
    } };
    mp_command_submit(&command);
    building_construction_clear_type();
}

static int command_buildequiv(const char *file)
{
    static const int types[] = {
        BUILDING_ROAD, BUILDING_HOUSE_VACANT_LOT, BUILDING_WELL, BUILDING_PREFECTURE, BUILDING_CLEAR_LAND,
        BUILDING_WALL, BUILDING_AQUEDUCT, BUILDING_GARDENS, BUILDING_PLAZA, BUILDING_MARKET,
        BUILDING_WAREHOUSE, BUILDING_DRAGGABLE_RESERVOIR, BUILDING_GATEHOUSE, BUILDING_LOW_BRIDGE
    };
    int num_types = sizeof(types) / sizeof(types[0]);
    int tested = 0, effective = 0, failures = 0;
    for (int y = 12; y < 150; y += 17) {
        for (int x = 12; x < 150; x += 11) {
            building_construction_placement p = {
                .type = types[tested % num_types],
                .x_start = x, .y_start = y,
                .x_end = x + (tested % 5), .y_end = y + (tested % 3),
                .road_orientation = types[tested % num_types] == BUILDING_GATEHOUSE ? 1 : 0
            };
            if (!load(file)) {
                return 2;
            }
            uint64_t before = mp_checksum_state();
            build_directly(&p);
            uint64_t direct = mp_checksum_state();
            int treasury_direct = city_finance_treasury();
            run_trace(25, 25, 0, 0);
            uint64_t direct_later = mp_checksum_state();

            if (!load(file)) {
                return 2;
            }
            build_with_command(&p);
            uint64_t via_command = mp_checksum_state();
            int treasury_command = city_finance_treasury();
            run_trace(25, 25, 0, 0);
            uint64_t command_later = mp_checksum_state();

            tested++;
            if (direct != before) {
                effective++;
            }
            if (direct != via_command || direct_later != command_later || treasury_direct != treasury_command) {
                failures++;
                printf("DIFFERENT: type %d from (%d,%d) to (%d,%d)\n", p.type, p.x_start, p.y_start, p.x_end, p.y_end);
            }
        }
    }
    printf("%d placements tested, %d changed the city, %d different\n", tested, effective, failures);
    // at least some placements must succeed, otherwise the test proves nothing
    return failures || effective < 10 ? 1 : 0;
}

static int command_trace(const char *file, int ticks, int step)
{
    if (!load(file)) {
        return 2;
    }
    run_trace(ticks, step, 0, stdout);
    return 0;
}

static int command_pieces(const char *file, int ticks)
{
    if (!load(file)) {
        return 2;
    }
    if (ticks > 0) {
        run_trace(ticks, ticks, 0, 0);
    }
    uint64_t total = mp_checksum_state_pieces(print_piece, 0);
    printf("%-34s %016" PRIx64 "\n", "TOTAL", total);
    return 0;
}

#define MAX_PIECES 128

typedef struct {
    int count;
    const char *names[MAX_PIECES];
    uint64_t checksums[MAX_PIECES];
} piece_list;

static void record_piece(const char *name, uint64_t checksum, void *userdata)
{
    piece_list *list = userdata;
    if (list->count < MAX_PIECES) {
        list->names[list->count] = name;
        list->checksums[list->count] = checksum;
        list->count++;
    }
}

static int command_diffpieces(const char *file, int ticks, int check_tick)
{
    static piece_list first;
    static piece_list second;
    if (check_tick <= 0 || check_tick > ticks) {
        check_tick = ticks;
    }
    if (!load(file)) {
        return 2;
    }
    run_trace(check_tick, check_tick, 0, 0);
    mp_checksum_state_pieces(record_piece, &first);
    game_file_write_saved_game("diffpieces-first.sav");
    if (ticks > check_tick) {
        run_trace(ticks - check_tick, ticks, 0, 0);
    }
    if (!load(file)) {
        return 2;
    }
    run_trace(check_tick, check_tick, 0, 0);
    mp_checksum_state_pieces(record_piece, &second);
    game_file_write_saved_game("diffpieces-second.sav");
    int differences = 0;
    for (int i = 0; i < first.count && i < second.count; i++) {
        if (first.checksums[i] != second.checksums[i]) {
            printf("differs: %s\n", first.names[i]);
            differences++;
        }
    }
    printf("%d piece(s) differ at tick %d of the second run\n", differences, check_tick);
    if (differences) {
        printf("States written to diffpieces-first.sav and diffpieces-second.sav (see the compare tool)\n");
    }
    return differences ? 1 : 0;
}

static int command_idempotence(const char *file, int ticks, int step)
{
    static uint64_t first[MAX_SAMPLES];
    static uint64_t second[MAX_SAMPLES];
    if (!load(file)) {
        return 2;
    }
    int n = run_trace(ticks, step, first, 0);
    if (!load(file)) {
        return 2;
    }
    run_trace(ticks, step, second, 0);
    if (n > MAX_SAMPLES) {
        n = MAX_SAMPLES;
    }
    for (int i = 0; i < n; i++) {
        if (first[i] != second[i]) {
            int tick = i == n - 1 ? ticks : i * step;
            printf("Second run of %s diverges at tick %d (sample %d): %016" PRIx64 " != %016" PRIx64 "\n",
                file, tick, i, first[i], second[i]);
            return 1;
        }
    }
    printf("Both runs of %s are identical over %d ticks (%d samples)\n", file, ticks, n);
    return 0;
}

int main(int argc, char **argv)
{
    int first = 1;
    while (first < argc && strncmp(argv[first], "--", 2) == 0) {
        if (strcmp(argv[first], "--mp") == 0) {
            options.multiplayer = 1;
            first++;
        } else if (strcmp(argv[first], "--difficulty") == 0 && first + 1 < argc) {
            options.local_difficulty = atoi(argv[first + 1]);
            first += 2;
        } else if (strcmp(argv[first], "--gods") == 0 && first + 1 < argc) {
            options.local_gods = atoi(argv[first + 1]);
            first += 2;
        } else {
            return usage();
        }
    }
    argc -= first - 1;
    argv += first - 1;
    if (argc < 3) {
        return usage();
    }
    const char *command = argv[1];
    const char *file = argv[2];
    int ticks = argc > 3 ? atoi(argv[3]) : 0;
    int step = argc > 4 ? atoi(argv[4]) : 1;
    if (step <= 0) {
        step = 1;
    }

    if (!game_pre_init() || !game_init()) {
        printf("Unable to initialize the game\n");
        return 2;
    }
    apply_local_settings();

    int result;
    if (strcmp(command, "checksum") == 0) {
        result = command_checksum(file);
    } else if (strcmp(command, "buildequiv") == 0) {
        result = command_buildequiv(file);
    } else if (strcmp(command, "run") == 0 && argc > 4) {
        result = command_run(file, ticks, argv[4]);
    } else if (strcmp(command, "trace") == 0 && argc > 3) {
        result = command_trace(file, ticks, step);
    } else if (strcmp(command, "pieces") == 0) {
        result = command_pieces(file, ticks);
    } else if (strcmp(command, "idempotence") == 0 && argc > 3) {
        result = command_idempotence(file, ticks, step);
    } else if (strcmp(command, "diffpieces") == 0 && argc > 3) {
        result = command_diffpieces(file, ticks, step == 1 && argc <= 4 ? ticks : step);
    } else {
        result = usage();
    }
    // Never write c3.inf or julius.ini: other tests in this directory depend on them
    game_exit_without_saving_settings();
    return result;
}
