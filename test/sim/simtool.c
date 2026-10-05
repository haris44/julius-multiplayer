// Headless simulation tool: loads saved games, runs ticks exactly like the autopilot and
// prints state checksums. See doc/mp/TESTING.md.
#include "building/building.h"
#include "building/construction.h"
#include "figure/figure.h"
#include "building/construction_clear.h"
#include "building/type.h"
#include "building/count.h"
#include "building/menu.h"
#include "building/storage.h"
#include "city/view.h"
#include "city/buildings.h"
#include "city/festival.h"
#include "city/finance.h"
#include "city/labor.h"
#include "city/health.h"
#include "city/population.h"
#include "city/ratings.h"
#include "scenario/data.h"
#include "scenario/property.h"
#include "scenario/request.h"
#include "city/data_private.h"
#include "city/resource.h"
#include "city/sentiment.h"
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
#include "figure/route.h"
#include "mp/audit.h"
#include "map/road_network.h"
#include "map/routing.h"
#include "map/routing_terrain.h"
#include "building/construction_routed.h"
#include "game/undo.h"
#include "map/figure.h"
#include "map/water_supply.h"
#include "map/building.h"
#include "map/owner.h"
#include "mp/endgame.h"
#include "mp/permissions.h"
#include "mp/mapgen.h"
#include "city/map.h"
#include "mp/checksum.h"
#include "game/time.h"
#include "game/player_context.h"
#include "map/data.h"
#include "mp/actions.h"
#include "mp/compose.h"
#include "mp/savegame.h"
#include "mp/fog.h"
#include "building/warehouse.h"
#include "mp/missionary.h"
#include "mp/territory.h"
#include "scenario/map.h"
#include "game/resource.h"
#include "mp/colors.h"
#include "mp/lockstep.h"
#include "mp/session.h"

#include <inttypes.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SAMPLES 100000
#define MAX_PIECES 128
#define FIGURE_TYPE_COUNT 256

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
    printf("  simtool mpnode host PORT PLAYERS SAVE TICKS [cities]  network game host (headless); with\n");
    printf("                                         'cities' every player has a copy of the city\n");
    printf("  simtool mpnode join ADDRESS PORT TICKS [desync]  network game client (headless); with\n");
    printf("                                         'desync' it changes its own state to test detection\n");
    printf("                                         each player issues scripted commands; prints the\n");
    printf("                                         final checksum, fails on desynchronisation\n");
    printf("  simtool relocequiv SAVE TICKS STRIDE DX DY  a city moved to a grid of side STRIDE, shifted by\n");
    printf("                                         (DX, DY), runs TICKS exactly as on its original grid\n");
    printf("  simtool twins SAVE TICKS               the city with a twin city below it on a large map runs\n");
    printf("                                         exactly as alone, and the twin gets the same statistics\n");
    printf("  simtool mapgen TEMPLATE PLAYERS SEED TICKS  generated map: every player settles at its arrival\n");
    printf("                                         point and gets immigrants\n");
    printf("  simtool mpsave SAVE CITIES TICKS OUT   writes OUT, a multiplayer saved game of CITIES copies of\n");
    printf("                                         SAVE after TICKS ticks\n");
    printf("  simtool intruders SAVE TICKS           the walkers of a twin city moved into the first city act on\n");
    printf("                                         none of its buildings\n");
    printf("  simtool neighbours SAVE TICKS          two copies of the city joined by a road: walkers cross,\n");
    printf("                                         but no city acts on the buildings of the other\n");
    printf("  simtool preparedmap SAVE PLAYERS TICKS prepared map: land reached from the main road, materials of\n");
    printf("                                         each arrival point, permissions; MAPGEN_PICTURE=F.ppm\n");
    printf("  simtool reservoirlevel SAVE            prepared map: a reservoir cut off from its source empties\n");
    printf("                                         slowly, joined again it fills\n");
    printf("  simtool notrade SAVE                   a template without trade by land and by sea is refused\n");
    printf("  simtool tradecities MAP                trade cities of the empire of a map, by land and by sea\n");
    printf("  simtool fog SAVE                       prepared map: what a player discovers and sees (D-038)\n");
    printf("  simtool outside SAVE                   prepared map: buildings outside the zone collapse in 3 months\n");
    printf("  simtool missions SAVE                  prepared map: missions near a missionary, marble, training\n");
    printf("  simtool territory SAVE                 prepared map: players build only in their zone (D-036)\n");
    printf("  simtool caesarroads SAVE               generated map: no player clears a road of Caesar, every\n");
    printf("                                         city uses it\n");
    printf("  simtool viewcorners SAVE PLAYERS       generated map: the camera reaches its four corners\n");
    printf("  simtool privatepopups SAVE TICKS       twin cities: the popups and sounds of player 2 do not\n");
    printf("                                         reach player 1\n");
    printf("  simtool caesarfree SAVE TICKS          Caesar (requests, anger, salary...) acts in a classic game\n");
    printf("                                         and not in a multiplayer game\n");
    printf("  simtool mpresume SAVE TICKS MORE       twin cities: saving after TICKS (.mpsav), then loading it and\n");
    printf("                                         running MORE ticks equals running on without saving\n");
    printf("  simtool idempotence SAVE TICKS [STEP]  loads and runs SAVE twice in one process,\n");
    printf("                                         fails if the two traces differ\n");
    printf("  simtool diffpieces SAVE TICKS [CHECK]  runs SAVE for TICKS, reloads it and runs CHECK ticks\n");
    printf("                                         (default TICKS), lists the pieces that differ at CHECK\n");
    return 2;
}

