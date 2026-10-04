// Headless simulation tool: loads saved games, runs ticks exactly like the autopilot and
// prints state checksums. See doc/mp/TESTING.md.
#include "building/building.h"
#include "building/construction.h"
#include "building/construction_clear.h"
#include "building/type.h"
#include "building/count.h"
#include "building/menu.h"
#include "building/storage.h"
#include "city/buildings.h"
#include "city/festival.h"
#include "city/finance.h"
#include "city/labor.h"
#include "city/resource.h"
#include "empire/city.h"
#include "empire/type.h"
#include "core/time.h"
#include "game/file.h"
#include "game/game.h"
#include "game/rules.h"
#include "game/settings.h"
#include "map/bridge.h"
#include "map/terrain.h"
#include "map/grid.h"
#include "mp/checksum.h"
#include "game/time.h"
#include "mp/actions.h"
#include "mp/compose.h"
#include "mp/lockstep.h"
#include "mp/session.h"

#include <inttypes.h>
#include <time.h>
#include <unistd.h>
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
    printf("  simtool clearequiv SAVE                same for clearing forts and bridges: answering the popup\n");
    printf("                                         equals sending the answer in the command\n");
    printf("  simtool actionequiv SAVE               same for city settings (taxes, wages, storage, trade...)\n");
    printf("  simtool mpnode host PORT PLAYERS SAVE TICKS   network game host (headless)\n");
    printf("  simtool mpnode join ADDRESS PORT TICKS [desync]  network game client (headless); with\n");
    printf("                                         'desync' it changes its own state to test detection\n");
    printf("                                         each player issues scripted commands; prints the\n");
    printf("                                         final checksum, fails on desynchronisation\n");
    printf("  simtool relocequiv SAVE TICKS STRIDE DX DY  a city moved to a grid of side STRIDE, shifted by\n");
    printf("                                         (DX, DY), runs TICKS exactly as on its original grid\n");
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

int test_stub_answer_popup(int accepted);

static int compare_clear(const char *file, building_construction_placement *p, int answer, int *failures)
{
    // former path: the popup is shown, then answered
    if (!load(file)) {
        return 0;
    }
    build_directly(p);
    int asked = test_stub_answer_popup(answer);
    uint64_t direct = mp_checksum_state();
    int treasury_direct = city_finance_treasury();

    if (!load(file)) {
        return 0;
    }
    int ask_fort, ask_bridge;
    building_construction_clear_land_needs_confirmation(p->x_start, p->y_start, p->x_end, p->y_end,
        &ask_fort, &ask_bridge);
    p->fort_answer = ask_fort ? answer : 0;
    p->bridge_answer = ask_bridge ? answer : 0;
    mp_command command = { .type = MP_COMMAND_BUILD, .args = {
        p->type, p->sub_type, p->x_start, p->y_start, p->x_end, p->y_end, p->road_orientation,
        (p->fort_answer & 0xff) | ((p->bridge_answer & 0xff) << 8)
    } };
    mp_command_submit(&command);
    building_construction_clear_type();
    if (direct != mp_checksum_state() || treasury_direct != city_finance_treasury() || asked != (ask_fort || ask_bridge)) {
        printf("DIFFERENT: clear (%d,%d)-(%d,%d) answer %d asked %d/%d%d\n",
            p->x_start, p->y_start, p->x_end, p->y_end, answer, asked, ask_fort, ask_bridge);
        (*failures)++;
    }
    return asked;
}

static int command_clearequiv(const char *file)
{
    if (!load(file)) {
        return 2;
    }
    // collect forts and bridges of the saved game
    int targets_x[40], targets_y[40], num_targets = 0;
    for (int i = 1; i < MAX_BUILDINGS && num_targets < 20; i++) {
        building *b = building_get(i);
        if (b->state == BUILDING_STATE_IN_USE && b->type == BUILDING_FORT) {
            targets_x[num_targets] = b->x;
            targets_y[num_targets] = b->y;
            num_targets++;
        }
    }
    for (int y = 0; y < GRID_SIZE && num_targets < 40; y++) {
        for (int x = 0; x < GRID_SIZE && num_targets < 40; x++) {
            if (map_is_bridge(map_grid_offset(x, y)) && (x + y) % 3 == 0) {
                targets_x[num_targets] = x;
                targets_y[num_targets] = y;
                num_targets++;
            }
        }
    }
    int failures = 0, asked = 0;
    for (int i = 0; i < num_targets; i++) {
        for (int answer = 1; answer >= -1; answer -= 2) {
            building_construction_placement p = { .type = BUILDING_CLEAR_LAND,
                .x_start = targets_x[i] - 1, .y_start = targets_y[i] - 1,
                .x_end = targets_x[i] + 1, .y_end = targets_y[i] + 1 };
            asked += compare_clear(file, &p, answer, &failures);
        }
    }
    printf("%d forts and bridges, %d confirmations asked, %d different\n", num_targets, asked, failures);
    return failures || !asked ? 1 : 0;
}