static int load(const char *file)
{
    size_t length = strlen(file);
    int is_map = length > 4 && strcmp(file + length - 4, ".map") == 0;
    if (is_map) {
        scenario_set_custom(2); // a map of the free game
    }
    if (!(is_map ? game_file_start_scenario(file) : game_file_load_saved_game(file))) {
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
        // separate cities: in the city of the player
        int x0 = 0, y0 = 0, size;
        if (player_context_num_players() > 1) {
            mp_compose_city_area(player, MP_COMPOSE_CITY_GAP, &x0, &y0, &size);
        }
        int x = x0 + 20 + (n * 7 + player * 31) % 120;
        int y = y0 + 20 + (n * 13 + player * 17) % 120;
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
    // argv: mpnode host PORT PLAYERS SAVE TICKS [cities] | mpnode join ADDRESS PORT TICKS
    int is_host = argc >= 7 && strcmp(argv[2], "host") == 0;
    int cities = 0, generate = 0;
    for (int i = 7; i < argc; i++) {
        cities |= strcmp(argv[i], "cities") == 0;
        generate |= strcmp(argv[i], "generate") == 0;
    }
    int is_join = argc >= 6 && strcmp(argv[2], "join") == 0;
    if (!is_host && !is_join) {
        return usage();
    }
    int ticks = atoi(is_host ? argv[6] : argv[5]);
    int cheat = is_join && argc >= 7 && strcmp(argv[6], "desync") == 0;
    // 'pause': this client pauses the game at half time and resumes it a second later;
    // 'leave': this client leaves the game at half time
    int pauser = is_join && argc >= 7 && strcmp(argv[6], "pause") == 0;
    // 'baddata': this client has other game data; the host must refuse it
    int bad_data = is_join && argc >= 7 && strcmp(argv[6], "baddata") == 0;
    int expect_reject = is_host && strcmp(argv[argc - 1], "expect-reject") == 0;
    if (bad_data) {
        mp_lockstep_test_alter_game_data();
    }
    int leaver = is_join && argc >= 7 && strcmp(argv[6], "leave") == 0;
    time_t pause_start = 0;
    int paused_seen = 0, ticks_while_paused = 0, tick_at_pause = -1;
    int ok = is_host ? mp_lockstep_host(atoi(argv[3]), atoi(argv[4]), argv[5], cities || generate)
                     : mp_lockstep_join(argv[3], atoi(argv[4]));
    if (ok && generate) {
        mp_lockstep_set_generated_map(1, 5);
    }
    if (!ok) {
        printf("FAILED: %s\n", mp_lockstep_status());
        return 1;
    }
    setting_reset_speeds(500, setting_scroll_speed());
    time_t deadline = time(0) + 120;
    int start_tick = -1;
    int start_tax = 0;
    int last_played = -1;
    while (time(0) < deadline) {
        mp_lockstep_state state = mp_lockstep_get_state();
        if (state == MP_LOCKSTEP_DESYNC || state == MP_LOCKSTEP_DISCONNECTED) {
            break;
        }
        if (expect_reject && strstr(mp_lockstep_status(), "refusé")) {
            printf("status: %s\n", mp_lockstep_status());
            mp_lockstep_stop();
            return 0;
        }
        if (state == MP_LOCKSTEP_RUNNING && start_tick < 0) {
            start_tick = mp_lockstep_base_tick();
            mp_lockstep_set_tick_limit(ticks);
            start_tax = city_finance_tax_percentage();
        }
        int before = game_time_absolute_tick();
        if (start_tick >= 0) {
            int tick_in_game = before - start_tick;
            if (tick_in_game >= ticks || (leaver && tick_in_game >= ticks / 2)) {
                break;
            }
            if (mp_lockstep_is_paused()) {
                // turns issued before the pause still run, nothing after them
                if (tick_at_pause < 0) {
                    tick_at_pause = tick_in_game;
                }
                paused_seen = 1;
                if (tick_in_game - tick_at_pause > ticks_while_paused) {
                    ticks_while_paused = tick_in_game - tick_at_pause;
                }
                if (pauser && time(0) - pause_start >= 1) {
                    mp_lockstep_request_pause(0);
                }
            } else {
                tick_at_pause = -1;
            }
            if (pauser && !pause_start && tick_in_game >= ticks / 2) {
                pause_start = time(0);
                mp_lockstep_request_pause(1);
            }
            // every tick passed since the last frame: a client catching up runs several in a frame
            for (int t = last_played + 1; t <= tick_in_game; t++) {
                mpnode_play(t);
                if (cheat && t == ticks / 2) {
                    city_finance_change_tax_percentage(3); // changed on this computer only
                }
            }
            last_played = tick_in_game;
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
        if (is_host) {
            usleep(500 * 1000); // let the clients read the notice before the connection closes
        }
        mp_lockstep_stop();
        return detected ? 0 : 1;
    }
    if (bad_data) {
        printf("status: %s\n", mp_lockstep_status());
        return strstr(mp_lockstep_status(), "Refusé") ? 0 : 1;
    }
    if (leaver) {
        printf("left the game at tick %d\n", game_time_absolute_tick() - start_tick);
        mp_lockstep_stop();
        return 0;
    }
    printf("tick %d checksum %016" PRIx64 "\n", start_tick >= 0 ? game_time_absolute_tick() - start_tick : -1,
        mp_checksum_state());
    printf("pause seen: %d, ticks run while paused: %d\n", paused_seen, ticks_while_paused);
    int result = mp_lockstep_get_state() == MP_LOCKSTEP_RUNNING && start_tick >= 0 &&
        game_time_absolute_tick() - start_tick == ticks;
    if (player_context_num_players() > 1) {
        // separate cities: the interface shows the local city, and every player changed only its own taxes
        int local = mp_session_local_player_id();
        if (player_context_current() != local) {
            printf("WRONG CITY: the interface shows city %d instead of %d\n", player_context_current(), local);
            result = 0;
        }
        for (int p = 0; p < player_context_num_players(); p++) {
            player_context_switch(p);
            int tax = city_finance_tax_percentage();
            int expected = p % 2 ? tax > start_tax : tax < start_tax;
            printf("city %d: tax %d%% (start %d%%), treasury %d%s\n", p, tax, start_tax, city_finance_treasury(),
                expected ? "" : " UNEXPECTED");
            result = result && expected;
        }
        player_context_switch(local);
    }
    if (is_host) {
        result = result && mp_lockstep_last_verified_turn() == (ticks / 4) - 1;
    }
    // a pause stops every computer within the turns already issued (2 turns of 4 ticks)
    if (ticks_while_paused > 8) {
        printf("WRONG: %d ticks ran during the pause\n", ticks_while_paused);
        result = 0;
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

#define TWIN_GAP MP_COMPOSE_CITY_GAP // farther than desirability and herds reach

typedef struct {
    int count;
    int done;
    uint64_t hash[MAX_PIECES];
    const char *name[MAX_PIECES];
} fingerprint;

static void record_city_piece(const char *name, uint64_t checksum, void *userdata)
{
    fingerprint *fp = userdata;
    // the pieces of the first city come first and end with its extra state
    if (fp->done || strcmp(name, "extra_state") == 0) {
        fp->done = 1;
        return;
    }
    // grids are compared on the area of the city; the header holds the number of cities
    if (strstr(name, "grid") || strcmp(name, "mp_header") == 0) {
        return;
    }
    if (fp->count < MAX_PIECES) {
        fp->name[fp->count] = name;
        fp->hash[fp->count++] = checksum;
    }
}

// both cities on one large map: the city on top, the copy (if any) below, water and forest between
static int setup_twin_map(const char *file, int num_cities, int *width, int *height)
{
    if (!load(file)) {
        return 0;
    }
    *width = map_data.width;
    *height = map_data.height;
    // each city has the same surroundings: TWIN_GAP tiles of rock on every side; the twin is on the diagonal
    return mp_compose_separate_cities(num_cities, TWIN_GAP);
}

typedef struct {
    int population, treasury, culture, prosperity, peace, favor, employed, unemployment, sentiment, health;
} city_stats;

static void get_stats(city_stats *s)
{
    s->population = city_population();
    s->treasury = city_finance_treasury();
    s->culture = city_rating_culture();
    s->prosperity = city_rating_prosperity();
    s->peace = city_rating_peace();
    s->favor = city_rating_favor();
    s->employed = city_labor_workers_employed();
    s->unemployment = city_labor_unemployment_percentage();
    s->sentiment = city_sentiment();
    s->health = city_health();
}

static void print_stats(const char *label, const city_stats *s)
{
    printf("  %-6s population %d, treasury %d, culture %d, prosperity %d, peace %d, favor %d, employed %d, "
        "unemployment %d%%, sentiment %d, health %d\n", label, s->population, s->treasury, s->culture, s->prosperity,
        s->peace, s->favor, s->employed, s->unemployment, s->sentiment, s->health);
}

// Caesar in a game (M4.1): requests, anger, invasions, distant battles, emperor changes, salary
typedef struct {
    int requests, anger, imperial_soldiers, distant_battles, salary;
} caesar_events;

static uint64_t requests_state(void)
{
    uint64_t hash = 0;
    for (int i = 0; i < MAX_REQUESTS; i++) {
        const scenario_request *r = scenario_request_get(i);
        hash = hash * 31 + r->state * 1000 + r->months_to_comply;
    }
    return hash;
}

static void run_caesar_game(const char *file, int ticks, int multiplayer, caesar_events *events)
{
    options.multiplayer = multiplayer;
    memset(events, 0, sizeof(*events));
    if (!load(file)) {
        return;
    }
    // an angry Caesar, and a salary
    city_ratings_change_favor(-100);
    city_data.emperor.salary_amount = 50;
    uint64_t requests = requests_state();
    int warnings = city_data.emperor.invasion.warnings_given;
    int savings = city_data.emperor.personal_savings;
    int battle = city_data.distant_battle.months_until_battle;
    setting_reset_speeds(500, setting_scroll_speed());
    for (int t = 0; t < ticks; t++) {
        run_one_tick();
        if (city_data.figure.imperial_soldiers > events->imperial_soldiers) {
            events->imperial_soldiers = city_data.figure.imperial_soldiers;
        }
    }
    events->requests = requests_state() != requests;
    events->anger = city_data.emperor.invasion.warnings_given - warnings;
    events->salary = city_data.emperor.personal_savings - savings;
    events->distant_battles = city_data.distant_battle.months_until_battle != battle;
    options.multiplayer = 0;
}

static int command_caesarfree(const char *file, int ticks)
{
    caesar_events classic, multiplayer;
    run_caesar_game(file, ticks, 0, &classic);
    run_caesar_game(file, ticks, 1, &multiplayer);
    const caesar_events *e[2] = { &classic, &multiplayer };
    for (int m = 0; m < 2; m++) {
        printf("%-11s requests changed %d, anger warnings %d, imperial soldiers %d, distant battle %d, salary %d\n",
            m ? "multiplayer" : "classic", e[m]->requests, e[m]->anger, e[m]->imperial_soldiers,
            e[m]->distant_battles, e[m]->salary);
    }
    int classic_has_caesar = classic.requests || classic.anger || classic.imperial_soldiers || classic.salary;
    int multiplayer_has_caesar = multiplayer.requests || multiplayer.anger || multiplayer.imperial_soldiers ||
        multiplayer.distant_battles || multiplayer.salary;
    if (!classic_has_caesar) {
        printf("FAILED: Caesar does nothing in the classic game either, the test proves nothing\n");
        return 1;
    }
    printf("%s\n", multiplayer_has_caesar ? "DIFFERENT: Caesar acts in the multiplayer game" :
        "Identical: Caesar acts in the classic game only");
    return multiplayer_has_caesar ? 1 : 0;
}

static int command_twins(const char *file, int ticks)
{
    static fingerprint alone, twin;
    int width, height;
    memset(&alone, 0, sizeof(alone));
    memset(&twin, 0, sizeof(twin));

    if (!setup_twin_map(file, 1, &width, &height)) {
        printf("Unable to prepare the map\n");
        return 2;
    }
    run_trace(ticks, ticks, 0, 0);
    mp_checksum_state_pieces(record_city_piece, &alone);
    game_file_write_saved_game("twins-alone.sav"); // entity records are comparable, large grids are cut
    uint64_t alone_area = mp_compose_region_checksum(TWIN_GAP, TWIN_GAP, width, height);
    city_stats alone_stats;
    get_stats(&alone_stats);

    if (!setup_twin_map(file, 2, &width, &height)) {
        printf("Unable to create the twin city\n");
        return 2;
    }
    run_trace(ticks, ticks, 0, 0);
    mp_checksum_state_pieces(record_city_piece, &twin);
    game_file_write_saved_game("twins-city.sav");
    uint64_t twin_area = mp_compose_region_checksum(TWIN_GAP, TWIN_GAP, width, height);
    city_stats first_stats, second_stats;
    get_stats(&first_stats);
    player_context_switch(1);
    get_stats(&second_stats);
    player_context_switch(0);
    player_context_set_num_players(1);

    int failures = 0;
    for (int i = 0; i < alone.count && i < twin.count; i++) {
        if (alone.hash[i] != twin.hash[i]) {
            printf("city differs from the city alone: %s\n", alone.name[i]);
            failures++;
        }
    }
    if (alone_area != twin_area) {
        printf("city differs from the city alone: map grids of its area\n");
        failures++;
    }
    if (memcmp(&first_stats, &second_stats, sizeof(city_stats)) != 0) {
        printf("the twin city has different statistics\n");
        failures++;
    }
    print_stats("alone", &alone_stats);
    print_stats("city", &first_stats);
    print_stats("twin", &second_stats);
    printf("%s after %d ticks\n", failures ? "DIFFERENT" : "Identical", ticks);
    return failures ? 1 : 0;
}

extern int stub_city_message_popups[2];
extern int stub_sounds_played[2];

// every computer runs every city: the popups and sounds of the twin (player 2) never reach player 1
static int command_privatepopups(const char *file, int ticks)
{
    int width, height;
    if (!setup_twin_map(file, 2, &width, &height)) {
        printf("Unable to create the twin city\n");
        return 2;
    }
    memset(stub_city_message_popups, 0, sizeof(stub_city_message_popups));
    memset(stub_sounds_played, 0, sizeof(stub_sounds_played));
    run_trace(ticks, ticks, 0, 0);
    player_context_set_num_players(1);
    printf("own city: %d popups, %d sounds; other player's city: %d popups, %d sounds\n",
        stub_city_message_popups[0], stub_sounds_played[0], stub_city_message_popups[1], stub_sounds_played[1]);
    if (!stub_city_message_popups[0]) {
        printf("FAILED: no popup in the own city either, the test proves nothing\n");
        return 1;
    }
    if (stub_city_message_popups[1] || stub_sounds_played[1]) {
        printf("DIFFERENT: the city of the other player shows its popups or plays its sounds here\n");
        return 1;
    }
    printf("Identical: only the own city shows popups and plays sounds\n");
    return 0;
}

// first figure of the twin that is not the copy of its counterpart in the first city
static int find_twin_figure_difference(const player_clone *c)
{
    for (int i = 1; i < MAX_FIGURES; i++) {
        figure expected;
        figure_clone_record(&expected, figure_get(i), MAX_FIGURES + i, c);
        figure *actual = figure_get(MAX_FIGURES + i);
        if (!expected.state && !actual->state) {
            continue;
        }
        if (memcmp(&expected, actual, sizeof(figure)) != 0) {
            const unsigned char *a = (const unsigned char *) &expected;
            const unsigned char *b = (const unsigned char *) actual;
            printf("  figure %d (type %d, action %d): first different byte at struct offset %d\n",
                i, expected.type, expected.action_state, (int) 0);
            for (int k = 0; k < (int) sizeof(figure); k++) {
                if (a[k] != b[k]) {
                    printf("    offset %d: expected %d, twin %d\n", k, a[k], b[k]);
                }
            }
            printf("    expected: state %d action %d xy %d,%d dest %d,%d path %d wait %d building %d formation %d\n",
                expected.state, expected.action_state, expected.x, expected.y, expected.destination_x,
                expected.destination_y, expected.routing_path_id, expected.wait_ticks, expected.building_id,
                expected.formation_id);
            if (getenv("TWIN_DEBUG_FIGURE")) {
                int id = atoi(getenv("TWIN_DEBUG_FIGURE"));
                for (int p = 0; p < 2; p++) {
                    figure *g = figure_get(id + p * MAX_FIGURES);
                    printf("    figure %d: type %d state %d action %d xy %d,%d next %d\n", g->id, g->type, g->state,
                        g->action_state, g->x, g->y, g->next_figure_id_on_same_tile);
                }
            }
            printf("    twin:     state %d action %d xy %d,%d dest %d,%d path %d wait %d building %d formation %d\n",
                actual->state, actual->action_state, actual->x, actual->y, actual->destination_x,
                actual->destination_y, actual->routing_path_id, actual->wait_ticks, actual->building_id,
                actual->formation_id);
            return 1;
        }
    }
    return 0;
}

static int find_twin_building_difference(const player_clone *c)
{
    for (int i = 1; i < MAX_BUILDINGS; i++) {
        building expected;
        building_clone_record(&expected, building_get(i), MAX_BUILDINGS + i, c);
        building *actual = building_get(MAX_BUILDINGS + i);
        if (expected.state == BUILDING_STATE_UNUSED && actual->state == BUILDING_STATE_UNUSED) {
            continue;
        }
        // road network numbers are numbered across the whole map: only equality between buildings matters
        expected.road_network_id = actual->road_network_id;
        if (memcmp(&expected, actual, sizeof(building)) != 0) {
            const unsigned char *a = (const unsigned char *) &expected;
            const unsigned char *b = (const unsigned char *) actual;
            printf("  building %d (type %d): differing bytes at struct offsets", i, expected.type);
            for (int k = 0; k < (int) sizeof(building); k++) {
                if (a[k] != b[k]) {
                    printf(" %d (%d/%d)", k, a[k], b[k]);
                }
            }
            printf("\n");
            return 1;
        }
    }
    return 0;
}

// Connected neighbours (M4.3): two copies of a city side by side on land, joined by a road built by the
// second player. Walkers cross over, but no city acts on the buildings of the other (mp/audit)
static int find_edge_road(int x_min, int x_max, int y_min, int y_max, int rightmost, int *x_road, int *y_road)
{
    int found = 0;
    for (int y = y_min; y <= y_max; y++) {
        for (int x = x_min; x <= x_max; x++) {
            int offset = map_grid_offset(x, y);
            if (map_terrain_is(offset, TERRAIN_ROAD) && !map_terrain_is(offset, TERRAIN_BUILDING) &&
                (!found || (rightmost ? x > *x_road : x < *x_road))) {
                *x_road = x;
                *y_road = y;
                found = 1;
            }
        }
    }
    return found;
}

#define NEIGHBOUR_GAP 4
#define NEIGHBOUR_ROADS 6

// a road from a road of the first city to the nearest reachable road of the second, built by the second player
static int connect_cities(int width, int height, int shift, int attempt)
{
    int xa, ya;
    if (!find_edge_road(TWIN_GAP, TWIN_GAP + width - 1 - attempt, TWIN_GAP, TWIN_GAP + height - 1, 1, &xa, &ya) ||
        !map_routing_calculate_distances_for_building(ROUTED_BUILDING_ROAD, xa, ya)) {
        return 0;
    }
    int x1 = 0, y1 = 0, best = 0;
    for (int y = TWIN_GAP; y < TWIN_GAP + height; y++) {
        for (int x = TWIN_GAP + shift; x < TWIN_GAP + shift + width; x++) {
            int offset = map_grid_offset(x, y);
            int distance = map_routing_distance(offset);
            if (map_terrain_is(offset, TERRAIN_ROAD) && distance > 1 && (!best || distance < best)) {
                best = distance;
                x1 = x;
                y1 = y;
            }
        }
    }
    if (!best) {
        return 0;
    }
    mp_command command = { .type = MP_COMMAND_BUILD, .player_id = 1, .args = { BUILDING_ROAD, 0, xa, ya, x1, y1, 0, 0 } };
    mp_command_execute(&command);
    map_road_network_update(); // done by the next daily update in a game
    return map_road_network_get(map_grid_offset(xa, ya)) == map_road_network_get(map_grid_offset(x1, y1));
}

static int command_neighbours(const char *file, int ticks)
{
    if (!load(file)) {
        return 2;
    }
    int width = map_data.width;
    int height = map_data.height;
    int shift = width + NEIGHBOUR_GAP;
    if (!mp_compose_relocate(GRID_MAX_SIZE, TWIN_GAP, TWIN_GAP) ||
        !mp_compose_extend_map(TWIN_GAP, TWIN_GAP, shift + TWIN_GAP, TWIN_GAP) ||
        !mp_compose_add_twin(TWIN_GAP, TWIN_GAP, width, height, shift, 0)) {
        printf("Unable to compose the neighbours\n");
        return 2;
    }
    // land between the two cities
    for (int y = TWIN_GAP; y < TWIN_GAP + height; y++) {
        for (int x = TWIN_GAP + width; x < TWIN_GAP + shift; x++) {
            map_terrain_set(map_grid_offset(x, y), 0);
        }
    }
    map_routing_update_all();
    player_context_switch(1);
    city_finance_process_donation(100000);
    player_context_switch(0);
    int roads = 0;
    for (int attempt = 0; attempt < 120 && roads < NEIGHBOUR_ROADS; attempt += 3) {
        roads += connect_cities(width, height, shift, attempt);
    }
    printf("roads joining the cities: %d\n", roads);
    if (!roads) {
        printf("No road of the second city can be reached from the first one\n");
        return 2;
    }

    mp_audit_reset();
    setting_reset_speeds(500, setting_scroll_speed());
    int crossings[2] = { 0, 0 };
    for (int tick = 0; tick < ticks; tick++) {
        run_one_tick();
        for (int i = 1; i < 2 * MAX_FIGURES; i++) {
            figure *f = figure_get(i);
            if (f->state == FIGURE_STATE_ALIVE && !figure_is_herd(f)) {
                int owner = i / MAX_FIGURES;
                int in_other_city = owner == 0 ? f->x >= TWIN_GAP + shift : f->x < TWIN_GAP + width;
                crossings[owner] += in_other_city;
            }
        }
    }
    printf("walkers in the other city (figure ticks): %d from the first city, %d from the second\n",
        crossings[0], crossings[1]);
    printf("effects on the buildings of the other player: %d%s%s\n", mp_audit_violations(),
        mp_audit_violations() ? ", first: " : "", mp_audit_violations() ? mp_audit_first_kind() : "");
    if (!crossings[0] && !crossings[1]) {
        printf("FAILED: nobody crossed, the test proves nothing\n");
        return 1;
    }
    return mp_audit_violations() ? 1 : 0;
}

// Intruders (M4.3): the walkers of the twin city are moved to the same place in the first city, an exact copy
// of theirs: they walk its roads like their twins, but no effect of theirs may reach its buildings
static int command_intruders(const char *file, int ticks)
{
    int width, height;
    if (!setup_twin_map(file, 2, &width, &height)) {
        return 2;
    }
    int shift = (width > height ? width : height) + TWIN_GAP;
    setting_reset_speeds(500, setting_scroll_speed());
    run_trace(50, 50, 0, 0);
    int moved = 0;
    for (int i = MAX_FIGURES + 1; i < 2 * MAX_FIGURES; i++) {
        figure *f = figure_get(i);
        if (f->state != FIGURE_STATE_ALIVE || figure_is_herd(f) || !f->grid_offset) {
            continue;
        }
        map_figure_delete(f);
        f->x -= shift;
        f->y -= shift;
        f->previous_tile_x -= shift;
        f->previous_tile_y -= shift;
        f->cross_country_x -= 15 * shift;
        f->cross_country_y -= 15 * shift;
        f->grid_offset = map_grid_offset(f->x, f->y);
        map_figure_add(f);
        moved++;
    }
    mp_audit_reset();
    run_trace(ticks, ticks, 0, 0);
    printf("walkers moved into the first city: %d\n", moved);
    printf("effects on the buildings of the other player: %d%s%s\n", mp_audit_violations(),
        mp_audit_violations() ? ", first: " : "", mp_audit_violations() ? mp_audit_first_kind() : "");
    player_context_switch(0);
    player_context_set_num_players(1);
    if (!moved) {
        printf("FAILED: no walker to move, the test proves nothing\n");
        return 1;
    }
    return mp_audit_violations() ? 1 : 0;
}

// Network game map (M4.4): the copies of the city are separated by land, where a road can join them
static int command_openland(const char *file)
{
    int width, height;
    if (!setup_twin_map(file, 2, &width, &height)) {
        return 2;
    }
    mp_compose_open_land_between_cities(TWIN_GAP);
    int x0, y0, size;
    mp_compose_city_area(0, TWIN_GAP, &x0, &y0, &size);
    int x1, y1;
    mp_compose_city_area(1, TWIN_GAP, &x1, &y1, &size);
    // from the corner of the first city to the corner of the second one, through the land around them
    int xa = x0 + size, ya = y0 + size, xb = x1 - 1, yb = y1 - 1;
    player_context_switch(1);
    city_finance_process_donation(100000);
    player_context_switch(0);
    mp_command command = { .type = MP_COMMAND_BUILD, .player_id = 1, .args = { BUILDING_ROAD, 0, xa, ya, xb, yb, 0, 0 } };
    mp_command_execute(&command);
    int built = map_terrain_is(map_grid_offset(xa, ya), TERRAIN_ROAD) &&
        map_terrain_is(map_grid_offset(xb, yb), TERRAIN_ROAD);
    int owner = map_owner_get_claimed(map_grid_offset(xa, ya));
    printf("road from (%d, %d) to (%d, %d) between the cities: %s, owned by player %d\n", xa, ya, xb, yb,
        built ? "built" : "NOT built", owner + 1);
    player_context_set_num_players(1);
    return built && owner == 1 ? 0 : 1;
}

// AI invasions option (M4.5): enemy armies come in a multiplayer game only when the rules allow them
static int count_new_enemies(const char *file, int ticks, int ai_invasions)
{
    if (!load(file)) {
        return -1;
    }
    game_rules_settings rules;
    game_rules_default_multiplayer_settings(&rules);
    rules.ai_invasions = ai_invasions;
    game_rules_set_multiplayer(&rules);
    static unsigned char present[MAX_FIGURES];
    for (int i = 1; i < MAX_FIGURES; i++) {
        figure *f = figure_get(i);
        present[i] = f->state == FIGURE_STATE_ALIVE && figure_is_enemy(f);
    }
    int new_enemies = 0;
    setting_reset_speeds(500, setting_scroll_speed());
    for (int tick = 0; tick < ticks; tick++) {
        run_one_tick();
        for (int i = 1; i < MAX_FIGURES; i++) {
            figure *f = figure_get(i);
            if (f->state == FIGURE_STATE_ALIVE && figure_is_enemy(f) && !present[i]) {
                present[i] = 1;
                new_enemies++;
            }
        }
    }
    return new_enemies;
}

static int command_aiinvasions(const char *file, int ticks)
{
    int with = count_new_enemies(file, ticks, 1);
    int without = count_new_enemies(file, ticks, 0);
    printf("new enemies with AI invasions: %d, without: %d\n", with, without);
    if (with <= 0) {
        printf("FAILED: no invasion in this game, the test proves nothing\n");
        return 1;
    }
    return without ? 1 : 0;
}

// End of the game by score (M4.6): two cities, the second one with higher taxes; the game ends at the start
// of the year start + YEARS, with a score for every city
static int command_endscore(const char *file, int years)
{
    int width, height;
    if (!setup_twin_map(file, 2, &width, &height)) {
        return 2;
    }
    game_rules_settings rules;
    game_rules_default_multiplayer_settings(&rules);
    rules.end_condition = GAME_END_SCORE;
    rules.score_years = years;
    game_rules_set_multiplayer(&rules);
    mp_endgame_reset();
    int end_year = game_time_year() + years;
    setting_reset_speeds(500, setting_scroll_speed());
    int ticks = 0;
    while (!mp_endgame_is_over() && ticks < 9600 * (years + 1)) {
        run_one_tick();
        ticks++;
    }
    int year = game_time_year();
    printf("over: %d after %d ticks, year %d (expected %d), scores %d / %d, winner: player %d\n", mp_endgame_is_over(),
        ticks, year, end_year, mp_endgame_score(0), mp_endgame_score(1), mp_endgame_winner() + 1);
    player_context_switch(0);
    player_context_set_num_players(1);
    return mp_endgame_is_over() && year == end_year && mp_endgame_score(0) > 0 ? 0 : 1;
}

// Permissions to exploit (M4.7, D-020): shared out between the cities, evaluated in each city's context
static int command_permissions(const char *file, int num_cities)
{
    int width, height;
    if (!setup_twin_map(file, num_cities, &width, &height)) {
        return 2;
    }
    static const struct { int building; int resource; const char *name; } RAW[] = {
        {BUILDING_IRON_MINE, RESOURCE_IRON, "iron"}, {BUILDING_CLAY_PIT, RESOURCE_CLAY, "clay"},
        {BUILDING_TIMBER_YARD, RESOURCE_TIMBER, "timber"}, {BUILDING_OLIVE_FARM, RESOURCE_OLIVES, "olives"},
        {BUILDING_VINES_FARM, RESOURCE_VINES, "vines"}, {BUILDING_MARBLE_QUARRY, RESOURCE_MARBLE, "marble"},
    };
    int originally[6];
    building_menu_update();
    for (int r = 0; r < 6; r++) {
        originally[r] = building_menu_is_enabled(RAW[r].building);
    }
    mp_permissions_share_out();
    int failures = 0;
    int holders[6] = { 0 };
    for (int p = 0; p < num_cities; p++) {
        player_context_switch(p);
        building_menu_update();
        printf("city %d:", p);
        for (int r = 0; r < 6; r++) {
            if (building_menu_is_enabled(RAW[r].building)) {
                printf(" %s", RAW[r].name);
                holders[r]++;
            }
        }
        int weapons = building_menu_is_enabled(BUILDING_WEAPONS_WORKSHOP);
        int iron_available = building_menu_is_enabled(BUILDING_IRON_MINE) ||
            empire_can_import_resource_potentially(RESOURCE_IRON);
        printf(", weapons workshop %s (iron %s)\n", weapons ? "yes" : "no", iron_available ? "available" : "none");
        if (weapons != iron_available) {
            printf("WRONG: weapons workshop without iron, or iron without workshop\n");
            failures++;
        }
    }
    player_context_switch(0);
    for (int r = 0; r < 6; r++) {
        if (originally[r] && holders[r] != 1) {
            printf("WRONG: %s may be produced by %d cities instead of one\n", RAW[r].name, holders[r]);
            failures++;
        }
    }
    player_context_set_num_players(1);
    return failures ? 1 : 0;
}

// A multiplayer saved game of CITIES copies of a city, after TICKS ticks (tests and manual games)
static int command_mpsave(const char *file, int num_cities, int ticks, const char *output)
{
    int width, height;
    if (!setup_twin_map(file, num_cities, &width, &height)) {
        return 2;
    }
    mp_compose_open_land_between_cities(TWIN_GAP);
    mp_permissions_share_out();
    run_trace(ticks, ticks, 0, 0);
    int ok = mp_savegame_write(output);
    printf("%s: %d cities after %d ticks%s\n", output, num_cities, ticks, ok ? "" : ", NOT written");
    player_context_switch(0);
    player_context_set_num_players(1);
    return ok ? 0 : 1;
}

// Generated maps (M6.2): every player gets an arrival point and land to settle; immigrants come to each
// city once its player has built a road and housing. The same seed gives the same map.
static void settle_city(int player_id, int size)
{
    int cx, cy;
    mp_mapgen_city_center(player_id, &cx, &cy);
    player_context_switch(player_id);
    const map_tile *entry = city_map_entry_point();
    int ex = entry->x, ey = entry->y;
    player_context_switch(0);
    mp_command road = { .type = MP_COMMAND_BUILD, .player_id = player_id, .args = { BUILDING_ROAD, 0, ex, ey, cx, cy, 0, 0 } };
    mp_command_execute(&road);
    mp_command road2 = { .type = MP_COMMAND_BUILD, .player_id = player_id, .args = { BUILDING_ROAD, 0, cx - 6, cy, cx + 6, cy, 0, 0 } };
    mp_command_execute(&road2);
    mp_command houses = { .type = MP_COMMAND_BUILD, .player_id = player_id,
        .args = { BUILDING_HOUSE_VACANT_LOT, 0, cx - 5, cy + 1, cx + 5, cy + 2, 0, 0 } };
    mp_command_execute(&houses);
    mp_command houses2 = { .type = MP_COMMAND_BUILD, .player_id = player_id,
        .args = { BUILDING_HOUSE_VACANT_LOT, 0, cx - 5, cy - 2, cx + 5, cy - 1, 0, 0 } };
    mp_command_execute(&houses2);
}

static int count_roads(int x_from, int x_to, int y)
{
    int roads = 0;
    for (int x = x_from; x <= x_to; x++) {
        roads += map_terrain_is(map_grid_offset(x, y), TERRAIN_ROAD) ? 1 : 0;
    }
    return roads;
}

// the roads of Caesar (D-034): no player may clear them, none is tinted, and every city may use them
static int command_caesarroads(const char *file)
{
    int size = mp_mapgen_default_size(2);
    if (!mp_mapgen_create(file, 2, size, 11)) {
        printf("Unable to generate the map\n");
        return 2;
    }
    int failures = 0;
    for (int p = 0; p < 2; p++) {
        int cx, cy;
        mp_mapgen_city_center(p, &cx, &cy);
        // a road of Caesar, as a map places it, and next to it a road the player builds
        mp_command build = { .type = MP_COMMAND_BUILD, .player_id = p, .args = { BUILDING_ROAD, 0, cx - 6, cy, cx + 6, cy, 0, 0 } };
        mp_command_execute(&build);
        for (int x = cx - 6; x <= cx + 6; x++) {
            map_owner_set(map_grid_offset(x, cy), MAP_OWNER_CAESAR);
        }
        mp_command own = { .type = MP_COMMAND_BUILD, .player_id = p, .args = { BUILDING_ROAD, 0, cx - 6, cy + 3, cx + 6, cy + 3, 0, 0 } };
        mp_command_execute(&own);
        int tinted = mp_colors_tint_for_tile(map_grid_offset(cx, cy)) != 0;
        // houses along the road of Caesar: the city reaches them by that road
        mp_command houses = { .type = MP_COMMAND_BUILD, .player_id = p,
            .args = { BUILDING_HOUSE_VACANT_LOT, 0, cx - 5, cy + 1, cx + 5, cy + 1, 0, 0 } };
        mp_command_execute(&houses);
        for (int q = 0; q < 2; q++) {
            mp_command clear = { .type = MP_COMMAND_BUILD, .player_id = q, .args = { BUILDING_CLEAR_LAND, 0, cx - 6, cy, cx + 6, cy, 0, 0 } };
            mp_command_execute(&clear);
        }
        mp_command clear_own = { .type = MP_COMMAND_BUILD, .player_id = p, .args = { BUILDING_CLEAR_LAND, 0, cx - 6, cy + 3, cx + 6, cy + 3, 0, 0 } };
        mp_command_execute(&clear_own);
        int caesar_roads = count_roads(cx - 6, cx + 6, cy);
        int own_roads = count_roads(cx - 6, cx + 6, cy + 3);
        printf("player %d: road of Caesar %d/13 tiles left, own road %d/13 tiles left, %s\n", p + 1, caesar_roads,
            own_roads, tinted ? "TINTED" : "not tinted");
        if (caesar_roads != 13 || tinted) {
            failures++;
        }
        if (own_roads != 0) {
            printf("FAILED: the player cannot clear his own road either, the test proves nothing\n");
            return 1;
        }
    }
    // a day of the game: the houses along the roads of Caesar get road access and their first immigrants
    run_trace(1000, 1000, 0, 0);
    for (int p = 0; p < 2; p++) {
        player_context_switch(p);
        printf("player %d: population %d\n", p + 1, city_population());
        if (!city_population()) {
            printf("the houses along the road of Caesar get no immigrant\n");
            failures++;
        }
    }
    player_context_switch(0);
    player_context_set_num_players(1);
    printf("%s\n", failures ? "DIFFERENT: the roads of Caesar are not neutral and permanent" :
        "Identical: the roads of Caesar stay, untinted, and serve every city");
    return failures ? 1 : 0;
}

// picture of a generated map, one square of `scale` pixels per tile (PPM), for Alexandre to look at the map
static void write_map_picture(const char *filename, int size, int scale)
{
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        return;
    }
    fprintf(fp, "P6\n%d %d\n255\n", size * scale, size * scale);
    for (int y = 0; y < size * scale; y++) {
        for (int x = 0; x < size * scale; x++) {
            int offset = map_grid_offset(x / scale, y / scale);
            int t = map_terrain_get(offset);
            unsigned char c[3] = { 150, 170, 90 };
            if (t & TERRAIN_ROAD) {
                int caesar = map_owner_get_claimed(offset) == MAP_OWNER_CAESAR;
                c[0] = caesar ? 240 : 140; c[1] = caesar ? 240 : 110; c[2] = caesar ? 240 : 80;
            } else if (t & TERRAIN_WATER) {
                c[0] = 50; c[1] = 90; c[2] = 200;
            } else if (t & TERRAIN_ROCK) {
                c[0] = 120; c[1] = 120; c[2] = 120;
            } else if (t & TERRAIN_TREE) {
                c[0] = 30; c[1] = 100; c[2] = 40;
            } else if (t & TERRAIN_MEADOW) {
                c[0] = 210; c[1] = 200; c[2] = 90;
            }
            fwrite(c, 1, 3, fp);
        }
    }
    fclose(fp);
    printf("%s written\n", filename);
}

// land that roads may reach from the first arrival point: everything but water and rock (trees can be cleared)
static int count_unreachable_land(int size)
{
    static uint8_t seen[GRID_MAX_SIZE * GRID_MAX_SIZE];
    static int queue[GRID_MAX_SIZE * GRID_MAX_SIZE];
    memset(seen, 0, sizeof(seen));
    int ex, ey;
    mp_mapgen_entry_point(0, &ex, &ey);
    int head = 0, tail = 0;
    queue[tail++] = ey * size + ex;
    seen[ey * size + ex] = 1;
    while (head < tail) {
        int i = queue[head++];
        int x = i % size, y = i / size;
        static const int DX[] = { 1, -1, 0, 0 };
        static const int DY[] = { 0, 0, 1, -1 };
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d], ny = y + DY[d];
            if (nx < 0 || ny < 0 || nx >= size || ny >= size || seen[ny * size + nx] ||
                map_terrain_is(map_grid_offset(nx, ny), TERRAIN_WATER | TERRAIN_ROCK)) {
                continue;
            }
            seen[ny * size + nx] = 1;
            queue[tail++] = ny * size + nx;
        }
    }
    int unreachable = 0;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            if (!seen[y * size + x] && !map_terrain_is(map_grid_offset(x, y), TERRAIN_WATER | TERRAIN_ROCK)) {
                unreachable++;
            }
        }
    }
    return unreachable;
}

static int count_terrain_near(int cx, int cy, int radius, int terrain)
{
    int count = 0;
    for (int y = cy - radius; y <= cy + radius; y++) {
        for (int x = cx - radius; x <= cx + radius; x++) {
            count += map_terrain_is(map_grid_offset(x, y), terrain) ? 1 : 0;
        }
    }
    return count;
}

// the prepared maps (D-033): all land reached from the main road, each arrival point with its materials only,
// permissions that match, a main road of Caesar, the same map every time, and cities that grow
static int command_preparedmap(const char *file, int num_players, int ticks)
{
    if (!mp_mapgen_create_prepared(file, num_players)) {
        printf("Unable to create the prepared map\n");
        return 2;
    }
    int size = mp_mapgen_prepared_size(num_players);
    if (getenv("MAPGEN_PICTURE")) {
        write_map_picture(getenv("MAPGEN_PICTURE"), size, 3);
    }
    int failures = 0;
    int unreachable = count_unreachable_land(size);
    printf("land out of reach of the main road: %d tiles\n", unreachable);
    failures += unreachable != 0;
    for (int p = 0; p < num_players; p++) {
        int cx, cy;
        mp_mapgen_city_center(p, &cx, &cy);
        int water = count_terrain_near(cx, cy, 26, TERRAIN_WATER);
        int rock = count_terrain_near(cx, cy, 26, TERRAIN_ROCK);
        int trees = count_terrain_near(cx, cy, 26, TERRAIN_TREE);
        int meadow = count_terrain_near(cx, cy, 26, TERRAIN_MEADOW);
        int wants_rock = mp_mapgen_slot_allows(p, RESOURCE_IRON) || mp_mapgen_slot_allows(p, RESOURCE_MARBLE);
        int wants_trees = mp_mapgen_slot_allows(p, RESOURCE_TIMBER);
        int wants_water = mp_mapgen_slot_has_water(p) || mp_mapgen_slot_allows(p, RESOURCE_CLAY);
        printf("player %d: water %d, rock %d, trees %d, meadow %d\n", p + 1, water, rock, trees, meadow);
        if ((water > 0) != wants_water || (rock > 0) != wants_rock || (trees > 0) != wants_trees || meadow < 50) {
            printf("  the land around player %d does not match its materials\n", p + 1);
            failures++;
        }
        player_context_switch(p);
        static const int MATERIALS[] = { RESOURCE_IRON, RESOURCE_CLAY, RESOURCE_TIMBER, RESOURCE_OLIVES,
            RESOURCE_VINES, RESOURCE_MARBLE };
        for (int i = 0; i < 6; i++) {
            if (empire_city_our_production_allowed(MATERIALS[i]) != mp_mapgen_slot_allows(p, MATERIALS[i])) {
                printf("  player %d: permission for material %d does not match the map\n", p + 1, MATERIALS[i]);
                failures++;
            }
        }
        player_context_switch(0);
        int ex, ey;
        mp_mapgen_entry_point(p, &ex, &ey);
        int entry = map_grid_offset(ex, ey);
        if (!map_terrain_is(entry, TERRAIN_ROAD) || map_owner_get_claimed(entry) != MAP_OWNER_CAESAR) {
            printf("  player %d: no road of Caesar at the arrival point\n", p + 1);
            failures++;
        }
    }
    // ships of the empire sail from the edge of the map to the central lake, and to the lake of each player who has
    // one; caravans come by the main road (checked above)
    int rx, ry;
    mp_mapgen_river_point(&rx, &ry);
    map_routing_calculate_distances_water_boat(rx, ry);
    int lake = map_grid_offset(size / 2 + 14, size / 2 - 14);
    int sailing = scenario_map_has_river_entry() && map_routing_distance(lake) > 0;
    printf("river from (%d, %d) to the central lake: %s\n", rx, ry, sailing ? "ships sail" : "NO WAY");
    failures += !sailing;
    for (int p = 0; p < num_players; p++) {
        if (!mp_mapgen_slot_has_water(p)) {
            continue;
        }
        int px, py, cx, cy, reached = 0;
        mp_mapgen_player_river_point(p, &px, &py);
        mp_mapgen_city_center(p, &cx, &cy);
        map_routing_calculate_distances_water_boat(px, py);
        for (int y = cy - 26; y <= cy + 26 && !reached; y++) {
            for (int x = cx - 26; x <= cx + 26 && !reached; x++) {
                int o = map_grid_offset(x, y);
                reached = map_terrain_is(o, TERRAIN_WATER) && map_routing_distance(o) > 0;
            }
        }
        player_context_switch(p);
        int entry_ok = scenario_map_river_entry().x == px && scenario_map_river_entry().y == py;
        player_context_switch(0);
        printf("player %d: ships from (%d, %d) %s\n", p + 1, px, py,
            reached && entry_ok ? "sail to his lake" : "DO NOT REACH HIS LAKE");
        failures += !reached || !entry_ok;
    }

    uint64_t first = mp_checksum_state();
    player_context_switch(0);
    player_context_set_num_players(1);
    if (!mp_mapgen_create_prepared(file, num_players) || mp_checksum_state() != first) {
        printf("DIFFERENT: the prepared map changed between two creations\n");
        return 1;
    }
    // as in a network game, the map goes through a file
    char map_file[64];
    snprintf(map_file, sizeof(map_file), "prepared-%d.mpmap", num_players);
    if (!mp_savegame_write(map_file) || !mp_savegame_read(map_file)) {
        printf("Unable to write and read the map\n");
        return 2;
    }
    remove(map_file);

    // the players without water build a reservoir at the end of the aqueduct of Caesar: it fills; another one,
    // away from any water, stays dry
    for (int p = 0; p < num_players; p++) {
        int ax, ay;
        if (!mp_mapgen_caesar_aqueduct_end(p, &ax, &ay)) {
            continue;
        }
        int cx, cy;
        mp_mapgen_city_center(p, &cx, &cy);
        int ex, ey;
        mp_mapgen_entry_point(p, &ex, &ey);
        int west = ex == 0; // the west branch runs along a row, the north one along a column; its end touches
                            // the middle of a side of the reservoir
        int x = west ? ax - 2 : ax;
        int y = west ? ay : ay - 2;
        mp_command fed = { .type = MP_COMMAND_BUILD, .player_id = p, .args = { BUILDING_DRAGGABLE_RESERVOIR, 0, x, y, x, y, 0, 0 } };
        mp_command_execute(&fed);
        mp_command dry = { .type = MP_COMMAND_BUILD, .player_id = p, .args = { BUILDING_DRAGGABLE_RESERVOIR, 0, cx - 6, cy - 6, cx - 6, cy - 6, 0, 0 } };
        mp_command_execute(&dry);
        player_context_switch(p);
        map_water_supply_update_reservoir_fountain();
        building *fed_reservoir = building_get(map_building_at(map_grid_offset(x, y)));
        building *dry_reservoir = building_get(map_building_at(map_grid_offset(cx - 6, cy - 6)));
        player_context_switch(0);
        int fed_ok = fed_reservoir->type == BUILDING_RESERVOIR && fed_reservoir->has_water_access;
        int dry_ok = dry_reservoir->type == BUILDING_RESERVOIR && !dry_reservoir->has_water_access;
        printf("player %d: reservoir on the aqueduct of Caesar %s, reservoir away from water %s\n", p + 1,
            fed_ok ? "has water" : "HAS NO WATER", dry_ok ? "dry" : "NOT DRY OR NOT BUILT");
        failures += !fed_ok || !dry_ok;
    }

    for (int p = 0; p < num_players; p++) {
        settle_city(p, size);
    }
    map_road_network_update_grid();
    for (int p = 0; p < num_players; p++) {
        player_context_switch(p);
        map_road_network_update_largest();
    }
    player_context_switch(0);
    run_trace(ticks, ticks, 0, 0);
    for (int p = 0; p < num_players; p++) {
        player_context_switch(p);
        printf("player %d: population %d\n", p + 1, city_population());
        failures += city_population() == 0;
    }
    player_context_switch(0);
    player_context_set_num_players(1);
    printf("%s\n", failures ? "DIFFERENT: the prepared map is not as designed" :
        "Identical: the prepared map is as designed");
    return failures ? 1 : 0;
}