static int find_building(building_type type)
{
    for (int i = 1; i < MAX_BUILDINGS; i++) {
        building *b = building_get(i);
        if (b->state == BUILDING_STATE_IN_USE && b->type == type) {
            return i;
        }
    }
    return 0;
}

static int find_closed_trade_city(void)
{
    for (int i = 1; i < 41; i++) {
        empire_city *c = empire_city_get(i);
        if (c->in_use && c->type == EMPIRE_CITY_TRADE && !c->is_open) {
            return i;
        }
    }
    return 0;
}

// Same calls as the user interface buttons before M2.4
static void action_directly(int action, int a, int b)
{
    switch (action) {
        case MP_ACTION_CHANGE_TAXES:
            city_finance_change_tax_percentage(a); city_finance_estimate_taxes(); city_finance_calculate_totals();
            break;
        case MP_ACTION_CHANGE_WAGES:
            city_labor_change_wages(a); city_finance_estimate_wages(); city_finance_calculate_totals();
            break;
        case MP_ACTION_SET_LABOR_PRIORITY: city_labor_set_priority(a, b); break;
        case MP_ACTION_FESTIVAL_SELECT_GOD: city_festival_select_god(a); break;
        case MP_ACTION_FESTIVAL_SELECT_SIZE: city_festival_select_size(a); break;
        case MP_ACTION_FESTIVAL_HOLD: city_festival_schedule(); break;
        case MP_ACTION_STORAGE_CYCLE_RESOURCE: building_storage_cycle_resource_state(building_get(a)->storage_id, b); break;
        case MP_ACTION_STORAGE_TOGGLE_EMPTY_ALL: building_storage_toggle_empty_all(building_get(a)->storage_id); break;
        case MP_ACTION_STORAGE_ACCEPT_NONE: building_storage_accept_none(building_get(a)->storage_id); break;
        case MP_ACTION_SET_TRADE_CENTER: city_buildings_set_trade_center(a); break;
        case MP_ACTION_OPEN_TRADE_ROUTE: empire_city_open_trade(a); building_menu_update(); break;
        case MP_ACTION_CYCLE_TRADE_STATUS: city_resource_cycle_trade_status(a); break;
        case MP_ACTION_CHANGE_EXPORT_OVER: city_resource_change_export_over(a, b); break;
        case MP_ACTION_TOGGLE_STOCKPILED: city_resource_toggle_stockpiled(a); break;
        case MP_ACTION_TOGGLE_MOTHBALLED: city_resource_toggle_mothballed(a); break;
    }
}