static int reservoir_at(int x, int y, int *level)
{
    int id = map_building_at(map_grid_offset(x, y));
    building *b = building_get(id);
    *level = b->type == BUILDING_RESERVOIR ? map_water_supply_reservoir_level(id) : -1;
    return b->type == BUILDING_RESERVOIR && b->has_water_access;
}

// reservoirs hold water (ME.1, D-035): cut off from its source, a reservoir keeps serving until it is empty, about
// 270 days; joined again, it fills in about 54 days
static int command_reservoirlevel(const char *file)
{
    if (!mp_mapgen_create_prepared(file, 2)) {
        printf("Unable to create the prepared map\n");
        return 2;
    }
    int ax, ay;
    if (!mp_mapgen_caesar_aqueduct_end(0, &ax, &ay)) {
        printf("No aqueduct of Caesar for player 1\n");
        return 2;
    }
    // no enemy army comes to destroy the reservoirs
    game_rules_settings rules;
    game_rules_default_multiplayer_settings(&rules);
    rules.ai_invasions = 0;
    game_rules_set_multiplayer(&rules);
    // R1 on the aqueduct of Caesar, an aqueduct of player 1 westwards, R2 at its end
    mp_command r1 = { .type = MP_COMMAND_BUILD, .player_id = 0, .args = { BUILDING_DRAGGABLE_RESERVOIR, 0, ax - 2, ay, ax - 2, ay, 0, 0 } };
    mp_command aqueduct = { .type = MP_COMMAND_BUILD, .player_id = 0, .args = { BUILDING_AQUEDUCT, 0, ax - 4, ay, ax - 10, ay, 0, 0 } };
    mp_command r2 = { .type = MP_COMMAND_BUILD, .player_id = 0, .args = { BUILDING_DRAGGABLE_RESERVOIR, 0, ax - 12, ay, ax - 12, ay, 0, 0 } };
    mp_command cut = { .type = MP_COMMAND_BUILD, .player_id = 0, .args = { BUILDING_CLEAR_LAND, 0, ax - 7, ay, ax - 7, ay, 0, 0 } };
    mp_command join = { .type = MP_COMMAND_BUILD, .player_id = 0, .args = { BUILDING_AQUEDUCT, 0, ax - 7, ay, ax - 7, ay, 0, 0 } };
    mp_command_execute(&r1);
    mp_command_execute(&aqueduct);
    mp_command_execute(&r2);
    int full = map_water_supply_reservoir_level_full();
    int failures = 0, level1, level2, water1, water2;
    struct { int days; mp_command *before; int r2_water; int r2_full; const char *label; } steps[] = {
        { 2, 0, 1, 0, "joined, 2 days later" },
        { 60, 0, 1, 1, "60 days later" },
        { 100, &cut, 1, 0, "cut off, 100 days later" },
        { 200, 0, 0, 0, "300 days after the cut" },
        { 3, &join, 1, 0, "joined again, 3 days later" },
    };
    for (int i = 0; i < 5; i++) {
        if (steps[i].before) {
            mp_command_execute(steps[i].before);
        }
        for (int day = 0; day < steps[i].days; day++) {
            run_trace(50, 50, 0, 0);
            // no engineer here: keep the reservoirs from collapsing
            building_get(map_building_at(map_grid_offset(ax - 2, ay)))->damage_risk = 0;
            building_get(map_building_at(map_grid_offset(ax - 12, ay)))->damage_risk = 0;
        }
        water1 = reservoir_at(ax - 2, ay, &level1);
        water2 = reservoir_at(ax - 12, ay, &level2);
        int ok = water1 && water2 == steps[i].r2_water && (!steps[i].r2_full || level2 == full);
        printf("%-28s R1 %s (%d), R2 %s (%d/%d)%s\n", steps[i].label, water1 ? "water" : "dry", level1,
            water2 ? "water" : "dry", level2, full, ok ? "" : "  <-- UNEXPECTED");
        failures += !ok;
    }
    player_context_switch(0);
    player_context_set_num_players(1);
    printf("%s\n", failures ? "DIFFERENT: reservoirs do not hold water as designed" :
        "Identical: reservoirs hold water as designed");
    return failures ? 1 : 0;
}

static int build_as(int player_id, int type, int x1, int y1, int x2, int y2)
{
    mp_command command = { .type = MP_COMMAND_BUILD, .player_id = player_id, .args = { type, 0, x1, y1, x2, y2, 0, 0 } };
    mp_command_execute(&command);
    return building_get(map_building_at(map_grid_offset(x1, y1)))->type != BUILDING_NONE;
}

// territories (MT.1, D-036): no zone, no building; a mission opens a zone; inhabited houses push it further; the
// other player builds nothing there but roads
static int command_territory(const char *file)
{
    if (!mp_mapgen_create_prepared(file, 2)) {
        printf("Unable to create the prepared map\n");
        return 2;
    }
    game_rules_settings rules;
    game_rules_default_multiplayer_settings(&rules);
    rules.territories = 1;
    rules.ai_invasions = 0;
    game_rules_set_multiplayer(&rules);
    int cx, cy;
    mp_mapgen_city_center(0, &cx, &cy);
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-62s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)

    CHECK(!build_as(0, BUILDING_HOUSE_VACANT_LOT, cx, cy + 2, cx, cy + 2), "without a zone, no house");
    CHECK(build_as(0, BUILDING_MISSION_POST, cx + 3, cy - 4, cx + 3, cy - 4), "a mission is built anywhere");
    run_trace(50, 50, 0, 0);
    CHECK(mp_territory_owner(map_grid_offset(cx + 3 + 20, cy - 4)) == 0, "the mission gives a zone of 20 tiles");
    CHECK(mp_territory_owner(map_grid_offset(cx + 3 + 22, cy - 4)) == -1, "but not further");
    CHECK(build_as(0, BUILDING_HOUSE_VACANT_LOT, cx, cy + 2, cx, cy + 2), "a house in the zone");
    CHECK(!build_as(0, BUILDING_HOUSE_VACANT_LOT, cx + 30, cy + 2, cx + 30, cy + 2), "no house 30 tiles from the mission");
    CHECK(!build_as(1, BUILDING_PREFECTURE, cx, cy + 6, cx, cy + 6), "the other player builds nothing in the zone");
    build_as(1, BUILDING_ROAD, cx - 10, cy + 8, cx - 6, cy + 8);
    CHECK(map_terrain_is(map_grid_offset(cx - 8, cy + 8), TERRAIN_ROAD), "but he builds roads there");

    // a quarter of houses along the main road: once inhabited, the zone goes 20 tiles beyond it
    int far_x = cx - 5 - 20;
    int before = mp_territory_owner(map_grid_offset(far_x, cy + 1));
    build_as(0, BUILDING_HOUSE_VACANT_LOT, cx - 5, cy + 1, cx + 5, cy + 1);
    run_trace(3000, 3000, 0, 0);
    printf("population of player 1: %d\n", city_population());
    int after = mp_territory_owner(map_grid_offset(far_x, cy + 1));
    CHECK(before == -1 && after == 0, "inhabited houses push the zone 20 tiles further");

    player_context_switch(0);
    player_context_set_num_players(1);
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: territories do not work as designed" : "Identical: territories work as designed");
    return failures ? 1 : 0;
}