static int command_actionequiv(const char *file)
{
    if (!load(file)) {
        return 2;
    }
    int warehouse = find_building(BUILDING_WAREHOUSE);
    int granary = find_building(BUILDING_GRANARY);
    int trade_city = find_closed_trade_city();
    int industry = 0;
    for (int r = RESOURCE_MIN; r < RESOURCE_MAX && !industry; r++) {
        if (building_count_industry_total(r) > 0) {
            industry = r;
        }
    }
    struct { int action, a, b; } cases[] = {
        {MP_ACTION_CHANGE_TAXES, 1, 0}, {MP_ACTION_CHANGE_TAXES, -1, 0},
        {MP_ACTION_CHANGE_WAGES, 1, 0}, {MP_ACTION_CHANGE_WAGES, -1, 0},
        {MP_ACTION_SET_LABOR_PRIORITY, 2, 1}, {MP_ACTION_SET_LABOR_PRIORITY, 5, 3},
        {MP_ACTION_FESTIVAL_SELECT_GOD, 3, 0}, {MP_ACTION_FESTIVAL_SELECT_SIZE, 2, 0}, {MP_ACTION_FESTIVAL_HOLD, 0, 0},
        {MP_ACTION_STORAGE_CYCLE_RESOURCE, warehouse, RESOURCE_POTTERY},
        {MP_ACTION_STORAGE_CYCLE_RESOURCE, granary, RESOURCE_WHEAT},
        {MP_ACTION_STORAGE_TOGGLE_EMPTY_ALL, warehouse, 0}, {MP_ACTION_STORAGE_ACCEPT_NONE, granary, 0},
        {MP_ACTION_SET_TRADE_CENTER, warehouse, 0},
        {MP_ACTION_OPEN_TRADE_ROUTE, trade_city, 0},
        {MP_ACTION_CYCLE_TRADE_STATUS, RESOURCE_OIL, 0}, {MP_ACTION_CYCLE_TRADE_STATUS, RESOURCE_WEAPONS, 0},
        {MP_ACTION_CHANGE_EXPORT_OVER, RESOURCE_OIL, 1}, {MP_ACTION_TOGGLE_STOCKPILED, RESOURCE_WHEAT, 0},
        {MP_ACTION_TOGGLE_MOTHBALLED, industry, 0}
    };
    int num_cases = sizeof(cases) / sizeof(cases[0]);
    int failures = 0, effective = 0, tested = 0;
    for (int i = 0; i < num_cases; i++) {
        if (!cases[i].a && cases[i].action >= MP_ACTION_STORAGE_CYCLE_RESOURCE &&
            cases[i].action != MP_ACTION_FESTIVAL_HOLD && cases[i].action <= MP_ACTION_OPEN_TRADE_ROUTE) {
            continue; // nothing to act on in this saved game
        }
        if (cases[i].action == MP_ACTION_TOGGLE_MOTHBALLED && !industry) {
            continue;
        }
        load(file);
        uint64_t before = mp_checksum_state();
        action_directly(cases[i].action, cases[i].a, cases[i].b);
        uint64_t direct = mp_checksum_state();
        run_trace(25, 25, 0, 0);
        uint64_t direct_later = mp_checksum_state();

        load(file);
        mp_command command = { .type = MP_COMMAND_CITY_ACTION, .args = { cases[i].action, cases[i].a, cases[i].b } };
        mp_command_submit(&command);
        uint64_t via_command = mp_checksum_state();
        run_trace(25, 25, 0, 0);
        uint64_t command_later = mp_checksum_state();

        tested++;
        effective += direct != before;
        if (direct != via_command || direct_later != command_later) {
            printf("DIFFERENT: action %d (%d, %d)\n", cases[i].action, cases[i].a, cases[i].b);
            failures++;
        }
    }
    printf("%d actions tested, %d changed the city, %d different\n", tested, effective, failures);
    return failures || effective < tested / 2 ? 1 : 0;
}

// Scripted player: builds roads and changes taxes, differently for each player
static void mpnode_play(int tick_in_game)
{
    int player = mp_session_local_player_id();
    if (tick_in_game % 40 == 7 + player * 3) {
        int n = tick_in_game / 40;
        int x = 20 + (n * 7 + player * 31) % 120;
        int y = 20 + (n * 13 + player * 17) % 120;
        mp_command command = { .type = MP_COMMAND_BUILD, .args = {
            n % 3 == 2 ? BUILDING_HOUSE_VACANT_LOT : BUILDING_ROAD, 0, x, y, x + 6, y + (n % 2) * 3, 0, 0
        } };
        mp_command_submit(&command);
    }
    if (tick_in_game % 100 == 50 + player) {
        mp_action_change_taxes(player % 2 ? 1 : -1);
    }
}