static figure *first_missionary(int player_id)
{
    for (int i = player_id * MAX_FIGURES + 1; i < (player_id + 1) * MAX_FIGURES; i++) {
        figure *f = figure_get(i);
        if (f->state == FIGURE_STATE_ALIVE && mp_missionary_is_scout(f)) {
            return f;
        }
    }
    return 0;
}

// missions and missionaries (MT.2, MT.3, D-037)
static int command_missions(const char *file)
{
    if (!mp_mapgen_create_prepared(file, 2)) {
        printf("Unable to create the prepared map\n");
        return 2;
    }
    game_rules_settings rules;
    game_rules_default_multiplayer_settings(&rules);
    rules.territories = 1;
    rules.ai_invasions = 0;
    game_rules_set_multiplayer(&rules);
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-62s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    figure *m = first_missionary(0);
    CHECK(m != 0, "player 1 starts with a missionary");
    if (!m) {
        return 1;
    }
    int mx = m->x, my = m->y;
    int treasury = city_finance_treasury();
    CHECK(!build_as(0, BUILDING_MISSION_POST, mx + 30, my, mx + 30, my), "no mission 30 tiles from the missionary");
    CHECK(build_as(0, BUILDING_MISSION_POST, mx + 4, my, mx + 4, my), "a mission 4 tiles from him");
    CHECK(city_finance_treasury() == treasury, "the first mission is free");
    run_trace(50, 50, 0, 0);

    // a warehouse in the new zone, without marble: the next mission waits for marble
    build_as(0, BUILDING_WAREHOUSE, mx - 6, my - 6, mx - 6, my - 6);
    building *warehouse = building_get(map_building_at(map_grid_offset(mx - 6, my - 6)));
    CHECK(!build_as(0, BUILDING_MISSION_POST, mx + 4, my + 6, mx + 4, my + 6), "the second mission needs marble");
    for (int i = 0; i < 6; i++) {
        building_warehouse_add_resource(warehouse, RESOURCE_MARBLE);
    }
    run_trace(50, 50, 0, 0);
    int marble = city_resource_count(RESOURCE_MARBLE);
    treasury = city_finance_treasury();
    CHECK(build_as(0, BUILDING_MISSION_POST, mx + 4, my + 6, mx + 4, my + 6), "with marble, it is built");
    run_trace(50, 50, 0, 0);
    printf("marble: %d loads before, %d after; treasury %d before, %d after\n", marble,
        city_resource_count(RESOURCE_MARBLE), treasury, city_finance_treasury());
    CHECK(city_resource_count(RESOURCE_MARBLE) == marble - MP_MISSION_MARBLE_LOADS, "it took the marble");

    // the missionary walks where he is sent
    mp_command move = { .type = MP_COMMAND_CITY_ACTION, .player_id = 0, .args = { MP_ACTION_MISSIONARY_MOVE, m->id, mx - 25, my - 5 } };
    mp_command_execute(&move);
    run_trace(400, 400, 0, 0);
    printf("missionary at (%d, %d), sent to (%d, %d)\n", m->x, m->y, mx - 25, my - 5);
    CHECK(m->x == mx - 25 && m->y == my - 5, "the missionary walks where he is sent");

    // a mission trains one missionary at a time, for money
    int mission_id = map_building_at(map_grid_offset(mx + 4, my));
    treasury = city_finance_treasury();
    mp_command train = { .type = MP_COMMAND_CITY_ACTION, .player_id = 0, .args = { MP_ACTION_TRAIN_MISSIONARY, mission_id, 0 } };
    mp_command_execute(&train);
    int trained = mp_mission_missionary(mission_id);
    CHECK(trained && city_finance_treasury() == treasury - MP_MISSIONARY_TRAINING_COST, "a mission trains a missionary for money");
    mp_command_execute(&train);
    CHECK(city_finance_treasury() == treasury - MP_MISSIONARY_TRAINING_COST, "only one at a time");
    figure_get(trained)->state = FIGURE_STATE_DEAD;
    mp_command_execute(&train);
    CHECK(mp_mission_missionary(mission_id) && mp_mission_missionary(mission_id) != trained, "when he dies, another one");

    player_context_switch(0);
    player_context_set_num_players(1);
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: missions do not work as designed" : "Identical: missions work as designed");
    return failures ? 1 : 0;
}

// buildings left outside the zone collapse after three months, unless a mission covers them again (MT.4, D-036)
static int command_outside(const char *file)
{
    if (!mp_mapgen_create_prepared(file, 2)) {
        printf("Unable to create the prepared map\n");
        return 2;
    }
    game_rules_settings rules;
    game_rules_default_multiplayer_settings(&rules);
    rules.territories = 1;
    rules.ai_invasions = 0;
    game_rules_set_multiplayer(&rules);
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-62s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
#define TYPE_AT(x, y) building_get(map_building_at(map_grid_offset(x, y)))->type
    figure *m = first_missionary(0);
    int mx = m->x, my = m->y;
    build_as(0, BUILDING_MISSION_POST, mx + 4, my, mx + 4, my);
    run_trace(50, 50, 0, 0);
    CHECK(build_as(0, BUILDING_PREFECTURE, mx + 10, my + 5, mx + 10, my + 5), "a prefecture in the zone of the mission");
    build_as(0, BUILDING_CLEAR_LAND, mx + 4, my, mx + 5, my + 1);
    CHECK(building_get(map_building_at(map_grid_offset(mx + 4, my)))->state != BUILDING_STATE_IN_USE,
        "the mission is cleared");
    run_trace(40 * 50, 40 * 50, 0, 0);
    CHECK(TYPE_AT(mx + 10, my + 5) == BUILDING_PREFECTURE, "40 days later, the prefecture still stands");
    run_trace(10 * 50, 10 * 50, 0, 0);
    CHECK(TYPE_AT(mx + 10, my + 5) != BUILDING_PREFECTURE, "after three months outside the zone, it collapsed");

    // the city owns no land any more: a new mission is free and saves the next prefecture in time
    CHECK(build_as(0, BUILDING_MISSION_POST, mx + 4, my, mx + 4, my), "a new mission, free again");
    run_trace(50, 50, 0, 0);
    build_as(0, BUILDING_PREFECTURE, mx + 12, my + 5, mx + 12, my + 5);
    build_as(0, BUILDING_CLEAR_LAND, mx + 4, my, mx + 5, my + 1);
    run_trace(20 * 50, 20 * 50, 0, 0);
    build_as(0, BUILDING_MISSION_POST, mx + 4, my, mx + 4, my);
    run_trace(60 * 50, 60 * 50, 0, 0);
    CHECK(TYPE_AT(mx + 12, my + 5) == BUILDING_PREFECTURE, "a mission built within the three months saves it");

    player_context_switch(0);
    player_context_set_num_players(1);
#undef TYPE_AT
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: buildings outside the zone do not behave as designed" :
        "Identical: buildings outside the zone behave as designed");
    return failures ? 1 : 0;
}

static int walk_missionary(figure *m, int x, int y)
{
    mp_command move = { .type = MP_COMMAND_CITY_ACTION, .player_id = 0, .args = { MP_ACTION_MISSIONARY_MOVE, m->id, x, y } };
    mp_command_execute(&move);
    for (int day = 0; day < 200 && (m->x != x || m->y != y); day++) {
        run_trace(50, 50, 0, 0);
    }
    run_trace(50, 50, 0, 0); // the fog is updated once a day
    return m->x == x && m->y == y;
}

// fog of war (MB.1, D-038): what player 1 discovers and sees
static int command_fog(const char *file)
{
    if (!mp_mapgen_create_prepared(file, 2)) {
        printf("Unable to create the prepared map\n");
        return 2;
    }
    game_rules_settings rules;
    game_rules_default_multiplayer_settings(&rules);
    rules.ai_invasions = 0;
    game_rules_set_multiplayer(&rules);
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-62s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    int x0, y0, x1, y1;
    mp_mapgen_city_center(0, &x0, &y0);
    mp_mapgen_city_center(1, &x1, &y1);
    int home = map_grid_offset(x0, y0);
    int other = map_grid_offset(x1, y1);
    run_trace(50, 50, 0, 0);
    CHECK(mp_fog_is_discovered(home) && mp_fog_is_lit(home), "player 1 sees around his missionary");
    CHECK(!mp_fog_is_discovered(other), "not the city of player 2");
    figure *m = first_missionary(0);
    CHECK(walk_missionary(m, x1 - 10, y1 + 3), "his missionary walks to it");
    CHECK(mp_fog_is_discovered(other) && mp_fog_is_lit(other), "there, he discovers and sees it");
    CHECK(walk_missionary(m, x0, y0 + 3), "he walks back home");
    CHECK(mp_fog_is_discovered(other) && !mp_fog_is_lit(other), "it stays discovered, but no longer seen");
    rules.fog_of_war = 0;
    game_rules_set_multiplayer(&rules);
    CHECK(mp_fog_is_discovered(map_grid_offset(5, 5)) && mp_fog_is_lit(map_grid_offset(5, 5)),
        "without fog of war, everything shows");

    player_context_switch(0);
    player_context_set_num_players(1);
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: the fog of war does not work as designed" :
        "Identical: the fog of war works as designed");
    return failures ? 1 : 0;
}

// trade cities of the empire of a map: by land and by sea, open or to be opened
static void count_trade_cities(int *land, int *sea)
{
    *land = *sea = 0;
    for (int i = 0; i < 41; i++) { // the empire holds 41 cities
        empire_city *c = empire_city_get(i);
        if (c && c->in_use && (c->type == EMPIRE_CITY_TRADE || c->type == EMPIRE_CITY_FUTURE_TRADE)) {
            if (c->is_sea_trade) {
                (*sea)++;
            } else {
                (*land)++;
            }
        }
    }
}

// a template whose empire does not trade by land and by sea is refused for the prepared maps
static int command_notrade(const char *file)
{
    int refused = !mp_mapgen_create_prepared(file, 2) && mp_mapgen_lacks_trade_routes();
    player_context_switch(0);
    player_context_set_num_players(1);
    printf("%s\n", refused ? "Identical: the map without trade by land and by sea is refused" :
        "DIFFERENT: the map is accepted");
    return refused ? 0 : 1;
}

static int command_tradecities(const char *file)
{
    if (!load(file)) {
        return 2;
    }
    int land, sea;
    count_trade_cities(&land, &sea);
    printf("%s: %d trade cities by land, %d by sea\n", file, land, sea);
    return 0;
}

// the camera of a player reaches every corner of a generated map, after it went through a .mpmap file as in a game
static int command_viewcorners(const char *file, int num_players)
{
    int size = mp_mapgen_default_size(num_players);
    char map_file[64]; // one file per test: ctest runs them in parallel
    snprintf(map_file, sizeof(map_file), "viewcorners-%d.mpmap", num_players);
    if (!mp_mapgen_create(file, num_players, size, 11) || !mp_savegame_write(map_file) ||
        !mp_savegame_read(map_file)) {
        printf("Unable to generate the map\n");
        return 2;
    }
    remove(map_file);
    city_view_set_viewport(800, 600);
    city_view_init();
    int width_tiles, height_tiles;
    city_view_get_viewport_size_tiles(&width_tiles, &height_tiles);
    static const int CORNER_X[] = { 0, 1, 0, 1 };
    static const int CORNER_Y[] = { 0, 0, 1, 1 };
    int failures = 0;
    for (int i = 0; i < 4; i++) {
        int x = CORNER_X[i] ? size - 2 : 1;
        int y = CORNER_Y[i] ? size - 2 : 1;
        int grid_offset = map_grid_offset(x, y);
        int x_view, y_view, camera_x, camera_y;
        city_view_go_to_grid_offset(grid_offset);
        city_view_grid_offset_to_xy_view(grid_offset, &x_view, &y_view);
        city_view_get_camera(&camera_x, &camera_y);
        int visible = (x_view || y_view) && x_view >= camera_x && x_view < camera_x + width_tiles &&
            y_view >= camera_y && y_view < camera_y + height_tiles;
        printf("tile (%d, %d): view (%d, %d), camera (%d, %d): %s\n", x, y, x_view, y_view, camera_x, camera_y,
            visible ? "visible" : "OUT OF REACH");
        failures += !visible;
    }
    player_context_switch(0);
    player_context_set_num_players(1);
    printf("%s\n", failures ? "DIFFERENT: the camera cannot show every corner of the map" :
        "Identical: the camera shows every corner of the map");
    return failures ? 1 : 0;
}

static int command_mapgen(const char *file, int num_players, int seed, int ticks)
{
    int size = mp_mapgen_default_size(num_players);
    if (!mp_mapgen_create(file, num_players, size, seed)) {
        printf("Unable to generate the map\n");
        return 2;
    }
    if (getenv("MAPGEN_OUTPUT")) {
        // a multiplayer map (.mpmap): the starting game of the generated map
        if (!mp_savegame_write(getenv("MAPGEN_OUTPUT"))) {
            return 1;
        }
        printf("%s written\n", getenv("MAPGEN_OUTPUT"));
    }
    if (getenv("MAPGEN_ASCII")) {
        for (int y = 0; y < size; y += 4) {
            for (int x = 0; x < size; x += 2) {
                int t = map_terrain_get(map_grid_offset(x, y));
                putchar(t & TERRAIN_WATER ? '~' : t & TERRAIN_TREE ? 'T' : t & TERRAIN_ROCK ? '^' : t & TERRAIN_MEADOW ? ',' : '.');
            }
            putchar('\n');
        }
    }
    uint64_t first = mp_checksum_state();
    player_context_switch(0);
    player_context_set_num_players(1);
    if (!mp_mapgen_create(file, num_players, size, seed) || mp_checksum_state() != first) {
        printf("DIFFERENT: the same seed gave another map\n");
        return 1;
    }
    for (int p = 0; p < num_players; p++) {
        settle_city(p, size);
    }
    map_road_network_update_grid();
    for (int p = 0; p < num_players; p++) {
        player_context_switch(p);
        map_road_network_update_largest();
    }
    player_context_switch(0);
    run_trace(ticks, ticks, 0, 0);
    int failures = 0;
    for (int p = 0; p < num_players; p++) {
        player_context_switch(p);
        int cx, cy;
        mp_mapgen_city_center(p, &cx, &cy);
        printf("city %d at (%d, %d): population %d, treasury %d\n", p, cx, cy, city_population(), city_finance_treasury());
        failures += city_population() <= 0;
    }
    player_context_switch(0);
    player_context_set_num_players(1);
    printf("%s\n", failures ? "FAILED: a city got no immigrant" : "Every city got immigrants");
    return failures ? 1 : 0;
}

// Diagnosis: one figure of city 0, tick by tick, on a map of CITIES composed cities
static int command_figtrace(const char *file, int num_cities, int id, int from, int to)
{
    int width, height;
    if (!setup_twin_map(file, num_cities, &width, &height)) {
        return 2;
    }
    setting_reset_speeds(500, setting_scroll_speed());
    for (int tick = 1; tick <= to; tick++) {
        run_one_tick();
        for (int p = 0; tick >= from && p < num_cities; p++) {
            figure *f = figure_get(id + p * MAX_FIGURES);
            printf("  city %d target %d targeted by %d\n", p, f->target_figure_id, f->targeted_by_figure_id);
            printf("%d: state %d action %d xy %d,%d dest %d,%d path %d/%d wait %d roam %d dir %d, free paths %d\n",
                tick, f->state, f->action_state, f->x, f->y, f->destination_x, f->destination_y, f->routing_path_id,
                f->routing_path_current_tile, f->wait_ticks, f->roam_length, f->direction, figure_route_count_free());
            printf("    length %d progress %d previous %d,%d terrain %d building %d target %d missile %d source %d,%d\n",
                f->routing_path_length, f->progress_on_tile, f->previous_tile_x, f->previous_tile_y,
                f->terrain_usage, f->building_id, f->target_figure_id, f->wait_ticks_missile, f->source_x, f->source_y);
        }
    }
    return 0;
}

static int command_twinfigures(const char *file, int ticks)
{
    int width, height;
    if (!setup_twin_map(file, 2, &width, &height)) {
        return 2;
    }
    int shift = (width > height ? width : height) + TWIN_GAP;
    player_clone c = { 0, 1, shift, shift, shift + shift * GRID_SIZE };
    setting_reset_speeds(500, setting_scroll_speed());
    for (int tick = 0; tick <= ticks; tick++) {
        if (tick) {
            run_one_tick();
        }
        if (getenv("TWIN_DEBUG_IDS")) {
            int ids[3];
            if (sscanf(getenv("TWIN_DEBUG_IDS"), "%d,%d,%d", &ids[0], &ids[1], &ids[2]) == 3 && tick >= 49) {
                for (int k = 0; k < 3; k++) {
                    for (int p = 0; p < 2; p++) {
                        building *b = building_get(ids[k] + p * MAX_BUILDINGS);
                        printf("    tick %d building %d: type %d network %d entry %d road %d\n", tick, b->id, b->type,
                            b->road_network_id, b->distance_from_entry, b->has_road_access);
                    }
                }
            }
        }
        if (find_twin_building_difference(&c) || find_twin_figure_difference(&c)) {
            printf("the twin's figures differ at tick %d\n", tick);
            game_file_write_saved_game("twinfigures-city.sav");
            player_context_switch(1);
            game_file_write_saved_game("twinfigures-twin.sav");
            player_context_switch(0);
            player_context_set_num_players(1);
            return 1;
        }
    }
    player_context_set_num_players(1);
    printf("twin figures are copies for %d ticks\n", ticks);
    return 0;
}

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