static int command_mpnode(int argc, char **argv)
{
    // argv: mpnode host PORT PLAYERS SAVE TICKS | mpnode join ADDRESS PORT TICKS
    int is_host = argc >= 7 && strcmp(argv[2], "host") == 0;
    int is_join = argc >= 6 && strcmp(argv[2], "join") == 0;
    if (!is_host && !is_join) {
        return usage();
    }
    int ticks = atoi(is_host ? argv[6] : argv[5]);
    int cheat = is_join && argc >= 7 && strcmp(argv[6], "desync") == 0;
    int ok = is_host ? mp_lockstep_host(atoi(argv[3]), atoi(argv[4]), argv[5])
                     : mp_lockstep_join(argv[3], atoi(argv[4]));
    if (!ok) {
        printf("FAILED: %s\n", mp_lockstep_status());
        return 1;
    }
    setting_reset_speeds(500, setting_scroll_speed());
    time_t deadline = time(0) + 120;
    int start_tick = -1;
    int last_played = -1;
    while (time(0) < deadline) {
        mp_lockstep_state state = mp_lockstep_get_state();
        if (state == MP_LOCKSTEP_DESYNC || state == MP_LOCKSTEP_DISCONNECTED) {
            break;
        }
        if (state == MP_LOCKSTEP_RUNNING && start_tick < 0) {
            start_tick = game_time_absolute_tick();
        }
        int before = game_time_absolute_tick();
        if (start_tick >= 0) {
            int tick_in_game = before - start_tick;
            if (tick_in_game >= ticks) {
                break;
            }
            if (tick_in_game != last_played) {
                mpnode_play(tick_in_game);
                if (cheat && tick_in_game == ticks / 2) {
                    city_finance_change_tax_percentage(3); // changed on this computer only
                }
                last_played = tick_in_game;
            }
        }
        clock_millis += 2;
        time_set_millis(clock_millis);
        game_run();
        if (game_time_absolute_tick() == before) {
            usleep(500);
        }
    }
    // the host keeps listening a little to verify the last turns of the clients
    if (is_host) {
        time_t end = time(0) + 3;
        int last_turn = (ticks / 4) - 1;
        while (time(0) < end && mp_lockstep_last_verified_turn() < last_turn &&
            mp_lockstep_get_state() == MP_LOCKSTEP_RUNNING) {
            mp_lockstep_poll();
            usleep(1000);
        }
        printf("verified turns: %d / %d\n", mp_lockstep_last_verified_turn() + 1, last_turn + 1);
    } else {
        usleep(300 * 1000); // let the last checksums reach the host
    }
    printf("status: %s\n", mp_lockstep_status());
    if (cheat || strcmp(argv[argc - 1], "expect-desync") == 0) {
        // expected outcome: the desynchronisation is detected
        int detected = mp_lockstep_get_state() == MP_LOCKSTEP_DESYNC;
        printf("desync %s\n", detected ? "detected" : "NOT detected");
        mp_lockstep_stop();
        return detected ? 0 : 1;
    }
    printf("tick %d checksum %016" PRIx64 "\n", start_tick >= 0 ? game_time_absolute_tick() - start_tick : -1,
        mp_checksum_state());
    int result = mp_lockstep_get_state() == MP_LOCKSTEP_RUNNING && start_tick >= 0 &&
        game_time_absolute_tick() - start_tick == ticks;
    if (is_host) {
        result = result && mp_lockstep_last_verified_turn() == (ticks / 4) - 1;
    }
    mp_lockstep_stop();
    return result ? 0 : 1;
}

static int command_relocequiv(const char *file, int ticks, int stride, int dx, int dy)
{
    // reference: same code path, but the city stays where it is
    if (!load(file) || !mp_compose_relocate(162, 0, 0)) {
        return 2;
    }
    run_trace(ticks, ticks, 0, 0);
    mp_compose_relocate(162, 0, 0);
    uint64_t reference = mp_checksum_state();
    game_file_write_saved_game("relocequiv-reference.sav");

    if (!load(file) || !mp_compose_relocate(stride, dx, dy)) {
        printf("Unable to move the city to a grid of %d shifted by (%d, %d)\n", stride, dx, dy);
        return 2;
    }
    run_trace(ticks, ticks, 0, 0);
    if (!mp_compose_relocate(162, -dx, -dy)) {
        printf("Unable to move the city back\n");
        return 2;
    }
    uint64_t moved = mp_checksum_state();
    game_file_write_saved_game("relocequiv-moved.sav");
    if (moved != reference) {
        printf("DIFFERENT after %d ticks on a grid of %d shifted by (%d, %d): see relocequiv-*.sav\n",
            ticks, stride, dx, dy);
        return 1;
    }
    printf("Identical after %d ticks on a grid of %d shifted by (%d, %d)\n", ticks, stride, dx, dy);
    return 0;
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
    } else if (strcmp(command, "relocequiv") == 0 && argc > 6) {
        result = command_relocequiv(file, ticks, atoi(argv[4]), atoi(argv[5]), atoi(argv[6]));
    } else if (strcmp(command, "mpnode") == 0) {
        result = command_mpnode(argc, argv);
    } else if (strcmp(command, "actionequiv") == 0) {
        result = command_actionequiv(file);
    } else if (strcmp(command, "clearequiv") == 0) {
        result = command_clearequiv(file);
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