static figure continued_figures[2 * MAX_FIGURES];

static void print_ranges(const char *label)
{
    for (int p = 0; p < player_context_num_players(); p++) {
        player_context_switch(p);
        int count = 0;
        for (int i = 0; i < GRID_SIZE * GRID_SIZE; i++) {
            count += map_water_supply_has_range(i, TERRAIN_RESERVOIR_RANGE) * 1000 + map_water_supply_has_range(i, TERRAIN_FOUNTAIN_RANGE);
        }
        printf("  %s: city %d ranges %d\n", label, p, count);
    }
    player_context_switch(0);
}

static int command_mpresume(const char *file, int ticks, int more)
{
    int width, height;
    // MPRESUME_ALONE: one city on a large grid; MPRESUME_CITIES=N: N cities (default 2)
    int num_cities = getenv("MPRESUME_ALONE") ? 1 : getenv("MPRESUME_CITIES") ? atoi(getenv("MPRESUME_CITIES")) : 2;
    if (!setup_twin_map(file, num_cities, &width, &height)) {
        return 2;
    }
    run_trace(ticks, ticks, 0, 0);
    if (getenv("MPRESUME_RANGES")) {
        print_ranges("saved");
    }
    // one file per tested save: ctest runs the tests in parallel
    char mpsav[300];
    snprintf(mpsav, sizeof(mpsav), "mpresume-%s-%d.mpsav", file, num_cities);
    if (!mp_savegame_write(mpsav)) {
        printf("Unable to write the multiplayer saved game\n");
        return 2;
    }
    // every registered per-city state, to check that loading restores it exactly
    static unsigned char *saved_slots[PLAYER_CONTEXT_MAX_PLAYERS];
    int num_players = player_context_num_players();
    player_context_flush();
    for (int p = 0; p < num_players; p++) {
        free(saved_slots[p]);
        saved_slots[p] = malloc(player_context_state_size());
        memcpy(saved_slots[p], player_context_slot(p), player_context_state_size());
    }
    static piece_list saved_pieces, loaded_pieces;
    saved_pieces.count = loaded_pieces.count = 0;
    uint64_t saved = mp_checksum_state_pieces(record_piece, &saved_pieces);
    static uint64_t continued[MAX_SAMPLES];
    // diagnosis: MPRESUME_DIFF_AT=K lists the pieces that differ K ticks after the save in both runs
    int diff_at = getenv("MPRESUME_DIFF_AT") ? atoi(getenv("MPRESUME_DIFF_AT")) : 0;
    static piece_list continued_pieces, resumed_pieces;
    continued_pieces.count = resumed_pieces.count = 0;
    if (diff_at > 0 && diff_at < more) {
        run_trace(diff_at, diff_at, 0, 0);
        mp_checksum_state_pieces(record_piece, &continued_pieces);
        for (int i = 0; i < 2 * MAX_FIGURES; i++) {
            continued_figures[i] = *figure_get(i);
        }
        more -= diff_at;
    }
    int n = run_trace(more, 1, continued, 0);

    if (!mp_savegame_read(mpsav)) {
        printf("Unable to read the multiplayer saved game\n");
        return 2;
    }
    if (getenv("MPRESUME_RANGES")) {
        print_ranges("loaded");
    }
    player_context_flush();
    int restored = 1;
    for (int p = 0; p < num_players; p++) {
        // messages: their popup queue, real-time sound delays and scrolling belong to the interface
        static const char *interface_state[] = { "messages", "message_sound", 0 };
        const char *region = player_context_first_difference(saved_slots[p], player_context_slot(p), interface_state);
        if (region) {
            printf("  city %d: state \"%s\" differs after loading\n", p, region);
            restored = 0;
        }
    }
    uint64_t loaded = mp_checksum_state_pieces(record_piece, &loaded_pieces);
    for (int i = 0; i < saved_pieces.count && i < loaded_pieces.count; i++) {
        if (saved_pieces.checksums[i] != loaded_pieces.checksums[i]) {
            printf("  piece %d differs after loading: %s\n", i, saved_pieces.names[i]);
        }
    }
    if (continued_pieces.count) {
        run_trace(diff_at, diff_at, 0, 0);
        mp_checksum_state_pieces(record_piece, &resumed_pieces);
        for (int i = 0; i < 2 * MAX_FIGURES; i++) {
            figure *f = figure_get(i);
            if (memcmp(f, &continued_figures[i], sizeof(figure)) != 0) {
                printf("  figure %d: continued type %d action %d xy %d,%d / resumed type %d action %d xy %d,%d\n", i,
                    continued_figures[i].type, continued_figures[i].action_state, continued_figures[i].x,
                    continued_figures[i].y, f->type, f->action_state, f->x, f->y);
            }
        }
        for (int i = 0; i < continued_pieces.count && i < resumed_pieces.count; i++) {
            if (continued_pieces.checksums[i] != resumed_pieces.checksums[i]) {
                printf("  piece %d differs %d ticks after loading: %s\n", i, diff_at, continued_pieces.names[i]);
            }
        }
        return 1;
    }
    static uint64_t resumed[MAX_SAMPLES];
    run_trace(more, 1, resumed, 0);
    player_context_switch(0);
    player_context_set_num_players(1);
    if (!restored) {
        printf("DIFFERENT: loading does not restore the state of every city\n");
        return 1;
    }
    if (saved != loaded) {
        printf("DIFFERENT: the loaded state is not the saved one (%016" PRIx64 " != %016" PRIx64 ")\n", saved, loaded);
        return 1;
    }
    for (int i = 0; i < n && i < MAX_SAMPLES; i++) {
        if (continued[i] != resumed[i]) {
            printf("DIFFERENT: the resumed game diverges at tick %d after loading\n", i);
            return 1;
        }
    }
    remove(mpsav);
    printf("Identical: loading after %d ticks and running %d more equals running on\n", ticks, more);
    return 0;
}

// first tick at which the twin city's statistics differ from the first city's
static int command_twinstats(const char *file, int ticks)
{
    int width, height;
    if (!setup_twin_map(file, 2, &width, &height)) {
        return 2;
    }
    setting_reset_speeds(500, setting_scroll_speed());
    for (int tick = 0; tick <= ticks; tick++) {
        if (tick) {
            run_one_tick();
        }
        city_stats first, second;
        get_stats(&first);
        player_context_switch(1);
        get_stats(&second);
        player_context_switch(0);
        if (memcmp(&first, &second, sizeof(city_stats)) != 0) {
            printf("statistics differ at tick %d\n", tick);
            print_stats("city", &first);
            print_stats("twin", &second);
            // alive figures by type in each city
            int counts[2][FIGURE_TYPE_COUNT] = {{0}};
            for (int i = 1; i < 2 * MAX_FIGURES; i++) {
                figure *f = figure_get(i);
                if (f->state == FIGURE_STATE_ALIVE && f->type < FIGURE_TYPE_COUNT) {
                    counts[i / MAX_FIGURES][f->type]++;
                }
            }
            for (int t = 0; t < FIGURE_TYPE_COUNT; t++) {
                if (counts[0][t] != counts[1][t]) {
                    printf("  figure type %d: city %d, twin %d\n", t, counts[0][t], counts[1][t]);
                }
            }
            game_file_write_saved_game("twinstats-city.sav");
            player_context_switch(1);
            game_file_write_saved_game("twinstats-twin.sav");
            player_context_switch(0);
            player_context_set_num_players(1);
            return 1;
        }
    }
    player_context_set_num_players(1);
    printf("same statistics for %d ticks\n", ticks);
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
    } else if (strcmp(command, "mpresume") == 0 && argc > 4) {
        result = command_mpresume(file, ticks, atoi(argv[4]));
    } else if (strcmp(command, "twinfigures") == 0 && argc > 3) {
        result = command_twinfigures(file, ticks);
    } else if (strcmp(command, "twinstats") == 0 && argc > 3) {
        result = command_twinstats(file, ticks);
    } else if (strcmp(command, "mapgen") == 0 && argc > 5) {
        result = command_mapgen(file, atoi(argv[3]), atoi(argv[4]), atoi(argv[5]));
    } else if (strcmp(command, "mpsave") == 0 && argc > 5) {
        result = command_mpsave(file, atoi(argv[3]), atoi(argv[4]), argv[5]);
    } else if (strcmp(command, "permissions") == 0 && argc > 3) {
        result = command_permissions(file, atoi(argv[3]));
    } else if (strcmp(command, "endscore") == 0 && argc > 3) {
        result = command_endscore(file, ticks);
    } else if (strcmp(command, "aiinvasions") == 0 && argc > 3) {
        result = command_aiinvasions(file, ticks);
    } else if (strcmp(command, "openland") == 0 && argc > 2) {
        result = command_openland(file);
    } else if (strcmp(command, "intruders") == 0 && argc > 3) {
        result = command_intruders(file, ticks);
    } else if (strcmp(command, "neighbours") == 0 && argc > 3) {
        result = command_neighbours(file, ticks);
    } else if (strcmp(command, "figtrace") == 0 && argc > 6) {
        result = command_figtrace(file, atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), atoi(argv[6]));
    } else if (strcmp(command, "caesarfree") == 0 && argc > 3) {
        result = command_caesarfree(file, ticks);
    } else if (strcmp(command, "preparedmap") == 0 && argc > 4) {
        result = command_preparedmap(file, atoi(argv[3]), atoi(argv[4]));
    } else if (strcmp(command, "reservoirlevel") == 0) {
        result = command_reservoirlevel(file);
    } else if (strcmp(command, "notrade") == 0) {
        result = command_notrade(file);
    } else if (strcmp(command, "tradecities") == 0) {
        result = command_tradecities(file);
    } else if (strcmp(command, "fog") == 0) {
        result = command_fog(file);
    } else if (strcmp(command, "outside") == 0) {
        result = command_outside(file);
    } else if (strcmp(command, "missions") == 0) {
        result = command_missions(file);
    } else if (strcmp(command, "territory") == 0) {
        result = command_territory(file);
    } else if (strcmp(command, "caesarroads") == 0) {
        result = command_caesarroads(file);
    } else if (strcmp(command, "viewcorners") == 0 && argc > 3) {
        result = command_viewcorners(file, atoi(argv[3]));
    } else if (strcmp(command, "privatepopups") == 0 && argc > 3) {
        result = command_privatepopups(file, ticks);
    } else if (strcmp(command, "twins") == 0 && argc > 3) {
        result = command_twins(file, ticks);
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
