// Headless simulation tool: loads saved games, runs ticks exactly like the autopilot and
// prints state checksums. See doc/mp/TESTING.md.
#include "building/building.h"
#include "building/construction.h"
#include "figure/figure.h"
#include "figure/formation.h"
#include "building/construction_clear.h"
#include "building/type.h"
#include "building/count.h"
#include "building/menu.h"
#include "building/storage.h"
#include "city/view.h"
#include "city/buildings.h"
#include "city/emperor.h"
#include "city/festival.h"
#include "city/finance.h"
#include "city/message.h"
#include "city/labor.h"
#include "city/gods.h"
#include "city/health.h"
#include "city/population.h"
#include "city/ratings.h"
#include "scenario/data.h"
#include "scenario/editor_map.h"
#include "scenario/property.h"
#include "scenario/request.h"
#include "city/data_private.h"
#include "city/resource.h"
#include "city/sentiment.h"
#include "empire/city.h"
#include "empire/empire.h"
#include "empire/trade_prices.h"
#include "empire/trade_route.h"
#include "empire/type.h"
#include "core/buffer.h"
#include "core/time.h"
#include "game/file.h"
#include "game/game.h"
#include "game/rules.h"
#include "game/settings.h"
#include "core/image.h"
#include "map/bridge.h"
#include "map/elevation.h"
#include "map/aqueduct.h"
#include "map/image.h"
#include "map/terrain.h"
#include "map/grid.h"
#include "figure/route.h"
#include "mp/audit.h"
#include "map/road_network.h"
#include "map/routing.h"
#include "map/routing_path.h"
#include "map/routing_terrain.h"
#include "building/construction_routed.h"
#include "game/undo.h"
#include "map/figure.h"
#include "map/water_supply.h"
#include "map/building.h"
#include "map/owner.h"
#include "mp/caesar.h"
#include "mp/caesar_rules.h"
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
#include "mp/trade.h"
#include "mp/fog.h"
#include "building/warehouse.h"
#include "mp/missionary.h"
#include "mp/territory.h"
#include "scenario/map.h"
#include "scenario/invasion.h"
#include "game/resource.h"
#include "mp/colors.h"
#include "mp/lobby.h"
#include "mp/lockstep.h"
#include "mp/session.h"
#include "platform/net.h"

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
    int prepared_map;     // the prepared map the commands play on (--map N: N - 1), map 1 by default (T4.14)
} options = { -1, -1, 0, 0 };

static int usage(void)
{
    printf("Usage: simtool [OPTIONS] COMMAND ...\n");
    printf("Options (before the command):\n");
    printf("  --difficulty N    local difficulty setting (0 very easy .. 4 very hard), as if set in c3.inf\n");
    printf("  --gods 0|1        local gods setting, as if set in c3.inf\n");
    printf("  --mp              play with the default multiplayer rules instead of the local settings\n");
    printf("  --map N           the prepared map of the commands that use one (1 by default, T4.14)\n");
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
    printf("  simtool preparedmap SAVE PLAYERS TICKS prepared map: land reached from the main road, food and\n");
    printf("                                         materials of each arrival point, permissions, farms;\n");
    printf("                                         MAPGEN_PICTURE=F.ppm\n");
    printf("  simtool reservoirlevel SAVE            prepared map: a reservoir cut off from its source empties\n");
    printf("                                         slowly, joined again it fills\n");
    printf("  simtool drying SAVE PLAYERS            prepared map: no pond; the city of the rocks dries up when\n");
    printf("                                         the aqueduct of Caesar is cut, fills again when mended\n");
    printf("  simtool inlandwater SAVE PLAYERS       prepared map: the aqueduct of Caesar waters the player inland\n");
    printf("                                         whichever arrival point he draws (players 2 to 4 too)\n");
    printf("  simtool longroutes SAVE [PLAYERS]      prepared map (for 4 by default): paths between the cities under\n");
    printf("                                         400 steps, within the limit of\n");
    printf("                                         a figure, a caravan between the farthest players arrives,\n");
    printf("                                         a ship sails a sea as large as the grid\n");
    printf("  simtool farinvasion SAVE              prepared map for 4: an enemy army from the edge farthest from a\n");
    printf("                                         city finds its way over the land and reaches it\n");
    printf("  simtool mapchoice SAVE                 the lobby chooses map 1, map 2 or one drawn by lot (T4.14)\n");
    printf("  simtool menuowner SAVE                prepared map: the build menu of the local player keeps his\n");
    printf("                                         materials when another player opens a route with the empire\n");
    printf("  simtool caravans SAVE                 a caravan of player 2 brings marble to player 1, who pays\n");
    printf("  simtool tradeprices SAVE               prices between players and notices to the buyer; a delivery\n");
    printf("                                         is paid at the price of the seller, without portorium\n");
    printf("  simtool importprice SAVE               multiplayer: the empire trades at the price of Rome, plus a\n");
    printf("                                         portorium of 50%% to buy, minus it to sell; classic unchanged\n");
    printf("  simtool notrade SAVE                   a template without trade by land and by sea is refused\n");
    printf("  simtool inspect MPSAV                  players, rules, climate, trade, missionaries of a saved game\n");
    printf("  simtool empiresells SAVE               the empire always sells, even when a player sells cheaper\n");
    printf("  simtool tradeisolation SAVE            each player trades with the empire on his own: the trade of\n");
    printf("                                         another player stays the same\n");
    printf("  simtool tradeconservation SAVE         trade between players makes and loses no goods nor money\n");
    printf("  simtool traderesume SAVE               a game saved while caravans travel goes on the same\n");
    printf("  simtool caesarstate SAVE               laurels and wrath of Caesar: in the checksum, saved, resumed\n");
    printf("  simtool caesarlaurels SAVE             notes and monthly laurels of a city, ranks, the score wins\n");
    printf("  simtool terrain MAP PLAYERS X Y W H [raw]  the terrain of a part of the prepared map (or of an .mpsav),\n");
    printf("                                         one letter a tile; raw: terrain bits, image and elevation too\n");
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
    printf("  simtool lobbyrules SAVE                the lobby proposes an easy game; hosted alone, the game starts\n");
    printf("                                         with the rules changed after hosting\n");
    printf("  simtool resumerules TEMPLATE PORT [HEADER]  a multiplayer game resumed from the lobby keeps its saved\n"
           "                                        rules (HEADER: bytes of an older header, missing rules read as 0)\n");
    printf("  simtool badrules PORT                  a player refuses rules of the host out of their bounds\n");
    printf("  simtool attacksource SAVE SOURCE TICKS attacks of SOURCE (army, uprising, mars) come in a classic\n");
    printf("                                         game and with AI invasions on, none with them off\n");
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

// the prepared map chosen by --map (map 1 by default): every test of the prepared maps runs on each of them (T4.14)
static int create_prepared(const char *file, int num_players, unsigned int placement_seed)
{
    return mp_mapgen_create_prepared_map(file, num_players, options.prepared_map, placement_seed);
}

static int prepared_size(int num_players)
{
    return mp_mapgen_prepared_map_size(options.prepared_map, num_players);
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

// T4.11: the rules chosen in the lobby, as a player sees them
static int same_lobby_rules(const game_rules_settings *a, const game_rules_settings *b)
{
    return a->difficulty == b->difficulty && a->gods_enabled == b->gods_enabled &&
        a->ai_invasions == b->ai_invasions && a->fog_of_war == b->fog_of_war &&
        a->end_condition == b->end_condition && a->caesar_score == b->caesar_score &&
        a->prepared_map == b->prepared_map;
}

static void print_lobby_rules(const char *label, const game_rules_settings *r)
{
    printf("%s: difficulty %d gods %d invasions %d fog %d end %d score %d map %d\n", label, r->difficulty,
        r->gods_enabled, r->ai_invasions, r->fog_of_war, r->end_condition, r->caesar_score, r->prepared_map);
}

// The host changes every rule of the lobby after "Host", once the players are there, then starts the game
static void change_every_lobby_rule(void)
{
    mp_lobby_change_rule(MP_LOBBY_RULE_DIFFICULTY);
    mp_lobby_change_rule(MP_LOBBY_RULE_GODS);
    mp_lobby_change_rule(MP_LOBBY_RULE_END);
    mp_lobby_change_rule(MP_LOBBY_RULE_INVASIONS);
    mp_lobby_change_rule(MP_LOBBY_RULE_FOG);
    mp_lobby_change_rule(MP_LOBBY_RULE_MAP);
}

static int every_lobby_rule_differs(const game_rules_settings *a, const game_rules_settings *b)
{
    return a->difficulty != b->difficulty && a->gods_enabled != b->gods_enabled &&
        a->ai_invasions != b->ai_invasions && a->fog_of_war != b->fog_of_war && a->caesar_score != b->caesar_score &&
        a->prepared_map != b->prepared_map;
}

static int command_mpnode(int argc, char **argv)
{
    // argv: mpnode host PORT PLAYERS SAVE TICKS [cities] | mpnode join ADDRESS PORT TICKS
    int is_host = argc >= 7 && strcmp(argv[2], "host") == 0;
    int cities = 0, generate = 0, lobby_rules = 0, map2 = 0;
    for (int i = 7; i < argc; i++) {
        cities |= strcmp(argv[i], "cities") == 0;
        generate |= strcmp(argv[i], "generate") == 0;
        lobby_rules |= strcmp(argv[i], "rules") == 0;
        map2 |= strcmp(argv[i], "map2") == 0; // 'map2' (T4.14): the host plays the generated map on map 2
    }
    int is_join = argc >= 6 && strcmp(argv[2], "join") == 0;
    if (!is_host && !is_join) {
        return usage();
    }
    int ticks = atoi(is_host ? argv[6] : argv[5]);
    int cheat = is_join && argc >= 7 && strcmp(argv[6], "desync") == 0;
    // 'pause': this client pauses the game at half time and resumes it one to two seconds after it saw the pause
    // (time() counts whole seconds: a pause of 'one second' from the request could end at once on a busy computer,
    // before the other players saw it);
    // 'leave': this client leaves the game at half time
    int pauser = is_join && argc >= 7 && strcmp(argv[6], "pause") == 0;
    // 'baddata': this client has other game data; the host must refuse it
    int bad_data = is_join && argc >= 7 && strcmp(argv[6], "baddata") == 0;
    int expect_reject = is_host && strcmp(argv[argc - 1], "expect-reject") == 0;
    if (bad_data) {
        mp_lockstep_test_alter_game_data();
    }
    int leaver = is_join && argc >= 7 && strcmp(argv[6], "leave") == 0;
    // 'rules' (T4.11): the host starts with the rules of the lobby, changes them all once the players are there and
    // starts the game by the button of the lobby; every player must play with the changed rules, and a client sees
    // them in its lobby without being able to change them
    lobby_rules |= is_join && argc >= 7 && strcmp(argv[6], "rules") == 0;
    game_rules_settings rules_at_host = { 0 }, rules_changed = { 0 };
    int rules_were_changed = 0;
    if (lobby_rules && is_host) {
        mp_lobby_rules_init();
        mp_lobby_rules_settings(&rules_at_host);
        mp_lockstep_set_rules(&rules_at_host);
    }
    if (map2 && is_host) {
        game_rules_settings rules;
        game_rules_default_multiplayer_settings(&rules);
        rules.prepared_map = GAME_MAP_2;
        mp_lockstep_set_rules(&rules);
    }
    time_t pause_start = 0, pause_seen_at = 0;
    int paused_seen = 0, ticks_while_paused = 0, tick_at_pause = -1;
    int ok = is_host ? mp_lockstep_host(atoi(argv[3]), atoi(argv[4]), argv[5], cities || generate)
                     : mp_lockstep_join(argv[3], atoi(argv[4]));
    if (ok && generate) {
        mp_lockstep_set_generated_map(1, 5);
    }
    if (ok && lobby_rules && is_host) {
        mp_lockstep_set_manual_start(1);
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
        if (lobby_rules && is_host && state == MP_LOCKSTEP_WAITING_FOR_PLAYERS && !rules_were_changed &&
            mp_lockstep_connected_players() == atoi(argv[4])) {
            change_every_lobby_rule();
            mp_lobby_rules_settings(&rules_changed);
            rules_were_changed = 1;
            mp_lobby_start_game();
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
                if (pauser && !pause_seen_at) {
                    pause_seen_at = time(0);
                }
                if (pauser && time(0) - pause_seen_at >= 2) {
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
    // the prepared map of the game, the same for every player (T4.14)
    printf("map %d, %d tiles wide\n", game_rules_multiplayer_settings()->prepared_map + 1, map_grid_width());
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
    if (lobby_rules) {
        const game_rules_settings *game = game_rules_multiplayer_settings();
        print_lobby_rules("game rules", game);
        if (is_host) {
            print_lobby_rules("rules when hosting", &rules_at_host);
            print_lobby_rules("rules at the start", &rules_changed);
            if (!rules_were_changed || !every_lobby_rule_differs(&rules_at_host, &rules_changed) ||
                !same_lobby_rules(game, &rules_changed)) {
                printf("WRONG: the game does not play with the rules changed after hosting\n");
                result = 0;
            }
        } else {
            const game_rules_settings *shown = mp_lobby_rules_shown();
            print_lobby_rules("lobby rules", shown);
            game_rules_settings before = *shown;
            mp_lobby_change_rule(MP_LOBBY_RULE_DIFFICULTY);
            mp_lobby_change_rule(MP_LOBBY_RULE_INVASIONS);
            if (mp_lobby_rules_editable() || !same_lobby_rules(&before, mp_lobby_rules_shown())) {
                printf("WRONG: a player who joined changes the rules of the host\n");
                result = 0;
            }
            if (!mp_lockstep_lobby_rules() || !same_lobby_rules(shown, game)) {
                printf("WRONG: the lobby of this player did not show the rules of the game\n");
                result = 0;
            }
        }
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

// T4.11: every source of attack obeys the AI invasions rule of a multiplayer game. The armies of the scenario,
// its local uprisings and the uprising sent by an angry Mars are told apart by the invasion of their formation.
enum { ATTACK_ARMY = 0, ATTACK_UPRISING = 1, ATTACK_MARS = 2, ATTACK_OTHER = 3, ATTACK_SOURCES = 4 };
static const char *ATTACK_NAMES[ATTACK_SOURCES] = { "army", "uprising", "mars", "other" };

static int attack_source(const formation *m)
{
    if (m->invasion_id == 23) {
        return ATTACK_MARS; // scenario_invasion_start_from_mars (the cheat uses it too, never in this test)
    }
    if (m->invasion_id < 0 || m->invasion_id >= MAX_INVASIONS) {
        return ATTACK_OTHER;
    }
    switch (scenario.invasions[m->invasion_id].type) {
        case INVASION_TYPE_ENEMY_ARMY:
            return ATTACK_ARMY;
        case INVASION_TYPE_LOCAL_UPRISING:
            return ATTACK_UPRISING;
        default:
            return ATTACK_OTHER;
    }
}

static int count_attacks(const char *file, int ticks, int ai_invasions, int multiplayer, int forced,
    int counts[ATTACK_SOURCES])
{
    memset(counts, 0, ATTACK_SOURCES * sizeof(int));
    if (!load(file)) {
        return 0;
    }
    if (multiplayer) {
        game_rules_settings rules;
        game_rules_default_multiplayer_settings(&rules);
        rules.ai_invasions = ai_invasions;
        rules.gods_enabled = 1;
        game_rules_set_multiplayer(&rules);
    } else {
        game_rules_set_classic();
    }
    if (forced == ATTACK_MARS) {
        // Mars is angry enough for a small curse at the next month, in a mission whose uprising has 9 men; the
        // other gods are content (a curse of another god is not an attack)
        scenario_set_campaign_mission(14);
        for (int g = 0; g < MAX_GODS; g++) {
            god_status *god = &city_data.religion.gods[g];
            god->happiness = god->target_happiness = g == GOD_MARS ? 0 : 60;
            god->wrath_bolts = g == GOD_MARS ? 20 : 0;
            god->small_curse_done = 0;
            god->months_since_festival = g == GOD_MARS ? 10 : 0;
        }
    } else if (forced == ATTACK_UPRISING) {
        // the scenario has a local uprising at the start of the next month, from a random invasion point
        int slot = MAX_INVASIONS - 1;
        for (int i = MAX_INVASIONS - 1; i >= 0; i--) {
            if (!scenario.invasions[i].type) {
                slot = i;
                break;
            }
        }
        int month = game_time_month() + 1;
        scenario.invasions[slot].type = INVASION_TYPE_LOCAL_UPRISING;
        scenario.invasions[slot].year = game_time_year() + month / 12 - scenario.start_year;
        scenario.invasions[slot].month = month % 12;
        scenario.invasions[slot].amount = 9;
        scenario.invasions[slot].from = MAX_INVASION_POINTS;
        scenario.invasions[slot].attack_type = FORMATION_ATTACK_FOOD_CHAIN;
    }
    static unsigned char seen[0x10000];
    memset(seen, 0, sizeof(seen));
    for (int i = 1; i < FORMATION_ARRAY_SIZE; i++) {
        formation *m = formation_get(i);
        if (m->in_use && !m->is_herd && !m->is_legion) {
            seen[m->invasion_sequence & 0xffff] = 1;
        }
    }
    setting_reset_speeds(500, setting_scroll_speed());
    for (int tick = 0; tick < ticks; tick++) {
        run_one_tick();
        for (int i = 1; i < FORMATION_ARRAY_SIZE; i++) {
            formation *m = formation_get(i);
            if (m->in_use && !m->is_herd && !m->is_legion && !seen[m->invasion_sequence & 0xffff]) {
                seen[m->invasion_sequence & 0xffff] = 1;
                counts[attack_source(m)]++;
            }
        }
    }
    return 1;
}

static int command_attacksource(const char *file, const char *source, int ticks)
{
    int wanted = -1;
    for (int s = 0; s < ATTACK_OTHER; s++) {
        if (strcmp(source, ATTACK_NAMES[s]) == 0) {
            wanted = s;
        }
    }
    if (wanted < 0) {
        return usage();
    }
    const char *runs[3] = { "classic", "multiplayer, AI invasions on", "multiplayer, AI invasions off" };
    int counts[3][ATTACK_SOURCES];
    for (int run = 0; run < 3; run++) {
        if (!count_attacks(file, ticks, run != 2, run != 0, wanted, counts[run])) {
            return 1;
        }
        printf("%s:", runs[run]);
        for (int s = 0; s < ATTACK_SOURCES; s++) {
            printf(" %s %d", ATTACK_NAMES[s], counts[run][s]);
        }
        printf("\n");
    }
    if (counts[0][wanted] <= 0 || counts[1][wanted] <= 0) {
        printf("FAILED: no attack of this source in this game, the test proves nothing\n");
        return 1;
    }
    int result = 0;
    for (int s = 0; s < ATTACK_OTHER; s++) {
        if (counts[2][s]) {
            printf("FAILED: %d attack(s) of source %s with AI invasions off\n", counts[2][s], ATTACK_NAMES[s]);
            result = 1;
        }
    }
    return result;
}

// T4.4, T4.11: the lobby proposes an easy game (the default rules of the tests stay hard); a game hosted alone
// starts with the rules changed after "Host"
static int command_lobbyrules(const char *file)
{
    int result = 0;
    mp_lobby_rules_init();
    game_rules_settings lobby, defaults;
    mp_lobby_rules_settings(&lobby);
    game_rules_default_multiplayer_settings(&defaults);
    print_lobby_rules("lobby rules", &lobby);
    print_lobby_rules("default rules", &defaults);
    if (lobby.difficulty != DIFFICULTY_EASY || defaults.difficulty != DIFFICULTY_HARD) {
        printf("WRONG: the lobby must propose an easy game, the default rules stay hard\n");
        result = 1;
    }
    if (!mp_lobby_rules_editable()) {
        printf("WRONG: the rules cannot be chosen before hosting\n");
        result = 1;
    }
    mp_lockstep_set_rules(&lobby);
    const int port = 27442;
    if (!mp_lockstep_host(port, 1, file, 1)) {
        printf("FAILED: %s\n", mp_lockstep_status());
        return 1;
    }
    mp_lockstep_set_manual_start(1);
    change_every_lobby_rule();
    game_rules_settings changed;
    mp_lobby_rules_settings(&changed);
    if (!mp_lockstep_lobby_rules() || !same_lobby_rules(mp_lockstep_lobby_rules(), &changed)) {
        printf("WRONG: the host does not keep the rules changed after hosting\n");
        result = 1;
    }
    mp_lobby_start_game();
    for (int frame = 0; frame < 100 && mp_lockstep_get_state() == MP_LOCKSTEP_WAITING_FOR_PLAYERS; frame++) {
        mp_lockstep_poll();
    }
    print_lobby_rules("rules after hosting", &changed);
    print_lobby_rules("game rules", game_rules_multiplayer_settings());
    if (mp_lockstep_get_state() != MP_LOCKSTEP_RUNNING || !every_lobby_rule_differs(&lobby, &changed) ||
        !same_lobby_rules(game_rules_multiplayer_settings(), &changed) ||
        game_rules_difficulty() != changed.difficulty) {
        printf("WRONG: the game does not start with the rules changed after hosting\n");
        result = 1;
    }
    mp_lockstep_stop();
    char name[64];
    snprintf(name, sizeof(name), "mp-session-%d-p0.mpsav", port);
    remove(name);
    return result;
}

// A game hosted alone on a new map with this choice of map and this seed of the lobby: the map it is played on, -1
// when it does not start
static int hosted_map(const char *file, int choice, unsigned int seed, int *width)
{
    const int port = 27450;
    player_context_switch(0);
    player_context_set_num_players(1); // the template is a game of one city
    game_rules_settings rules;
    mp_lobby_rules_settings(&rules);
    rules.prepared_map = choice;
    mp_lockstep_set_rules(&rules);
    if (!mp_lockstep_host(port, 1, file, 1)) {
        printf("FAILED: %s\n", mp_lockstep_status());
        return -1;
    }
    mp_lockstep_set_generated_map(1, seed);
    mp_lockstep_set_manual_start(1);
    mp_lobby_start_game();
    for (int frame = 0; frame < 100 && mp_lockstep_get_state() == MP_LOCKSTEP_WAITING_FOR_PLAYERS; frame++) {
        mp_lockstep_poll();
    }
    int map = mp_lockstep_get_state() == MP_LOCKSTEP_RUNNING ? game_rules_multiplayer_settings()->prepared_map : -1;
    *width = map_grid_width();
    mp_lockstep_stop();
    char name[64];
    snprintf(name, sizeof(name), "mp-session-%d-p0.mpsav", port);
    remove(name);
    player_context_switch(0);
    player_context_set_num_players(1);
    return map;
}

// T4.14: the lobby chooses the prepared map, map 1, map 2 or one drawn by lot with the seed of the lobby; the draw
// gives the same map for the same seed, the game keeps the map it is played on, and the two maps are different
static int command_mapchoice(const char *file)
{
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-70s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    game_rules_settings defaults, lobby;
    game_rules_default_multiplayer_settings(&defaults);
    mp_lobby_rules_init();
    mp_lobby_rules_settings(&lobby);
    CHECK(defaults.prepared_map == GAME_MAP_1, "the default rules (tests, command line) keep map 1");
    CHECK(lobby.prepared_map == GAME_MAP_RANDOM, "the lobby proposes a map drawn by lot");
    int cycle[3];
    for (int i = 0; i < 3; i++) {
        mp_lobby_change_rule(MP_LOBBY_RULE_MAP);
        mp_lobby_rules_settings(&lobby);
        cycle[i] = lobby.prepared_map;
    }
    CHECK(cycle[0] == GAME_MAP_1 && cycle[1] == GAME_MAP_2 && cycle[2] == GAME_MAP_RANDOM,
        "the rule goes from map 1 to map 2, then to a map drawn by lot");

    int fixed = 1, same = 1, drawn[MP_MAPGEN_NUM_PREPARED_MAPS] = { 0 };
    for (unsigned int seed = 0; seed <= 40; seed++) {
        fixed &= mp_mapgen_choose_prepared_map(GAME_MAP_1, seed) == 0 && mp_mapgen_choose_prepared_map(GAME_MAP_2, seed) == 1;
        int map = mp_mapgen_choose_prepared_map(GAME_MAP_RANDOM, seed);
        same &= map == mp_mapgen_choose_prepared_map(GAME_MAP_RANDOM, seed);
        if (map >= 0 && map < MP_MAPGEN_NUM_PREPARED_MAPS && seed > 0) {
            drawn[map]++;
        }
    }
    printf("maps drawn by lot with the seeds 1 to 40: map 1 %d times, map 2 %d times\n", drawn[0], drawn[1]);
    CHECK(fixed, "a map chosen in the lobby is the one played");
    CHECK(same, "the same seed draws the same map");
    CHECK(drawn[0] >= 10 && drawn[1] >= 10, "both maps are drawn");
    CHECK(mp_mapgen_choose_prepared_map(GAME_MAP_RANDOM, 0) == 0, "seed 0 keeps map 1 (tests)");

    // the two maps differ: size, sea and bridge, place of every city
    for (int players = 2; players <= 4; players += 2) {
        int size[2], bridge[2], north[2], cx[2][4], cy[2][4];
        for (int map = 0; map < 2; map++) {
            player_context_switch(0);
            player_context_set_num_players(1);
            if (!mp_mapgen_create_prepared_map(file, players, map, 0)) {
                printf("Unable to create the prepared map %d for %d players\n", map + 1, players);
                return 2;
            }
            size[map] = map_grid_width();
            int south;
            mp_mapgen_caesar_bridge(&bridge[map], &north[map], &south);
            for (int p = 0; p < players; p++) {
                mp_mapgen_city_center(p, &cx[map][p], &cy[map][p]);
            }
        }
        int cities_differ = 1;
        for (int p = 0; p < players; p++) {
            cities_differ &= cx[0][p] != cx[1][p] || cy[0][p] != cy[1][p];
        }
        printf("%d players: map 1 %d tiles wide, bridge at (%d, %d); map 2 %d tiles wide, bridge at (%d, %d)\n",
            players, size[0], bridge[0], north[0], size[1], bridge[1], north[1]);
        CHECK(cities_differ && (bridge[0] != bridge[1] || north[0] != north[1]),
            players == 2 ? "map 2 for 2 players has other places and another sea" :
            "map 2 for 4 players has other places and another sea");
        CHECK(mp_mapgen_prepared_map_size(1, players) == size[1], "the size of map 2 is known before it is made");
    }

    // the map travels with the rules of the game, saved and read back
    player_context_switch(0);
    player_context_set_num_players(1);
    if (!mp_mapgen_create_prepared_map(file, 2, 1, 0)) {
        printf("Unable to create the prepared map 2\n");
        return 2;
    }
    game_rules_settings rules;
    game_rules_default_multiplayer_settings(&rules);
    rules.territories = 1;
    rules.prepared_map = GAME_MAP_2;
    game_rules_set_multiplayer(&rules);
    const char *map_file = "mapchoice.mpmap";
    int written = mp_savegame_write(map_file);
    rules.prepared_map = GAME_MAP_1;
    game_rules_set_multiplayer(&rules);
    int read = written && mp_savegame_read(map_file);
    remove(map_file);
    CHECK(read && game_rules_multiplayer_settings()->prepared_map == GAME_MAP_2, "a saved game keeps its map");

    // hosted with a map drawn by lot, the game is played on the map of the draw, which its rules keep
    for (unsigned int seed = 1; seed <= 2; seed++) {
        int expected = mp_mapgen_choose_prepared_map(GAME_MAP_RANDOM, seed);
        int width = 0;
        int map = hosted_map(file, GAME_MAP_RANDOM, seed, &width);
        printf("hosted with a map drawn by lot, seed %u: map %d, %d tiles wide (drawn: map %d)\n", seed, map + 1,
            width, expected + 1);
        CHECK(map == expected && width == mp_mapgen_prepared_map_size(expected, 1),
            "the game is played on the map drawn, and keeps it in its rules");
    }
    int width = 0;
    CHECK(hosted_map(file, GAME_MAP_2, 1, &width) == 1 && width == mp_mapgen_prepared_map_size(1, 1),
        "hosted with map 2, the game is played on map 2");
    player_context_switch(0);
    player_context_set_num_players(1);
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: the lobby does not choose the map as planned" :
        "Identical: the lobby chooses the map as planned");
    return failures ? 1 : 0;
}

// K-review-fixes: a host sends the lobby of a player some rules; NUMBERS are the ten integers of the message
static int send_raw_rules(int socket, const int *numbers)
{
    uint8_t payload[4 + 1 + 11 * 4];
    buffer buf;
    buffer_init(&buf, payload, sizeof(payload));
    buffer_write_i32(&buf, 1 + 11 * 4);
    buffer_write_u8(&buf, 10); // MSG_RULES of mp/lockstep.c
    for (int i = 0; i < 10; i++) {
        buffer_write_i32(&buf, numbers[i]);
    }
    buffer_write_i32(&buf, GAME_MAP_1); // the prepared map (T4.14)
    return net_send(socket, payload, buf.index);
}

static void poll_lockstep_for(int milliseconds)
{
    for (int waited = 0; waited < milliseconds; waited += 20) {
        mp_lockstep_poll();
        net_sleep(20);
    }
}

// K-review-fixes (T4.11): a player who joined trusts no number of the rules of the host. A fake host sends rules
// out of their bounds (difficulty 99, booleans 7, end condition 9...): the lobby of the player must never show
// them; valid rules sent next are shown. A welcome message with rules out of bounds is refused.
static int command_badrules(int port)
{
    int listener = net_listen(port);
    if (listener == NET_INVALID_SOCKET) {
        printf("FAILED: cannot listen on port %d\n", port);
        return 2;
    }
    if (!mp_lockstep_join("127.0.0.1", port)) {
        printf("FAILED: %s\n", mp_lockstep_status());
        net_close(listener);
        return 2;
    }
    int host = NET_INVALID_SOCKET;
    for (int i = 0; i < 100 && host == NET_INVALID_SOCKET; i++) {
        host = net_accept(listener);
        net_sleep(20);
    }
    if (host == NET_INVALID_SOCKET) {
        printf("FAILED: the player did not connect\n");
        mp_lockstep_stop();
        net_close(listener);
        return 2;
    }
    int result = 0;
    const int bad[][10] = {
        { 99, 1, 0, 0, 1, 0, 10, 1, 1, 1000 },
        { -1, 1, 0, 0, 1, 0, 10, 1, 1, 1000 },
        { 1, 7, 0, 0, 1, 0, 10, 1, 1, 1000 },
        { 1, 1, -3, 0, 1, 0, 10, 1, 1, 1000 },
        { 1, 1, 0, 2, 1, 0, 10, 1, 1, 1000 },
        { 1, 1, 0, 0, 5, 0, 10, 1, 1, 1000 },
        { 1, 1, 0, 0, 1, 9, 10, 1, 1, 1000 },
        { 1, 1, 0, 0, 1, 0, -10, 1, 1, 1000 },
        { 1, 1, 0, 0, 1, 0, 10, 2, 1, 1000 },
        { 1, 1, 0, 0, 1, 0, 10, 1, -1, 1000 },
        { 1, 1, 0, 0, 1, 0, 10, 1, 1, -5 }
    };
    for (int i = 0; i < (int) (sizeof(bad) / sizeof(bad[0])); i++) {
        send_raw_rules(host, bad[i]);
        poll_lockstep_for(100);
        const game_rules_settings *seen = mp_lockstep_lobby_rules();
        const game_rules_settings *shown = mp_lobby_rules_shown();
        if (seen && seen->difficulty == bad[i][0] && seen->gods_enabled == bad[i][1] &&
            seen->end_condition == bad[i][5] && seen->caesar_score == bad[i][9] &&
            seen->score_years == bad[i][6] && seen->fog_of_war == bad[i][8]) {
            printf("WRONG: rules %d out of their bounds reach the lobby of the player\n", i);
            result = 1;
        }
        if (shown->difficulty < DIFFICULTY_VERY_EASY || shown->difficulty > DIFFICULTY_VERY_HARD ||
            (shown->gods_enabled != 0 && shown->gods_enabled != 1)) {
            printf("WRONG: the lobby shows difficulty %d, gods %d\n", shown->difficulty, shown->gods_enabled);
            result = 1;
        }
    }
    const int good[10] = { DIFFICULTY_VERY_HARD, 0, 1, 1, 0, GAME_END_CAESAR, 5, 1, 0, 1500 };
    send_raw_rules(host, good);
    poll_lockstep_for(100);
    const game_rules_settings *seen = mp_lockstep_lobby_rules();
    if (!seen) {
        printf("WRONG: valid rules do not reach the lobby of the player\n");
        result = 1;
    } else {
        print_lobby_rules("valid rules seen", seen);
        if (seen->difficulty != DIFFICULTY_VERY_HARD || seen->gods_enabled != 0 ||
            seen->end_condition != GAME_END_CAESAR || seen->caesar_score != 1500 || seen->score_years != 5) {
            printf("WRONG: valid rules are not shown as sent\n");
            result = 1;
        }
    }
    // welcome messages with rules out of their bounds, then with a player index or a number of players out of
    // theirs: the player leaves before reading the game
    const int welcome_rules[12] = { GAME_MODE_MULTIPLAYER, DIFFICULTY_EASY, 1, 0, 0, 1, 0, 10, 1, 1, 1000, GAME_MAP_1 };
    const struct { int player; int players; int difficulty; const char *what; } welcomes[] = {
        { 1, 2, 42, "difficulty 42" },
        { -1, 2, DIFFICULTY_EASY, "player -1" },
        { 2, 2, DIFFICULTY_EASY, "player 3 of 2" },
        { 1, 9, DIFFICULTY_EASY, "9 players" },
        { 0, 1, DIFFICULTY_EASY, "1 player" },
    };
    for (int w = 0; w < (int) (sizeof(welcomes) / sizeof(welcomes[0])); w++) {
        if (w > 0) {
            mp_lockstep_stop();
            net_close(host);
            host = NET_INVALID_SOCKET;
            if (!mp_lockstep_join("127.0.0.1", port)) {
                printf("FAILED: %s\n", mp_lockstep_status());
                net_close(listener);
                return 2;
            }
            for (int i = 0; i < 100 && host == NET_INVALID_SOCKET; i++) {
                host = net_accept(listener);
                net_sleep(20);
            }
            if (host == NET_INVALID_SOCKET) {
                printf("FAILED: the player did not connect again\n");
                mp_lockstep_stop();
                net_close(listener);
                return 2;
            }
        }
        uint8_t welcome[256];
        buffer buf;
        buffer_init(&buf, welcome, sizeof(welcome));
        buffer_write_i32(&buf, 0); // size, written below
        buffer_write_u8(&buf, 2); // MSG_WELCOME
        buffer_write_i32(&buf, welcomes[w].player);
        buffer_write_i32(&buf, welcomes[w].players);
        buffer_write_i32(&buf, 0); // base tick
        buffer_write_u8(&buf, 1); // separate cities
        buffer_write_u32(&buf, 0);
        buffer_write_u32(&buf, 0);
        for (int i = 0; i < 12; i++) {
            buffer_write_i32(&buf, i == 1 ? welcomes[w].difficulty : welcome_rules[i]);
        }
        buffer_write_i32(&buf, 4); // save size
        buffer_write_u32(&buf, 0);
        int size = buf.index;
        buffer_set(&buf, 0);
        buffer_write_i32(&buf, size - 4);
        net_send(host, welcome, size);
        poll_lockstep_for(200);
        printf("after a welcome with %s: state %d, %s\n", welcomes[w].what, mp_lockstep_get_state(),
            mp_lockstep_status());
        if (mp_lockstep_get_state() != MP_LOCKSTEP_DISCONNECTED ||
            strcmp(mp_lockstep_status(), "Message de l'hôte invalide") != 0) {
            printf("WRONG: a welcome message with %s is not refused\n", welcomes[w].what);
            result = 1;
        }
    }
    mp_lockstep_stop();
    net_close(host);
    net_close(listener);
    char name[64];
    snprintf(name, sizeof(name), "mp-session-%d-p1.mpsav", port);
    remove(name);
    printf("%s\n", result ? "FAILED" : "rules out of their bounds are refused");
    return result;
}

static int same_rules(const game_rules_settings *a, const game_rules_settings *b)
{
    return same_lobby_rules(a, b) && a->territories == b->territories && a->score_years == b->score_years &&
        a->fix_immigration_bug == b->fix_immigration_bug && a->fix_100_year_ghosts == b->fix_100_year_ghosts;
}

// K-review-fixes (T4.11, D-073): a multiplayer game resumed from the lobby (.mpsav) goes on with its saved rules
// (territories, fog, score...): the lobby shows them and cannot change them, the rules changed before and after
// "Host" are not those of the game
// K-review-fixes (T4.11): shortens the piece mp_header of a .mpsav to SIZE bytes, as written before the last rules
static int shorten_mp_header(const char *filename, int size)
{
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        return 0;
    }
    fseek(fp, 0, SEEK_END);
    long length = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    uint8_t *bytes = malloc(length);
    int ok = bytes && fread(bytes, 1, length, fp) == (size_t) length;
    fclose(fp);
    // after the magic and the version: name length, name, size of the data, data
    long at = 8;
    ok = ok && length > at + 1 && bytes[at] == 9 && memcmp(&bytes[at + 1], "mp_header", 9) == 0;
    if (ok) {
        buffer buf;
        buffer_init(&buf, &bytes[at + 10], 4);
        int old_size = buffer_read_i32(&buf);
        ok = size < old_size;
        if (ok) {
            buffer_init(&buf, &bytes[at + 10], 4);
            buffer_write_i32(&buf, size);
            fp = fopen(filename, "wb");
            long cut_from = at + 14 + size, cut_to = at + 14 + old_size;
            ok = fp && fwrite(bytes, 1, cut_from, fp) == (size_t) cut_from &&
                fwrite(&bytes[cut_to], 1, length - cut_to, fp) == (size_t) (length - cut_to);
            if (fp) {
                fclose(fp);
            }
        }
    }
    free(bytes);
    return ok;
}

static int command_resumerules(const char *file, int port, int header_size)
{
    char save_file[64];
    snprintf(save_file, sizeof(save_file), "resumerules-%d.mpsav", port);
    if (!mp_mapgen_create_prepared(file, 1, 0)) {
        printf("Unable to create the prepared map\n");
        return 2;
    }
    game_rules_settings saved;
    game_rules_default_multiplayer_settings(&saved);
    saved.difficulty = DIFFICULTY_VERY_HARD;
    saved.gods_enabled = 0;
    saved.ai_invasions = 0;
    saved.fog_of_war = 0;
    saved.territories = 1;
    saved.end_condition = GAME_END_CAESAR;
    saved.caesar_score = 1500;
    saved.score_years = 7;
    game_rules_set_multiplayer(&saved);
    if (!mp_savegame_write(save_file)) {
        printf("Unable to write %s\n", save_file);
        return 2;
    }
    if (header_size) {
        // a game saved before the last rules: those missing read as 0 (6 numbers of the map, then the rules)
        if (!shorten_mp_header(save_file, header_size)) {
            printf("Unable to shorten the header of %s\n", save_file);
            remove(save_file);
            return 2;
        }
        int rules_read = (header_size - 6 * 4) / 4 - 1; // after the mode
        if (rules_read < 10) {
            saved.caesar_score = 0;
        }
        if (rules_read < 9) {
            saved.fog_of_war = 0;
        }
        if (rules_read < 8) {
            saved.territories = 0;
        }
        printf("header of %d bytes: %d rules saved\n", header_size, rules_read);
    }
    // the lobby proposes other rules, as the window does before "Host"
    mp_lobby_rules_init();
    game_rules_settings lobby;
    mp_lobby_rules_settings(&lobby);
    mp_lockstep_set_rules(&lobby);
    int result = 0;
    if (!mp_lockstep_host(port, 1, save_file, 1)) {
        printf("FAILED: %s\n", mp_lockstep_status());
        remove(save_file);
        return 1;
    }
    mp_lockstep_set_manual_start(1);
    mp_lockstep_set_generated_map(0, 1); // a game goes on, as saved
    print_lobby_rules("saved rules", &saved);
    print_lobby_rules("lobby rules", &lobby);
    print_lobby_rules("rules shown after hosting", mp_lobby_rules_shown());
    if (mp_lobby_rules_editable()) {
        printf("WRONG: the lobby lets the host change the rules of a saved game\n");
        result = 1;
    }
    if (!mp_lockstep_lobby_rules() || !same_rules(mp_lockstep_lobby_rules(), &saved) ||
        !same_rules(mp_lobby_rules_shown(), &saved)) {
        printf("WRONG: the lobby does not show the rules of the saved game\n");
        result = 1;
    }
    change_every_lobby_rule();
    mp_lobby_start_game();
    for (int frame = 0; frame < 100 && mp_lockstep_get_state() == MP_LOCKSTEP_WAITING_FOR_PLAYERS; frame++) {
        mp_lockstep_poll();
    }
    const game_rules_settings *game = game_rules_multiplayer_settings();
    print_lobby_rules("game rules", game);
    printf("game territories %d, score years %d\n", game->territories, game->score_years);
    if (mp_lockstep_get_state() != MP_LOCKSTEP_RUNNING) {
        printf("FAILED: the game did not start: %s\n", mp_lockstep_status());
        result = 1;
    } else if (!same_rules(game, &saved) || game_rules_territories() != saved.territories ||
        game_rules_fog_of_war() != saved.fog_of_war || game_rules_difficulty() != DIFFICULTY_VERY_HARD) {
        printf("WRONG: the resumed game does not keep its saved rules\n");
        result = 1;
    }
    mp_lockstep_stop();
    char name[64];
    snprintf(name, sizeof(name), "mp-session-%d-p0.mpsav", port);
    remove(name);
    remove(save_file);
    printf("%s\n", result ? "FAILED" : "the resumed game keeps its saved rules");
    return result;
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
        mp_session_init_network(p, NULL); // the menu is the one of the local player: here, player p
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
    mp_session_init_offline();
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
            } else if (t & TERRAIN_AQUEDUCT) {
                c[0] = 120; c[1] = 230; c[2] = 250;
            } else if (t & TERRAIN_BUILDING) {
                c[0] = 200; c[1] = 60; c[2] = 60;
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
                (map_terrain_is(map_grid_offset(nx, ny), TERRAIN_WATER | TERRAIN_ROCK) &&
                !map_terrain_is(map_grid_offset(nx, ny), TERRAIN_ROAD))) {
                continue; // water and rocks stop walkers, but not bridges
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

// tiles of this terrain around a place, on the map only: the grid around it holds what the template left there
static int count_terrain_near(int cx, int cy, int radius, int terrain)
{
    int count = 0;
    for (int y = cy - radius; y <= cy + radius; y++) {
        for (int x = cx - radius; x <= cx + radius; x++) {
            count += map_grid_is_inside(x, y, 1) && map_terrain_is(map_grid_offset(x, y), terrain) ? 1 : 0;
        }
    }
    return count;
}

// water that ships cannot reach from the ends of the sea: ponds, which would water the city of the rocks (D-055)
static int count_ponds(int size)
{
    static uint8_t seen[GRID_MAX_SIZE * GRID_MAX_SIZE];
    static int queue[GRID_MAX_SIZE * GRID_MAX_SIZE];
    memset(seen, 0, sizeof(seen));
    int head = 0, tail = 0;
    for (int east = 0; east < 2; east++) {
        int x, y;
        mp_mapgen_sea_end(east, &x, &y);
        queue[tail++] = y * size + x;
        seen[y * size + x] = 1;
    }
    while (head < tail) {
        int i = queue[head++];
        int x = i % size, y = i / size;
        static const int DX[] = { 1, -1, 0, 0 };
        static const int DY[] = { 0, 0, 1, -1 };
        for (int d = 0; d < 4; d++) {
            int nx = x + DX[d], ny = y + DY[d];
            if (nx < 0 || ny < 0 || nx >= size || ny >= size || seen[ny * size + nx] ||
                !map_terrain_is(map_grid_offset(nx, ny), TERRAIN_WATER)) {
                continue;
            }
            seen[ny * size + nx] = 1;
            queue[tail++] = ny * size + nx;
        }
    }
    int ponds = 0;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            ponds += !seen[y * size + x] && map_terrain_is(map_grid_offset(x, y), TERRAIN_WATER);
        }
    }
    return ponds;
}

// what the plan of the prepared maps gives or refuses to each arrival point: food and raw materials (T4.15, D-062)
static const int PLAN_RESOURCES[] = { RESOURCE_WHEAT, RESOURCE_VEGETABLES, RESOURCE_FRUIT, RESOURCE_OLIVES,
    RESOURCE_VINES, RESOURCE_MEAT, RESOURCE_IRON, RESOURCE_TIMBER, RESOURCE_CLAY, RESOURCE_MARBLE };
#define NUM_PLAN_RESOURCES (int) (sizeof(PLAN_RESOURCES) / sizeof(PLAN_RESOURCES[0]))

// whether the permissions of the city of a player, which its saved game carries, are those of his arrival point
static int permissions_match_slot(int player_id)
{
    int ok = 1;
    player_context_switch(player_id);
    for (int i = 0; i < NUM_PLAN_RESOURCES; i++) {
        int resource = PLAN_RESOURCES[i];
        if (empire_city_our_production_allowed(resource) != mp_mapgen_slot_allows(player_id, resource)) {
            printf("  player %d: permission for resource %d does not match the map\n", player_id + 1, resource);
            ok = 0;
        }
    }
    player_context_switch(0);
    return ok;
}

// a place for a farm near the city of a player: three by three tiles of clear land or meadow, some meadow, away from
// the rows of the houses, of the start mission and of the reservoirs that the test builds
static int find_farm_place(int player_id, int *fx, int *fy)
{
    int cx, cy;
    mp_mapgen_city_center(player_id, &cx, &cy);
    for (int d = 4; d <= 22; d++) {
        for (int y = cy - d; y <= cy + d; y++) {
            for (int x = cx - d; x <= cx + d; x++) {
                int dx = x > cx ? x - cx : cx - x, dy = y > cy ? y - cy : cy - y;
                if ((dx > dy ? dx : dy) != d || (y + 2 >= cy - 7 && y <= cy + 3)) {
                    continue;
                }
                int clear = 1, meadow = 0;
                for (int yy = y; yy < y + 3; yy++) {
                    for (int xx = x; xx < x + 3; xx++) {
                        int terrain = map_terrain_get(map_grid_offset(xx, yy));
                        clear &= (terrain & ~TERRAIN_MEADOW) == 0;
                        meadow += (terrain & TERRAIN_MEADOW) != 0;
                    }
                }
                if (clear && meadow) {
                    *fx = x;
                    *fy = y;
                    return 1;
                }
            }
        }
    }
    return 0;
}

static int building_type_at(int x, int y)
{
    return building_get(map_building_at(map_grid_offset(x, y)))->type;
}

// the food of the plan in the build menu and in the build commands (T4.15, D-065): pig farms inland only, fruit
static void ignore_command(mp_command *command);

// farms on the coast only. Pig farms and wharves both give meat: the coast fishes, but raises no pigs
static int farms_follow_plan(int player_id)
{
    int inland = !mp_mapgen_slot_is_coastal(player_id);
    int x, y;
    if (!find_farm_place(player_id, &x, &y)) {
        printf("  player %d: no place for a farm near the city\n", player_id + 1);
        return 0;
    }
    // the build menu is the local player's only (D-064): look at it as this player
    player_context_switch(player_id);
    mp_session_init_network(player_id, ignore_command);
    building_menu_update();
    mp_session_init_offline();
    int pig_menu = building_menu_is_enabled(BUILDING_PIG_FARM);
    int fruit_menu = building_menu_is_enabled(BUILDING_FRUIT_FARM);
    int wheat_menu = building_menu_is_enabled(BUILDING_WHEAT_FARM) && building_menu_is_enabled(BUILDING_VEGETABLE_FARM);
    int wharf_menu = building_menu_is_enabled(BUILDING_WHARF); // the coast fishes
    player_context_switch(0);
    int refused = inland ? BUILDING_FRUIT_FARM : BUILDING_PIG_FARM;
    int allowed = inland ? BUILDING_PIG_FARM : BUILDING_FRUIT_FARM;
    mp_command refused_command = { .type = MP_COMMAND_BUILD, .player_id = player_id, .args = { refused, 0, x, y, x, y, 0, 0 } };
    mp_command_execute(&refused_command);
    int refused_built = building_type_at(x, y) == refused;
    mp_command allowed_command = { .type = MP_COMMAND_BUILD, .player_id = player_id, .args = { allowed, 0, x, y, x, y, 0, 0 } };
    mp_command_execute(&allowed_command);
    int allowed_built = building_type_at(x, y) == allowed;
    int ok = pig_menu == inland && fruit_menu == !inland && wheat_menu && (inland || wharf_menu) && !refused_built &&
        allowed_built;
    printf("player %d (%s): menu pigs %s, fruit %s, wheat and vegetables %s, wharf %s; at (%d, %d) %s farm %s, "
        "%s farm %s\n", player_id + 1, inland ? "inland" : "coast", pig_menu ? "yes" : "no", fruit_menu ? "yes" : "no",
        wheat_menu ? "yes" : "no", wharf_menu ? "yes" : "no", x, y, inland ? "fruit" : "pig", refused_built ? "BUILT" : "refused",
        inland ? "pig" : "fruit", allowed_built ? "built" : "NOT BUILT");
    return ok;
}

// the prepared maps (D-033, D-047): all land reached from the main road (over the bridge of Caesar), an arm of the
// sea from edge to edge, each arrival point with its materials only, no water near the player of the rocks but the
// aqueduct of Caesar, the others on the coast, permissions that match, the same map every time, cities that grow
static int command_preparedmap(const char *file, int num_players, int ticks)
{
    if (!create_prepared(file, num_players, 0)) {
        printf("Unable to create the prepared map\n");
        return 2;
    }
    int size = prepared_size(num_players);
    printf("prepared map %d for %d players: %d tiles wide\n", options.prepared_map + 1, num_players, size);
    if (getenv("MAPGEN_PICTURE")) {
        write_map_picture(getenv("MAPGEN_PICTURE"), size, 3);
    }
    int failures = 0;
    if (map_grid_width() != size || map_grid_height() != size) {
        printf("  the map is %d by %d tiles\n", map_grid_width(), map_grid_height());
        failures++;
    }
    int unreachable = count_unreachable_land(size);
    printf("land out of reach of the main road: %d tiles\n", unreachable);
    failures += unreachable != 0;
    // a map of forests and of the sea, without ponds (D-044, D-055)
    int ponds = count_ponds(size);
    printf("water away from the sea: %d tiles\n", ponds);
    failures += ponds != 0;
    int water = count_terrain_near(size / 2, size / 2, size / 2 - 1, TERRAIN_WATER);
    int trees = count_terrain_near(size / 2, size / 2, size / 2 - 1, TERRAIN_TREE);
    printf("water: %d%% of the map, forest: %d%%, climate %d\n", 100 * water / (size * size),
        100 * trees / (size * size), scenario_property_climate());
    // fewer woods since D-052 (mp_prepared_map_reachable: 12 to 18 %)
    if (100 * water / (size * size) < 8 || 100 * trees / (size * size) < 12 ||
        scenario_property_climate() != CLIMATE_NORTHERN) {
        printf("  not a map of forests and of the sea\n");
        failures++;
    }
    // the meadows show as meadows: drawn after the grass of empty land, as when the game loads a map
    int meadows = 0, hidden_meadows = 0;
    int meadow_image = image_group(GROUP_TERRAIN_MEADOW);
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int o = map_grid_offset(x, y);
            if (map_terrain_get(o) == TERRAIN_MEADOW) {
                meadows++;
                hidden_meadows += map_image_at(o) < meadow_image || map_image_at(o) >= meadow_image + 12;
            }
        }
    }
    printf("meadows: %d tiles, %d drawn as something else\n", meadows, hidden_meadows);
    failures += meadows == 0 || hidden_meadows != 0;
    int fishing = 0;
    for (int i = 0; i < MAX_FISH_POINTS; i++) {
        map_point fish = scenario_editor_fishing_point(i);
        fishing += fish.x >= 0 && map_terrain_is(map_grid_offset(fish.x, fish.y), TERRAIN_WATER);
    }
    int map_players = num_players <= 2 ? 2 : 4;
    int expected_fishing = 0;
    for (int p = 0; p < map_players; p++) {
        expected_fishing += mp_mapgen_slot_is_coastal(p);
    }
    printf("fishing points in the water: %d of %d\n", fishing, expected_fishing);
    failures += fishing != expected_fishing;
    for (int p = 0; p < num_players; p++) {
        int cx, cy;
        mp_mapgen_city_center(p, &cx, &cy);
        int water = count_terrain_near(cx, cy, 26, TERRAIN_WATER);
        int rock = count_terrain_near(cx, cy, 26, TERRAIN_ROCK);
        int trees = count_terrain_near(cx, cy, 26, TERRAIN_TREE);
        int meadow = count_terrain_near(cx, cy, 26, TERRAIN_MEADOW);
        int wants_rock = mp_mapgen_slot_allows(p, RESOURCE_IRON) || mp_mapgen_slot_allows(p, RESOURCE_MARBLE);
        int wants_trees = mp_mapgen_slot_allows(p, RESOURCE_TIMBER);
        int wants_water = mp_mapgen_slot_is_coastal(p); // the player of the rocks has only the aqueduct (D-047)
        int far_water = count_terrain_near(cx, cy, 45, TERRAIN_WATER);
        // the coast is in the zone the player starts with: he may build his docks at once
        int own_water = 0;
        for (int y = cy - 26; y <= cy + 26; y++) {
            for (int x = cx - 26; x <= cx + 26; x++) {
                int o = map_grid_offset(x, y);
                own_water += map_terrain_is(o, TERRAIN_WATER) && mp_territory_owner(o) == p;
            }
        }
        printf("player %d: water %d (%d in his zone, %d within 45 tiles), rock %d, trees %d, meadow %d\n", p + 1,
            water, own_water, far_water, rock, trees, meadow);
        if ((water > 0) != wants_water || (rock > 0) != wants_rock || (trees > 0) != wants_trees || meadow < 50 ||
            (!wants_water && far_water > 0) || (wants_water && own_water < 30)) {
            printf("  the land around player %d does not match its materials\n", p + 1);
            failures++;
        }
        player_context_switch(p);
        // what this city may produce: its empire allows the food, the map its raw materials
        static const char *NAMES[] = { "", "wheat", "vegetables", "fruit", "olives", "vines", "meat", "wine", "oil",
            "iron", "timber", "clay", "marble", "weapons", "furniture", "pottery" };
        printf("player %d may produce:", p + 1);
        for (int r = RESOURCE_MIN; r <= RESOURCE_POTTERY; r++) {
            if (empire_city_our_production_allowed(r)) {
                printf(" %s", NAMES[r]);
            }
        }
        printf("\n");
        player_context_switch(0);
        // food and raw materials: those of the plan (T4.15, D-062)
        failures += !permissions_match_slot(p);
        int ex, ey;
        mp_mapgen_entry_point(p, &ex, &ey);
        int entry = map_grid_offset(ex, ey);
        if (!map_terrain_is(entry, TERRAIN_ROAD) || map_owner_get_claimed(entry) != MAP_OWNER_CAESAR) {
            printf("  player %d: no road of Caesar at the arrival point\n", p + 1);
            failures++;
        }
    }
    // the arm of the sea goes from edge to edge, ships sail under the bridge of Caesar, which his main road crosses;
    // the ships of the empire of each player on the coast come from the nearest edge to his coast; caravans come by
    // the main road (checked above)
    int wx, wy, ex, ey;
    mp_mapgen_sea_end(0, &wx, &wy);
    mp_mapgen_sea_end(1, &ex, &ey);
    map_routing_calculate_distances_water_boat(wx, wy);
    int sailing = scenario_map_has_river_entry() && map_routing_distance(map_grid_offset(ex, ey)) > 0;
    printf("sea from (%d, %d) to (%d, %d): %s\n", wx, wy, ex, ey, sailing ? "ships sail across the map" : "NO WAY");
    failures += !sailing;
    int bx, by_north, by_south, bridge_ok = 1;
    mp_mapgen_caesar_bridge(&bx, &by_north, &by_south);
    for (int y = by_north; y <= by_south; y++) {
        int o = map_grid_offset(bx, y);
        bridge_ok &= map_is_bridge(o) && map_terrain_is(o, TERRAIN_ROAD) && map_owner_get_claimed(o) == MAP_OWNER_CAESAR;
    }
    printf("bridge of Caesar at column %d, from %d to %d (%d tiles): %s\n", bx, by_north, by_south,
        by_south - by_north + 1, bridge_ok ? "a road of Caesar over the sea" : "BROKEN");
    failures += !bridge_ok;
    for (int p = 0; p < num_players; p++) {
        if (!mp_mapgen_slot_is_coastal(p)) {
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
            reached && entry_ok ? "sail to his coast" : "DO NOT REACH HIS COAST");
        failures += !reached || !entry_ok;
    }

    uint64_t first = mp_checksum_state();
    player_context_switch(0);
    player_context_set_num_players(1);
    if (!create_prepared(file, num_players, 0) || mp_checksum_state() != first) {
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
    // the clients only get the saved game: it carries the permissions of every city, food included
    for (int p = 0; p < num_players; p++) {
        failures += !permissions_match_slot(p);
        failures += !farms_follow_plan(p);
    }

    // the players without water build a reservoir at the end of the aqueduct of Caesar: it fills; another one,
    // away from any water, stays dry. On the map for 4, two players live inland, also with three players (D-062)
    int inland_players = 0, fed_players = 0;
    for (int p = 0; p < num_players; p++) {
        inland_players += !mp_mapgen_slot_is_coastal(p);
    }
    for (int p = 0; p < num_players; p++) {
        int ax, ay;
        if (!mp_mapgen_caesar_aqueduct_end(p, &ax, &ay)) {
            continue;
        }
        fed_players++;
        int cx, cy;
        mp_mapgen_city_center(p, &cx, &cy);
        // the aqueduct of Caesar comes from the east along a row, on every prepared map: a reservoir west of its end
        int x = ax - 2;
        int y = ay;
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
    int expected_inland = num_players <= 2 ? 1 : 2;
    printf("players inland: %d, served by the aqueduct of Caesar: %d, expected %d\n", inland_players, fed_players,
        expected_inland);
    failures += inland_players != expected_inland || fed_players != expected_inland;

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
    if (!create_prepared(file, 2, 0)) {
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

// the aqueduct of Caesar is the only water of the city of the rocks (D-034, D-055): no pond on the map; his reservoir
// at the end of the aqueduct fills and waters its range. Once the aqueduct is cut, as soldiers will do at war (M10),
// the reservoir keeps serving while it holds water, then dries up with its range; mended, it fills again. Alone on
// the map, as Alexandre tries every version, as well as with others.
static int command_drying(const char *file, int num_players)
{
    // as a game prepared by the lobby: the map goes through a file, then the rules of the game come, with
    // territories, alone too; no enemy army comes to destroy the reservoir
    char map_file[64];
    snprintf(map_file, sizeof(map_file), "drying-%d.mpmap", num_players);
    if (!create_prepared(file, num_players, 0) || !mp_savegame_write(map_file) ||
        !mp_savegame_read(map_file)) {
        printf("Unable to create the prepared map\n");
        return 2;
    }
    remove(map_file);
    game_rules_settings rules;
    game_rules_default_multiplayer_settings(&rules);
    rules.territories = 1;
    rules.ai_invasions = 0;
    game_rules_set_multiplayer(&rules);
    int size = prepared_size(num_players);
    int failures = 0;
    int ponds = count_ponds(size);
    printf("water away from the sea: %d tiles\n", ponds);
    failures += ponds != 0;
    int p = 0, ax, ay;
    while (p < num_players && !mp_mapgen_caesar_aqueduct_end(p, &ax, &ay)) {
        p++;
    }
    if (p == num_players) {
        printf("Nobody has the aqueduct of Caesar\n");
        return 2;
    }
    // the aqueduct comes from the east along a row: a reservoir at its end, the cut a few tiles upstream
    int cut_x = ax + 5;
    int cut = map_grid_offset(cut_x, ay);
    if (!map_terrain_is(cut, TERRAIN_AQUEDUCT) || map_owner_get_claimed(cut) != MAP_OWNER_CAESAR) {
        printf("No aqueduct of Caesar at (%d, %d)\n", cut_x, ay);
        return 2;
    }
    mp_command reservoir = { .type = MP_COMMAND_BUILD, .player_id = p, .args = { BUILDING_DRAGGABLE_RESERVOIR, 0, ax - 2, ay, ax - 2, ay, 0, 0 } };
    mp_command_execute(&reservoir);
    int ranged = map_grid_offset(ax - 2, ay + 8); // a fountain there would have water from the reservoir
    mp_command clear = { .type = MP_COMMAND_BUILD, .player_id = p, .args = { BUILDING_CLEAR_LAND, 0, cut_x, ay, cut_x, ay, 0, 0 } };
    mp_command mend = { .type = MP_COMMAND_BUILD, .player_id = p, .args = { BUILDING_AQUEDUCT, 0, cut_x, ay, cut_x, ay, 0, 0 } };
    int full = map_water_supply_reservoir_level_full();
    setting_reset_speeds(500, setting_scroll_speed()); // one tick per run_one_tick()
    struct { int days; int cut_it; int mend_it; int water; int is_full; const char *label; } steps[] = {
        { 60, 0, 0, 1, 1, "fed by Caesar, 60 days" },
        { 100, 1, 0, 1, 0, "cut off, 100 days later" },
        { 200, 0, 0, 0, 0, "300 days after the cut" },
        { 3, 0, 1, 1, 0, "mended, 3 days later" },
    };
    for (int i = 0; i < 4; i++) {
        if (steps[i].cut_it) {
            // war will let soldiers break the aqueduct of Caesar (D-034): here the player clears it himself
            map_owner_set(cut, MAP_OWNER_NONE);
            mp_command_execute(&clear);
            if (map_terrain_is(cut, TERRAIN_AQUEDUCT)) {
                printf("The aqueduct of Caesar is not cut\n");
                return 2;
            }
        }
        if (steps[i].mend_it) {
            mp_command_execute(&mend);
        }
        for (int day = 0; day < steps[i].days; day++) {
            for (int tick = 0; tick < 50; tick++) {
                run_one_tick();
            }
            // no engineer here: keep the reservoir from collapsing
            building_get(map_building_at(map_grid_offset(ax - 2, ay)))->damage_risk = 0;
        }
        player_context_switch(p);
        int level;
        int water = reservoir_at(ax - 2, ay, &level);
        int range = map_water_supply_has_range(ranged, TERRAIN_RESERVOIR_RANGE);
        player_context_switch(0);
        int ok = water == steps[i].water && range == steps[i].water && (!steps[i].is_full || level == full) &&
            (steps[i].water || level == 0);
        printf("%-26s reservoir of player %d %s (%d/%d), fountains %s%s\n", steps[i].label, p + 1,
            water ? "water" : "dry", level, full, range ? "with water" : "dry", ok ? "" : "  <-- UNEXPECTED");
        failures += !ok;
    }
    player_context_switch(0);
    player_context_set_num_players(1);
    printf("%s\n", failures ? "DIFFERENT: the city of the rocks does not dry up when the aqueduct of Caesar is cut" :
        "Identical: the city of the rocks dries up when the aqueduct of Caesar is cut");
    return failures ? 1 : 0;
}

static int build_as(int player_id, int type, int x1, int y1, int x2, int y2);

// A reservoir of the player near (cx, cy), in his zone and away from water, with an aqueduct of 4 tiles leaving its
// west side from (*ax, *ay) westwards. Returns the id of the reservoir, 0 when no place was found.
static int own_reservoir_with_aqueduct(int player_id, int cx, int cy, int *ax, int *ay)
{
    for (int dy = 4; dy <= 16; dy += 4) {
        for (int dx = -12; dx <= 12; dx += 4) {
            int x = cx + dx, y = cy + dy;
            if (map_terrain_exists_tile_in_area_with_type(x - 2, y - 2, 7, TERRAIN_WATER | TERRAIN_AQUEDUCT) ||
                !build_as(player_id, BUILDING_DRAGGABLE_RESERVOIR, x, y, x, y)) {
                continue;
            }
            int id = map_building_at(map_grid_offset(x, y));
            building *r = building_get(id);
            *ax = r->x - 1;
            *ay = r->y + 1;
            build_as(player_id, BUILDING_AQUEDUCT, *ax, *ay, *ax - 3, *ay);
            int joined = 1;
            for (int i = 0; i < 4; i++) {
                joined &= map_terrain_is(map_grid_offset(*ax - i, *ay), TERRAIN_AQUEDUCT);
            }
            if (joined && r->type == BUILDING_RESERVOIR) {
                return id;
            }
        }
    }
    return 0;
}

// the aqueduct of Caesar waters the player inland, wherever his arrival point is drawn (T4.9): every city fills the
// aqueducts in turn on the same grid, the water of a pass must not stop the next one. Returns the number of failures
// and the checksum of the state at the end, which must not depend on anything hidden
static int inland_water_game(const char *file, int num_players, int seed, int p, uint64_t *checksum)
{
    char map_file[64];
    snprintf(map_file, sizeof(map_file), "inlandwater-%d-%d.mpmap", num_players, p);
    player_context_switch(0);
    player_context_set_num_players(1);
    if (!create_prepared(file, num_players, seed) || !mp_savegame_write(map_file) ||
        !mp_savegame_read(map_file)) {
        printf("Unable to create the prepared map\n");
        return -1;
    }
    remove(map_file);
    game_rules_settings rules;
    game_rules_default_multiplayer_settings(&rules);
    rules.territories = 1;
    rules.ai_invasions = 0;
    game_rules_set_multiplayer(&rules);
    int ax, ay;
    if (mp_mapgen_slot_is_coastal(p) || !mp_mapgen_caesar_aqueduct_end(p, &ax, &ay)) {
        printf("Player %d is not the one inland\n", p + 1);
        return -1;
    }
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("  %-70s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    int built = build_as(p, BUILDING_DRAGGABLE_RESERVOIR, ax - 2, ay, ax - 2, ay);
    built &= build_as(p, BUILDING_FOUNTAIN, ax - 2, ay + 6, ax - 2, ay + 6);
    built &= build_as(p, BUILDING_HOUSE_VACANT_LOT, ax - 2, ay + 8, ax - 2, ay + 8);
    // an aqueduct apart from everything, for each player: it stays dry
    int dry_x[MP_MAPGEN_MAX_PLAYERS], dry_y[MP_MAPGEN_MAX_PLAYERS];
    for (int q = 0; q < num_players; q++) {
        if (q == p) {
            dry_x[q] = ax - 6;
            dry_y[q] = ay - 4;
        } else {
            mp_mapgen_city_center(q, &dry_x[q], &dry_y[q]);
        }
        build_as(q, BUILDING_AQUEDUCT, dry_x[q], dry_y[q], dry_x[q], dry_y[q]); // not a building: terrain only
        built &= map_terrain_is(map_grid_offset(dry_x[q], dry_y[q]), TERRAIN_AQUEDUCT);
    }
    // K-review-fixes (T4.9): every other player has a reservoir of his own in his zone, joined to an aqueduct of his.
    // Away from any water it stays dry, and so does his aqueduct: the water of the player inland, which the same
    // aqueduct grid carries on the same day, never reaches it. The other player inland (4 players) also has a
    // reservoir at the end of his aqueduct of Caesar: it has water, and so has the aqueduct joined to it.
    int other_reservoir[MP_MAPGEN_MAX_PLAYERS] = { 0 }, other_x[MP_MAPGEN_MAX_PLAYERS], other_y[MP_MAPGEN_MAX_PLAYERS];
    int fed_reservoir[MP_MAPGEN_MAX_PLAYERS] = { 0 }, fed_x[MP_MAPGEN_MAX_PLAYERS], fed_y[MP_MAPGEN_MAX_PLAYERS];
    for (int q = 0; q < num_players && built; q++) {
        if (q == p) {
            continue;
        }
        int cx, cy;
        mp_mapgen_city_center(q, &cx, &cy);
        other_reservoir[q] = own_reservoir_with_aqueduct(q, cx, cy, &other_x[q], &other_y[q]);
        built &= other_reservoir[q] != 0;
        int qx, qy;
        if (!mp_mapgen_slot_is_coastal(q) && mp_mapgen_caesar_aqueduct_end(q, &qx, &qy)) {
            built &= build_as(q, BUILDING_DRAGGABLE_RESERVOIR, qx - 2, qy, qx - 2, qy);
            fed_reservoir[q] = map_building_at(map_grid_offset(qx - 2, qy));
            building *r = building_get(fed_reservoir[q]);
            // his aqueduct leaves the reservoir on the side opposite to the aqueduct of Caesar
            fed_x[q] = r->x - 1;
            fed_y[q] = r->y + 1;
            build_as(q, BUILDING_AQUEDUCT, fed_x[q], fed_y[q], fed_x[q] - 3, fed_y[q]);
            built &= map_terrain_is(map_grid_offset(fed_x[q], fed_y[q]), TERRAIN_AQUEDUCT);
        }
    }
    // K-review-fixes (T4.9): the zones touch (they grow with the cities and missions, D-036). The player inland
    // draws an aqueduct from his reservoir, and the next player q joins his own aqueduct to it, then to a reservoir
    // of his, away from water. The water of the player inland runs in his aqueduct, but stops where q's begins:
    // q's aqueduct and reservoir stay dry (the owner check of fill_aqueducts_from_offset)
    int q_joined = (p + 1) % num_players;
    int own_x = 0, own_y = 0, joined_reservoir = 0;
    if (built) {
        building *r = building_get(map_building_at(map_grid_offset(ax - 2, ay)));
        own_x = r->x - 1; // his aqueduct leaves the west side of his reservoir, 4 tiles westwards
        own_y = r->y + 1;
        build_as(p, BUILDING_CLEAR_LAND, own_x - 12, own_y - 1, own_x, own_y + 1);
        build_as(p, BUILDING_AQUEDUCT, own_x, own_y, own_x - 3, own_y);
        build_as(q_joined, BUILDING_AQUEDUCT, own_x - 4, own_y, own_x - 6, own_y);
        // q's reservoir, east of which his aqueduct ends: built where his zone has grown (here: territories off a
        // moment, the water does not look at zones but at who owns each aqueduct tile)
        game_rules_settings no_zones = rules;
        no_zones.territories = 0;
        game_rules_set_multiplayer(&no_zones);
        build_as(q_joined, BUILDING_DRAGGABLE_RESERVOIR, own_x - 8, own_y, own_x - 8, own_y);
        game_rules_set_multiplayer(&rules);
        joined_reservoir = map_building_at(map_grid_offset(own_x - 8, own_y));
        building *qr = building_get(joined_reservoir);
        for (int i = 0; i < 7; i++) {
            int o = map_grid_offset(own_x - i, own_y);
            built &= map_terrain_is(o, TERRAIN_AQUEDUCT) && map_owner_get_claimed(o) == (i < 4 ? p : q_joined);
        }
        built &= qr->type == BUILDING_RESERVOIR && BUILDING_OWNER(joined_reservoir) == q_joined &&
            qr->x + 3 == own_x - 6 && qr->y + 1 == own_y;
        if (!built) {
            printf("Unable to join the aqueduct of player %d to that of player %d\n", q_joined + 1, p + 1);
        }
    }
    if (!built) {
        printf("Unable to build in the zones of the players (reservoir %d fountain %d house %d)\n",
            building_get(map_building_at(map_grid_offset(ax - 2, ay)))->type,
            building_get(map_building_at(map_grid_offset(ax - 2, ay + 6)))->type,
            building_get(map_building_at(map_grid_offset(ax - 2, ay + 8)))->type);
        return -1;
    }
    setting_reset_speeds(500, setting_scroll_speed());
    int reservoir_id = map_building_at(map_grid_offset(ax - 2, ay));
    int fountain_id = map_building_at(map_grid_offset(ax - 2, ay + 6));
    int house_id = map_building_at(map_grid_offset(ax - 2, ay + 8));
    // two days, then to the tick before the water: the fountain has a worker (nobody lives here yet)
    for (int day = 0; day < 2; day++) {
        while (game_time_tick() != 27) {
            run_one_tick();
            building_get(reservoir_id)->damage_risk = 0;
        }
        player_context_switch(p);
        building_get(fountain_id)->num_workers = 5;
        player_context_switch(0);
        run_one_tick();
        run_one_tick();
    }
    player_context_switch(p);
    building *reservoir = building_get(reservoir_id);
    building *fountain = building_get(fountain_id);
    building *house = building_get(house_id);
    printf("player %d inland, aqueduct of Caesar ends at (%d, %d)\n", p + 1, ax, ay);
    CHECK(reservoir->type == BUILDING_RESERVOIR && reservoir->has_water_access, "his reservoir has water");
    CHECK(map_water_supply_has_range(fountain->grid_offset, TERRAIN_RESERVOIR_RANGE), "the fountain is in the water range");
    CHECK(fountain->type == BUILDING_FOUNTAIN && fountain->has_water_access, "the fountain has water");
    CHECK(house->house_size && house->has_water_access, "the house near the fountain is served");
    int caesar_tiles = 0, caesar_wet = 0;
    for (int y = 0; y < map_grid_height(); y++) {
        for (int x = 0; x < map_grid_width(); x++) {
            int o = map_grid_offset(x, y);
            if (map_terrain_is(o, TERRAIN_AQUEDUCT) && map_owner_get_claimed(o) == MAP_OWNER_CAESAR) {
                int mine = 0; // the dry aqueducts above, built in the road of Caesar of the city centers
                for (int q = 0; q < num_players; q++) {
                    mine |= x == dry_x[q] && y == dry_y[q];
                }
                if (!mine) {
                    caesar_tiles++;
                    caesar_wet += map_aqueduct_at(o) != 0;
                }
            }
        }
    }
    printf("  aqueduct of Caesar: %d of %d tiles with water\n", caesar_wet, caesar_tiles);
    CHECK(caesar_tiles > 0 && caesar_wet == caesar_tiles, "the whole aqueduct of Caesar carries water");
    for (int q = 0; q < num_players; q++) {
        int o = map_grid_offset(dry_x[q], dry_y[q]);
        CHECK(map_terrain_is(o, TERRAIN_AQUEDUCT) && !map_aqueduct_at(o), q == p ?
            "an aqueduct of his, joined to nothing, stays dry" : "an aqueduct of another player, joined to nothing, stays dry");
    }
    for (int q = 0; q < num_players; q++) {
        if (q == p) {
            continue;
        }
        player_context_switch(q);
        building *own = building_get(other_reservoir[q]);
        int dry = 1;
        for (int i = 0; i < 4; i++) {
            dry &= !map_aqueduct_at(map_grid_offset(other_x[q] - i, other_y[q]));
        }
        printf("  player %d: reservoir away from water %d, its aqueduct %s\n", q + 1, own->has_water_access,
            dry ? "dry" : "with water");
        CHECK(own->type == BUILDING_RESERVOIR && !own->has_water_access && dry,
            "another player's reservoir and aqueduct, away from water, stay dry");
        if (fed_reservoir[q]) {
            building *fed = building_get(fed_reservoir[q]);
            int wet = 1;
            for (int i = 0; i < 4; i++) {
                wet &= map_aqueduct_at(map_grid_offset(fed_x[q] - i, fed_y[q])) != 0;
            }
            CHECK(fed->type == BUILDING_RESERVOIR && fed->has_water_access && wet,
                "the other player inland: his reservoir and his aqueduct have water");
        }
    }
    int own_wet = 1, joined_dry = 1;
    for (int i = 0; i < 7; i++) {
        int wet = map_aqueduct_at(map_grid_offset(own_x - i, own_y)) != 0;
        if (i < 4) {
            own_wet &= wet;
        } else {
            joined_dry &= !wet;
        }
    }
    player_context_switch(p);
    CHECK(building_get(reservoir_id)->has_water_access && own_wet,
        "his reservoir and the aqueduct he draws from it have water");
    player_context_switch(q_joined);
    printf("  player %d joins his aqueduct to that of player %d: reservoir %d, aqueduct %s\n", q_joined + 1, p + 1,
        building_get(joined_reservoir)->has_water_access, joined_dry ? "dry" : "with water");
    CHECK(!building_get(joined_reservoir)->has_water_access && joined_dry,
        "another player's aqueduct joined to his, and its reservoir, stay dry");
    player_context_switch(0);
    *checksum = mp_checksum_state();
#undef CHECK
    return failures;
}

static int command_inlandwater(const char *file, int num_players)
{
    int failures = 0;
    for (int p = 0; p < num_players; p++) {
        int seed = 1;
        // a draw of the arrival points puts the player inland where the plan puts player 1
        for (; seed <= 60; seed++) {
            player_context_switch(0);
            player_context_set_num_players(1);
            if (create_prepared(file, num_players, seed) && !mp_mapgen_slot_is_coastal(p)) {
                break;
            }
        }
        if (seed > 60) {
            printf("No draw puts player %d inland\n", p + 1);
            return 2;
        }
        uint64_t first, second;
        int result = inland_water_game(file, num_players, seed, p, &first);
        if (result < 0) {
            return 2;
        }
        failures += result;
        result = inland_water_game(file, num_players, seed, p, &second);
        if (result < 0) {
            return 2;
        }
        failures += result;
        printf("  same game twice: %016" PRIx64 " and %016" PRIx64 "%s\n", first, second,
            first == second ? "" : "  <-- UNEXPECTED");
        failures += first != second;
    }
    player_context_switch(0);
    player_context_set_num_players(1);
    printf("%s\n", failures ? "DIFFERENT: the aqueduct of Caesar does not water every inland player" :
        "Identical: the aqueduct of Caesar waters every inland player");
    return failures ? 1 : 0;
}

static int build_as(int player_id, int type, int x1, int y1, int x2, int y2)
{
    mp_command command = { .type = MP_COMMAND_BUILD, .player_id = player_id, .args = { type, 0, x1, y1, x2, y2, 0, 0 } };
    mp_command_execute(&command);
    return building_get(map_building_at(map_grid_offset(x1, y1)))->type != BUILDING_NONE;
}

// territories (MT.1, D-036, D-045): every player starts with a mission and its zone, builds only in it; inhabited
// houses push it further; the other player builds nothing there but roads
static int command_territory(const char *file)
{
    if (!create_prepared(file, 2, 0)) {
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

    int sx, sy;
    mp_mapgen_start_mission(0, &sx, &sy);
    CHECK(building_get(map_building_at(map_grid_offset(sx, sy)))->type == BUILDING_MISSION_POST,
        "player 1 starts with his mission");
    CHECK(mp_territory_owner(map_grid_offset(sx + 1 + 20, sy)) == 0, "it gives a zone of 20 tiles from the start");
    CHECK(mp_territory_owner(map_grid_offset(sx + 1 + 22, sy)) == -1, "but not further");
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
    if (!create_prepared(file, 2, 0)) {
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
    int sx, sy;
    mp_mapgen_start_mission(0, &sx, &sy);
    CHECK(building_get(map_building_at(map_grid_offset(sx, sy)))->type == BUILDING_MISSION_POST,
        "and with his mission, built for him");
    CHECK(!build_as(0, BUILDING_MISSION_POST, mx + 30, my, mx + 30, my), "no mission 30 tiles from the missionary");

    // a warehouse on the main road, in the zone, without marble: the next mission waits for marble
    build_as(0, BUILDING_WAREHOUSE, mx - 10, my - 2, mx - 10, my - 2);
    building *warehouse = building_get(map_building_at(map_grid_offset(mx - 10, my - 2)));
    CHECK(!build_as(0, BUILDING_MISSION_POST, mx + 4, my, mx + 4, my), "the next mission needs marble");
    // T4.7 (D-067): a further mission costs 30 loads, one load short is not enough
    CHECK(MP_MISSION_MARBLE_LOADS == 30, "a further mission costs 30 loads of marble");
    for (int i = 0; i < MP_MISSION_MARBLE_LOADS - 1; i++) {
        building_warehouse_add_resource(warehouse, RESOURCE_MARBLE);
    }
    run_trace(50, 50, 0, 0);
    CHECK(city_resource_count(RESOURCE_MARBLE) == MP_MISSION_MARBLE_LOADS - 1 &&
        !build_as(0, BUILDING_MISSION_POST, mx + 4, my, mx + 4, my), "29 loads of marble are not enough");
    for (int i = 0; i < 3; i++) {
        building_warehouse_add_resource(warehouse, RESOURCE_MARBLE);
    }
    run_trace(50, 50, 0, 0);
    int marble = city_resource_count(RESOURCE_MARBLE);
    int treasury = city_finance_treasury();
    CHECK(build_as(0, BUILDING_MISSION_POST, mx + 4, my, mx + 4, my), "with marble, it is built 4 tiles from him");
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
    if (!create_prepared(file, 2, 0)) {
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
    int sx, sy;
    mp_mapgen_start_mission(0, &sx, &sy);
    CHECK(build_as(0, BUILDING_PREFECTURE, mx + 10, my + 5, mx + 10, my + 5),
        "a prefecture in the zone of the starting mission");
    build_as(0, BUILDING_CLEAR_LAND, sx, sy, sx + 1, sy + 1);
    CHECK(building_get(map_building_at(map_grid_offset(sx, sy)))->state != BUILDING_STATE_IN_USE,
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

// the missionary crosses the bridge of Caesar to the other shore (reported by Alexandre, 2026-10-06)
static int command_missionarybridge(const char *file)
{
    if (!create_prepared(file, 2, 0)) {
        printf("Unable to create the prepared map\n");
        return 2;
    }
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-62s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    int bx, by_north, by_south;
    mp_mapgen_caesar_bridge(&bx, &by_north, &by_south);
    run_trace(50, 50, 0, 0);
    figure *m = first_missionary(0);
    CHECK(m != 0, "player 1 has a missionary");
    if (m) {
        CHECK(walk_missionary(m, bx, by_south + 2), "he walks to the south end of the bridge");
        CHECK(walk_missionary(m, bx, (by_north + by_south) / 2), "he walks onto the bridge");
        CHECK(walk_missionary(m, bx, by_north - 2), "he crosses it to the north shore");
        // the bridge is drawn high above its tiles: a click on it often hits the water next to it
        mp_command beside = { .type = MP_COMMAND_CITY_ACTION, .player_id = 0,
            .args = { MP_ACTION_MISSIONARY_MOVE, m->id, bx - 1, (by_north + by_south) / 2 } };
        mp_command_execute(&beside);
        for (int day = 0; day < 60 && m->action_state == FIGURE_ACTION_221_MP_MISSIONARY_WALKING; day++) {
            run_trace(50, 50, 0, 0);
        }
        CHECK(m->x == bx && m->y == (by_north + by_south) / 2, "sent to the water beside it, he goes onto the bridge");
        printf("missionary at (%d, %d), bridge at column %d from %d to %d\n", m->x, m->y, bx, by_north, by_south);
    }
    player_context_switch(0);
    player_context_set_num_players(1);
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: the missionary cannot cross the bridge" :
        "Identical: the missionary crosses the bridge");
    return failures ? 1 : 0;
}

// whether an arrival point offers what the plan gives its side, inland or on the coast (D-062, T4.15): wheat and
// vegetables for everybody; inland iron, marble and pigs, no timber; on the coast timber, clay and fruit (and fish)
static int slot_follows_plan(int player_id)
{
    int inland = !mp_mapgen_slot_is_coastal(player_id);
    int ok = mp_mapgen_slot_allows(player_id, RESOURCE_WHEAT) && mp_mapgen_slot_allows(player_id, RESOURCE_VEGETABLES);
    ok &= mp_mapgen_slot_allows(player_id, RESOURCE_IRON) == inland;
    ok &= mp_mapgen_slot_allows(player_id, RESOURCE_MARBLE) == inland;
    ok &= mp_mapgen_slot_allows(player_id, RESOURCE_MEAT) == inland; // pigs inland; on the coast, only wharves
    ok &= mp_mapgen_slot_allows(player_id, RESOURCE_TIMBER) == !inland;
    ok &= mp_mapgen_slot_allows(player_id, RESOURCE_CLAY) == !inland;
    ok &= mp_mapgen_slot_allows(player_id, RESOURCE_FRUIT) == !inland;
    return ok;
}

// the players draw their arrival points by lot (Alexandre, 2026-10-06, D-052): player 1 does not always live inland.
// On the map for 4, two players live inland and two on the coast; three players leave a place on the coast (D-062)
static int command_placement(const char *file)
{
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-62s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    for (int num_players = 2; num_players <= 4; num_players++) {
        int inland_seen[4] = { 0 }, distinct = 1, inland_count_ok = 1, inland_timber = 0, plan_ok = 1;
        int permissions_ok = 1, olives_vines_ok = 1;
        for (unsigned int seed = 1; seed <= 12; seed++) {
            player_context_switch(0);
            player_context_set_num_players(1);
            if (!create_prepared(file, num_players, seed)) {
                printf("Unable to create the prepared map\n");
                return 2;
            }
            int inland = 0, coastal = 0, olives = 0, vines = 0;
            for (int p = 0; p < num_players; p++) {
                int x, y;
                mp_mapgen_city_center(p, &x, &y);
                for (int q = 0; q < p; q++) {
                    int qx, qy;
                    mp_mapgen_city_center(q, &qx, &qy);
                    distinct &= qx != x || qy != y;
                }
                plan_ok &= slot_follows_plan(p);
                permissions_ok &= permissions_match_slot(p);
                olives += mp_mapgen_slot_allows(p, RESOURCE_OLIVES);
                vines += mp_mapgen_slot_allows(p, RESOURCE_VINES);
                if (!mp_mapgen_slot_is_coastal(p)) {
                    inland_seen[p] = 1;
                    inland++;
                    // no more timber inland (Alexandre, D-062: « ne mets plus le bois au joueur des terres »)
                    inland_timber |= mp_mapgen_slot_allows(p, RESOURCE_TIMBER);
                    // olives for one and vines for the other on the map for 4; olives on the map for 2
                    olives_vines_ok &= mp_mapgen_slot_allows(p, RESOURCE_OLIVES) !=
                        mp_mapgen_slot_allows(p, RESOURCE_VINES);
                    olives_vines_ok &= num_players > 2 || mp_mapgen_slot_allows(p, RESOURCE_OLIVES);
                } else {
                    coastal++;
                    // on the coast, vines on the map for 2 only
                    olives_vines_ok &= !mp_mapgen_slot_allows(p, RESOURCE_OLIVES) &&
                        mp_mapgen_slot_allows(p, RESOURCE_VINES) == (num_players <= 2);
                }
            }
            // two inland on the map for 4, also with three players: the place left free is on the coast
            inland_count_ok &= inland == (num_players <= 2 ? 1 : 2) && coastal >= 1;
            olives_vines_ok &= olives == 1 && vines == 1;
        }
        int inland_players = inland_seen[0] + inland_seen[1] + inland_seen[2] + inland_seen[3];
        printf("%d players: the arrival points inland went to %d different players in 12 games\n", num_players,
            inland_players);
        CHECK(distinct, "every player has his own arrival point");
        CHECK(inland_players >= (num_players <= 2 ? 2 : 3), "player 1 does not always live inland");
        CHECK(inland_count_ok, num_players == 2 ? "one player inland, one on the coast" :
            num_players == 3 ? "two players inland, the free place is on the coast" :
            "two players inland, two on the coast");
        CHECK(!inland_timber, "the players inland may not exploit timber");
        CHECK(plan_ok, "materials and food of each arrival point follow the plan");
        CHECK(olives_vines_ok, "olives and vines as in the plan");
        CHECK(permissions_ok, "the permissions of each city are those of its arrival point");
    }
    player_context_switch(0);
    player_context_set_num_players(1);
    CHECK(create_prepared(file, 2, 0) && !mp_mapgen_slot_is_coastal(0), "seed 0 keeps the plan (tests)");
    player_context_switch(0);
    player_context_set_num_players(1);
    CHECK(create_prepared(file, 4, 0) && !mp_mapgen_slot_is_coastal(0) && mp_mapgen_slot_is_coastal(1) &&
        !mp_mapgen_slot_is_coastal(2) && mp_mapgen_slot_is_coastal(3), "seed 0 keeps the plan for 4 (tests)");
    player_context_switch(0);
    player_context_set_num_players(1);
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: the arrival points are not drawn by lot as planned" :
        "Identical: the arrival points are drawn by lot as planned");
    return failures ? 1 : 0;
}

// we go everywhere on the prepared maps (Alexandre, 2026-10-06, D-052): woods stay impassable, as in the original
// game, but they are fewer, and no clearing is shut in by them
static int command_reachable(const char *file, int num_players)
{
    if (!create_prepared(file, num_players, 0)) {
        printf("Unable to create the prepared map\n");
        return 2;
    }
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-62s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    run_trace(50, 50, 0, 0);
    figure *m = first_missionary(0);
    CHECK(m != 0, "player 1 has a missionary");
    if (m) {
        // distances from the missionary to every tile within reach: the route asks for the sea, out of reach
        int sea_x, sea_y;
        mp_mapgen_sea_end(0, &sea_x, &sea_y);
        map_routing_citizen_can_travel_over_land(m->x, m->y, sea_x, sea_y);
        int tiles = 0, trees = 0, land = 0, reached = 0;
        for (int y = 0; y < map_grid_height(); y++) {
            for (int x = 0; x < map_grid_width(); x++) {
                int o = map_grid_offset(x, y);
                tiles++;
                trees += map_terrain_is(o, TERRAIN_TREE);
                // clear land, meadows and roads: where one walks
                if (map_terrain_is(o, TERRAIN_NOT_CLEAR) && !map_terrain_is(o, TERRAIN_ROAD)) {
                    continue;
                }
                land++;
                reached += map_routing_distance(o) > 0 || o == m->grid_offset;
            }
        }
        printf("woods: %d.%d %% of the map; within reach: %d of %d tiles of land (%d.%d %%)\n",
            trees * 100 / tiles, trees * 1000 / tiles % 10, reached, land, reached * 100 / land,
            reached * 1000 / land % 10);
        CHECK(trees * 100 >= tiles * 12 && trees * 100 <= tiles * 18, "woods cover 12 to 18 % of the map");
        CHECK(reached * 1000 >= land * 995, "at least 99.5 % of the land is within reach");
    }
    player_context_switch(0);
    player_context_set_num_players(1);
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: parts of the map are out of reach" : "Identical: we go everywhere");
    return failures ? 1 : 0;
}

// fog of war (MB.1, D-038): what player 1 discovers and sees
static int command_fog(const char *file)
{
    if (!create_prepared(file, 2, 0)) {
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

// what an .mpsav holds: players, rules, climate, trade of the empire, missionaries, zones (to read the saves of the
// players)
static int command_inspect(const char *file)
{
    if (!mp_savegame_read(file)) {
        printf("Unable to read %s\n", file);
        return 2;
    }
    int num_players = player_context_num_players();
    printf("%d players, map %d x %d, climate %d, territories %d, fog %d, prepared map %d\n", num_players,
        map_data.width, map_data.height, scenario_property_climate(), game_rules_territories(), game_rules_fog_of_war(),
        game_rules_multiplayer_settings()->prepared_map + 1);
    int zone[PLAYER_CONTEXT_MAX_PLAYERS + 1] = { 0 };
    int water = 0;
    for (int y = 0; y < map_data.height; y++) {
        for (int x = 0; x < map_data.width; x++) {
            int o = map_grid_offset(x, y);
            int owner = mp_territory_owner(o);
            zone[owner >= 0 && owner < PLAYER_CONTEXT_MAX_PLAYERS ? owner : PLAYER_CONTEXT_MAX_PLAYERS]++;
            water += map_terrain_is(o, TERRAIN_WATER) != 0;
        }
    }
    printf("water tiles: %d\n", water);
    for (int p = 0; p < num_players; p++) {
        player_context_switch(p);
        int land, sea, open_land = 0, open_sea = 0;
        count_trade_cities(&land, &sea);
        for (int i = 0; i < 41; i++) {
            empire_city *c = empire_city_get(i);
            if (c && c->in_use && c->type == EMPIRE_CITY_TRADE && c->is_open) {
                if (c->is_sea_trade) {
                    open_sea++;
                } else {
                    open_land++;
                }
            }
        }
        printf("player %d: population %d, treasury %d, zone %d tiles, river entry (%d, %d), "
            "trade cities %d by land (%d open), %d by sea (%d open)\n", p + 1, city_population(),
            city_finance_treasury(), zone[p], scenario_map_river_entry().x, scenario_map_river_entry().y, land,
            open_land, sea, open_sea);
        // what this city may produce: its empire allows the food, the map its raw materials
        static const char *NAMES[] = { "", "wheat", "vegetables", "fruit", "olives", "vines", "meat", "wine", "oil",
            "iron", "timber", "clay", "marble", "weapons", "furniture", "pottery" };
        printf("  may produce:");
        for (int r = RESOURCE_MIN; r <= RESOURCE_POTTERY; r++) {
            if (empire_city_our_production_allowed(r)) {
                printf(" %s", NAMES[r]);
            }
        }
        printf("\n");
        for (int i = p * MAX_FIGURES + 1; i < (p + 1) * MAX_FIGURES; i++) {
            figure *f = figure_get(i);
            if (f->state == FIGURE_STATE_ALIVE && mp_missionary_is_scout(f)) {
                printf("  missionary %d at (%d, %d), action %d, destination (%d, %d)\n", f->id, f->x, f->y,
                    f->action_state, f->destination_x, f->destination_y);
            }
        }
    }
    player_context_switch(0);
    return 0;
}

// the terrain of a part of a prepared map, one letter per tile: W water, R road, B bridge, A aqueduct, b building,
// T tree, r rock, m meadow, . land (to look at what the generator made)
static int command_terrain(const char *file, int num_players, int x0, int y0, int width, int height, int raw)
{
    size_t length = strlen(file);
    int is_save = length > 6 && strcmp(file + length - 6, ".mpsav") == 0;
    if (is_save ? !mp_savegame_read(file) : !create_prepared(file, num_players, 0)) {
        printf("Unable to create the prepared map\n");
        return 2;
    }
    for (int y = y0; y < y0 + height; y++) {
        printf("%4d ", y);
        for (int x = x0; x < x0 + width; x++) {
            int o = map_grid_offset(x, y);
            char c = '.';
            if (map_is_bridge(o)) {
                c = 'B';
            } else if (map_terrain_is(o, TERRAIN_AQUEDUCT)) {
                c = 'A';
            } else if (map_terrain_is(o, TERRAIN_ROAD)) {
                c = 'R';
            } else if (map_terrain_is(o, TERRAIN_BUILDING)) {
                c = 'b';
            } else if (map_terrain_is(o, TERRAIN_WATER)) {
                c = 'W';
            } else if (map_terrain_is(o, TERRAIN_TREE)) {
                c = 'T';
            } else if (map_terrain_is(o, TERRAIN_ROCK)) {
                c = 'r';
            } else if (map_terrain_is(o, TERRAIN_MEADOW)) {
                c = 'm';
            }
            putchar(c);
        }
        putchar('\n');
    }
    if (raw) {
        // terrain bits, image and elevation of each tile
        for (int y = y0; y < y0 + height; y++) {
            for (int x = x0; x < x0 + width; x++) {
                int o = map_grid_offset(x, y);
                printf("(%d,%d) terrain %05x image %d elevation %d\n", x, y, map_terrain_get(o), map_image_at(o),
                    map_elevation_at(o));
            }
        }
    }
    player_context_switch(0);
    player_context_set_num_players(1);
    return 0;
}

// a template whose empire does not trade by land and by sea is refused for the prepared maps
static void city_action(int player_id, int action, int a1, int a2, int a3);
static int stock_of(int player_id, int resource);
static int treasury_of(int player_id);

// long trips on the large map (T4.17, D-064): between the cities the paths of the walkers stay within what a figure
// can store (500 steps), and under 400 steps on every prepared map (T4.14, D-064); a caravan between the two farthest
// players arrives, and a ship can sail the whole sea of a map as large as the grid
static int command_longroutes(const char *file, int num_players)
{
    if (num_players < 2 || num_players > 4 || !create_prepared(file, num_players, 0)) {
        printf("Unable to create the prepared map\n");
        return 2;
    }
    game_rules_settings rules;
    game_rules_default_multiplayer_settings(&rules);
    rules.ai_invasions = 0;
    game_rules_set_multiplayer(&rules);
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-70s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    int cx[4], cy[4];
    for (int p = 0; p < num_players; p++) {
        mp_mapgen_city_center(p, &cx[p], &cy[p]);
    }
    printf("prepared map %d for %d players\n", options.prepared_map + 1, num_players);
    static uint8_t path[2000];
    int far_a = 0, far_b = 1, far_length = 0, longest_land = 0, all_paths = 1;
    for (int a = 0; a < num_players; a++) {
        for (int b = a + 1; b < num_players; b++) {
            int road = map_routing_citizen_can_travel_over_road_garden(cx[a], cy[a], cx[b], cy[b]) ?
                map_routing_get_path(path, cx[a], cy[a], cx[b], cy[b], 8) : 0;
            int land = map_routing_citizen_can_travel_over_land(cx[a], cy[a], cx[b], cy[b]) ?
                map_routing_get_path(path, cx[a], cy[a], cx[b], cy[b], 8) : 0;
            printf("players %d and %d: path of %d steps by the roads, %d over the land\n", a + 1, b + 1, road, land);
            all_paths &= road > 0 && land > 0;
            if (road > far_length) {
                far_length = road;
                far_a = a;
                far_b = b;
            }
            longest_land = land > longest_land ? land : longest_land;
        }
    }
    CHECK(all_paths, "a path joins every pair of cities, by the roads and over the land");
    CHECK(far_length < 500 && longest_land < 500, "the longest path of the map is within the 500 steps of a figure");
    // a margin under the limit of a figure, which a new map must keep (D-064)
    CHECK(far_length < 400 && longest_land < 400, "the longest path of the map is under 400 steps");
    printf("farthest by the roads: players %d and %d, %d steps (longest over the land: %d)\n", far_a + 1, far_b + 1,
        far_length, longest_land);

    // a caravan between the two of them: the buyer is the first, the seller the second
    int buyer = far_a, seller = far_b;
    int wx[2] = { cx[buyer] - 6, cx[seller] - 6 }, wy[2] = { cy[buyer] + 1, cy[seller] + 1 };
    build_as(buyer, BUILDING_WAREHOUSE, wx[0], wy[0], wx[0], wy[0]);
    build_as(seller, BUILDING_WAREHOUSE, wx[1], wy[1], wx[1], wy[1]);
    player_context_switch(seller);
    building *store = building_get(map_building_at(map_grid_offset(wx[1], wy[1])));
    for (int i = 0; i < 8; i++) {
        building_warehouse_add_resource(store, RESOURCE_MARBLE);
    }
    player_context_switch(0);
    city_action(seller, MP_ACTION_SET_SELL_PRICE, buyer, RESOURCE_MARBLE, 150);
    city_action(buyer, MP_ACTION_SET_BUYS_FROM, seller, RESOURCE_MARBLE, 1);
    city_action(buyer, MP_ACTION_PROPOSE_ROUTE, seller, 1, 0);
    city_action(seller, MP_ACTION_PROPOSE_ROUTE, buyer, 1, 0);
    run_trace(50, 50, 0, 0); // the warehouses come into use
    int days = 0;
    for (; days < 400 && stock_of(buyer, RESOURCE_MARBLE) < 8; days++) {
        run_trace(50, 50, 0, 0);
    }
    printf("the caravan of player %d reaches player %d after %d days: %d of 8 loads, %d left with the seller\n",
        seller + 1, buyer + 1, days, stock_of(buyer, RESOURCE_MARBLE), stock_of(seller, RESOURCE_MARBLE));
    CHECK(stock_of(buyer, RESOURCE_MARBLE) == 8, "the 8 loads of marble arrive between the two farthest players");

    // the arm of the sea from one edge of the map to the other: a ship sails along all of it
    int sea_wx, sea_wy, sea_ex, sea_ey;
    mp_mapgen_sea_end(0, &sea_wx, &sea_wy);
    mp_mapgen_sea_end(1, &sea_ex, &sea_ey);
    map_routing_calculate_distances_water_boat(sea_wx, sea_wy);
    int ship_path = map_routing_get_path_on_water(path, sea_ex, sea_ey, 0);
    printf("the sea from (%d, %d) to (%d, %d): a ship's path of %d steps\n", sea_wx, sea_wy, sea_ex, sea_ey, ship_path);
    CHECK(ship_path > 0 && ship_path < 500, "a ship sails the whole arm of the sea, from one edge of the map to the other");

    // the sea is as large as the map: a ship sails to its last tile, the search of the route is not cut short
    int width = map_grid_width(), height = map_grid_height();
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            map_terrain_set(map_grid_offset(x, y), TERRAIN_WATER);
        }
    }
    map_routing_update_water();
    int far_x = width - 2, far_y = height - 2;
    map_routing_calculate_distances_water_boat(1, 1);
    printf("sea of %d tiles: distance to the opposite corner %d\n", width * height,
        map_routing_distance(map_grid_offset(far_x, far_y)));
    CHECK(map_routing_distance(map_grid_offset(far_x, far_y)) > 0, "a ship finds the far end of a sea of the size of the map");
    map_routing_calculate_distances_water_flotsam(1, 1);
    CHECK(map_routing_distance(map_grid_offset(far_x, far_y)) > 0, "so does the flotsam");
    player_context_switch(0);
    player_context_set_num_players(1);
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: the long routes do not work on the large map" :
        "Identical: the long routes work on the large map");
    return failures ? 1 : 0;
}

// the nearest tile of the edge of the map to (x, y), along that edge, where an army can land and walk to (tx, ty)
static int clear_edge_tile(int *x, int *y, int tx, int ty)
{
    int width = map_grid_width(), height = map_grid_height();
    int along_x = *y <= 1 || *y >= height - 2;
    for (int d = 0; d < 60; d++) {
        for (int sign = -1; sign <= 1; sign += 2) {
            int xx = along_x ? *x + sign * d : *x;
            int yy = along_x ? *y : *y + sign * d;
            if (xx < 1 || yy < 1 || xx > width - 2 || yy > height - 2) {
                continue;
            }
            int o = map_grid_offset(xx, yy);
            if (map_terrain_is(o, TERRAIN_ELEVATION | TERRAIN_ROCK | TERRAIN_TREE | TERRAIN_WATER |
                    TERRAIN_BUILDING | TERRAIN_AQUEDUCT | TERRAIN_WALL | TERRAIN_GATEHOUSE)) {
                continue;
            }
            // without any limit of tiles (through the building -1: none)
            if (map_routing_noncitizen_can_travel_over_land(xx, yy, tx, ty, -1, 0)) {
                *x = xx;
                *y = yy;
                return 1;
            }
        }
    }
    return 0;
}

// the counters of the searches of the routing, as saved: searches of the enemies over the land, all searches
static void routing_counters(int *enemy, int *total)
{
    uint8_t data[16];
    buffer buf;
    buffer_init(&buf, data, sizeof(data));
    map_routing_save_state(&buf);
    buffer_set(&buf, 4);
    *enemy = buffer_read_i32(&buf);
    *total = buffer_read_i32(&buf);
}

static int nearest_enemy_distance(int player_id, int x, int y)
{
    int nearest = 100000;
    for (int i = player_id * MAX_FIGURES + 1; i < (player_id + 1) * MAX_FIGURES; i++) {
        figure *f = figure_get(i);
        if (f->state == FIGURE_STATE_ALIVE && figure_is_enemy(f)) {
            int d = abs(f->x - x) > abs(f->y - y) ? abs(f->x - x) : abs(f->y - y);
            nearest = d < nearest ? d : nearest;
        }
    }
    return nearest;
}

// K-review-fixes (T4.17, D-064): an enemy army landing at the edge of the map for 4 farthest from a city reaches it.
// The searches of the enemies over the land stop after a number of tiles chosen for the maps of 162 tiles; on the large
// map they follow the size of the grid (classic: the original numbers).
static int command_farinvasion(const char *file)
{
    if (!mp_mapgen_create_prepared(file, 4, 0)) {
        printf("Unable to create the prepared map\n");
        return 2;
    }
    game_rules_settings rules;
    game_rules_default_multiplayer_settings(&rules);
    rules.territories = 1;
    rules.ai_invasions = 1;
    game_rules_set_multiplayer(&rules);
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-74s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    int size = map_grid_width();
    const int points[4][2] = { { size / 2, 1 }, { size / 2, size - 2 }, { 1, size / 3 }, { size - 2, 2 * size / 3 } };
    int p = 0, point = 0, far = -1, cx = 0, cy = 0;
    for (int q = 0; q < 4; q++) {
        int qx, qy;
        mp_mapgen_city_center(q, &qx, &qy);
        for (int i = 0; i < 4; i++) {
            int d = abs(points[i][0] - qx) + abs(points[i][1] - qy);
            if (d > far) {
                far = d;
                p = q;
                point = i;
                cx = qx;
                cy = qy;
            }
        }
    }
    int ex = points[point][0], ey = points[point][1];
    if (!clear_edge_tile(&ex, &ey, cx, cy)) {
        printf("No clear tile on the edge near (%d, %d)\n", points[point][0], points[point][1]);
        return 2;
    }
    printf("player %d, city at (%d, %d); the army lands at (%d, %d), %d tiles away\n", p + 1, cx, cy, ex, ey,
        abs(ex - cx) + abs(ey - cy));
    // the searches of route.c for an enemy going to a tile, with the limits of the original and the scaled ones
    int original = map_routing_noncitizen_can_travel_over_land(ex, ey, cx, cy, 0, 25000);
    int scaled_limit = map_routing_noncitizen_max_tiles(25000);
    int scaled = map_routing_noncitizen_can_travel_over_land(ex, ey, cx, cy, 0, scaled_limit);
    printf("search over the land within 25000 tiles: %s; within %d tiles: %s\n", original ? "found" : "not found",
        scaled_limit, scaled ? "found" : "not found");
    CHECK(scaled, "an enemy finds his way over the land from the farthest edge");
    CHECK(map_routing_noncitizen_max_tiles(5000) >= 5000 && map_routing_noncitizen_max_tiles(400) >= 400,
        "the scaled limits are never below those of the original");

    // route.c itself: an enemy going to a tile of the city from that edge finds his way over the land, without falling
    // back on the search "through everything", which walks through forts (the counters of the routing tell)
    player_context_switch(p);
    figure *walker = figure_create(FIGURE_ENEMY43_SPEAR, ex, ey, DIR_0_TOP);
    walker->terrain_usage = TERRAIN_USAGE_ENEMY;
    walker->destination_x = cx;
    walker->destination_y = cy;
    walker->destination_building_id = 0;
    int before_enemy, before_total, after_enemy, after_total;
    routing_counters(&before_enemy, &before_total);
    figure_route_add(walker);
    routing_counters(&after_enemy, &after_total);
    int path_length = walker->routing_path_length;
    figure_route_remove(walker);
    figure_delete(walker);
    player_context_switch(0);
    printf("route of an enemy to the city: %d steps, %d search(es) over the land, %d through everything\n",
        path_length, after_enemy - before_enemy, (after_total - before_total) - (after_enemy - before_enemy));
    CHECK(path_length > 0 && after_total - before_total == after_enemy - before_enemy,
        "route.c finds the way of an enemy over the land, not through everything");

    // a real army: the city has houses, the only invasion point is the farthest one
    build_as(p, BUILDING_HOUSE_VACANT_LOT, cx - 4, cy + 2, cx + 4, cy + 2);
    build_as(p, BUILDING_PREFECTURE, cx - 4, cy + 4, cx - 4, cy + 4);
    player_context_switch(p);
    scenario_editor_clear_invasion_points();
    scenario_editor_set_invasion_point(0, ex, ey);
    scenario_invasion_start_from_cheat();
    int start_distance = nearest_enemy_distance(p, cx, cy);
    player_context_switch(0);
    printf("army landed: nearest enemy %d tiles from the city\n", start_distance);
    CHECK(start_distance < 100000, "the army lands");
    setting_reset_speeds(500, setting_scroll_speed());
    int ticks = 0, nearest = start_distance;
    for (; ticks < 30000 && nearest > 12; ticks += 100) {
        run_trace(100, 100, 0, 0);
        player_context_switch(p);
        nearest = nearest_enemy_distance(p, cx, cy);
        player_context_switch(0);
    }
    printf("after %d ticks: nearest enemy %d tiles from the city\n", ticks, nearest);
    CHECK(nearest <= 12, "the army reaches the city");
    player_context_switch(0);
    player_context_set_num_players(1);
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: an army from the far edge does not reach the city" :
        "Identical: an army from the far edge reaches the city");
    return failures ? 1 : 0;
}

static void ignore_command(mp_command *command)
{
}

static const struct { int building; int resource; const char *name; } MENU_RAW[] = {
    { BUILDING_CLAY_PIT, RESOURCE_CLAY, "clay" }, { BUILDING_TIMBER_YARD, RESOURCE_TIMBER, "timber" },
    { BUILDING_IRON_MINE, RESOURCE_IRON, "iron" }, { BUILDING_MARBLE_QUARRY, RESOURCE_MARBLE, "marble" },
};

// the raw materials of the build menu of the local player, as a bit mask
static int menu_raw_mask(void)
{
    int mask = 0;
    for (int r = 0; r < 4; r++) {
        mask |= building_menu_is_enabled(MENU_RAW[r].building) << r;
    }
    return mask;
}

static void print_menu_raw(const char *when, int mask)
{
    printf("  %-44s:", when);
    for (int r = 0; r < 4; r++) {
        if (mask & (1 << r)) {
            printf(" %s", MENU_RAW[r].name);
        }
    }
    printf("\n");
}

// the build menu is the local player's (T4.13, D-061): a route that another player opens, a command run in his
// city on every computer, leaves it as it is; the player's own route refreshes it
static int command_menuowner(const char *file)
{
    if (!create_prepared(file, 2, 0)) {
        printf("Unable to create the prepared map\n");
        return 2;
    }
    // the routes of the empire are open on the prepared map: close them, so that the players open them
    for (int p = 0; p < 2; p++) {
        player_context_switch(p);
        for (int i = 1; i < 41; i++) {
            empire_city *c = empire_city_get(i);
            if (c->in_use && c->type == EMPIRE_CITY_TRADE) {
                c->is_open = 0;
            }
        }
    }
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("  %-60s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    for (int local = 0; local < 2; local++) {
        int other = 1 - local;
        printf("player %d is the local player (%s), player %d the other (%s)\n", local + 1,
            mp_mapgen_slot_is_coastal(local) ? "coast" : "inland", other + 1,
            mp_mapgen_slot_is_coastal(other) ? "coast" : "inland");
        // what the menu of the other player would be: he is the local player of his own computer
        mp_session_init_network(other, ignore_command);
        player_context_switch(other);
        building_menu_update();
        int other_would_have = menu_raw_mask();
        mp_session_init_network(local, ignore_command);
        player_context_switch(local);
        building_menu_update();
        int own = menu_raw_mask();
        print_menu_raw("menu of the local player", own);
        print_menu_raw("menu the other one would have", other_would_have);
        int expected = 0;
        for (int r = 0; r < 4; r++) {
            expected |= mp_mapgen_slot_allows(local, MENU_RAW[r].resource) << r;
        }
        CHECK(own == expected, "the menu shows the materials of his own arrival point");
        CHECK(own != other_would_have, "the two arrival points do not have the same materials");
        building_menu_has_changed(); // the interface has seen it
        // the other player opens a route with the empire: his command runs in his city
        player_context_switch(other);
        int city = find_closed_trade_city();
        player_context_switch(local);
        CHECK(city > 0, "the empire has a route to open");
        city_action(other, MP_ACTION_OPEN_TRADE_ROUTE, city, 0, 0);
        player_context_switch(other);
        CHECK(empire_city_get(city)->is_open, "the route of the other player is open");
        player_context_switch(local);
        print_menu_raw("menu after the route of the other player", menu_raw_mask());
        CHECK(menu_raw_mask() == own, "his menu keeps the same materials");
        CHECK(!building_menu_has_changed(), "his menu was not rebuilt");
        CHECK(!empire_city_get(city)->is_open, "the route of the other player is not his");
        // his own route
        int mine = find_closed_trade_city();
        CHECK(mine > 0, "he has a route to open too");
        city_action(local, MP_ACTION_OPEN_TRADE_ROUTE, mine, 0, 0);
        CHECK(empire_city_get(mine)->is_open, "his route is open");
        CHECK(building_menu_has_changed(), "his menu was rebuilt by his own route");
        CHECK(menu_raw_mask() == own, "and keeps the materials of his arrival point");
    }
    mp_session_init_offline();
    player_context_switch(0);
    player_context_set_num_players(1);
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: the build menu follows what the other players do" :
        "Identical: the build menu is the one of the local player");
    return failures ? 1 : 0;
}

static int command_notrade(const char *file)
{
    int refused = !create_prepared(file, 2, 0) && mp_mapgen_lacks_trade_routes();
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
    printf("%s: %d trade cities by land, %d by sea, climate %d\n", file, land, sea, scenario_property_climate());
    for (int i = 0; i < 41; i++) {
        empire_city *c = empire_city_get(i);
        if (c && c->in_use && (c->type == EMPIRE_CITY_TRADE || c->type == EMPIRE_CITY_FUTURE_TRADE)) {
            printf("  %s, cost %d, sells", c->is_sea_trade ? "sea " : "land", c->cost_to_open);
            for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
                if (c->sells_resource[r]) {
                    printf(" %d", r);
                }
            }
            printf("; buys");
            for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
                if (c->buys_resource[r]) {
                    printf(" %d", r);
                }
            }
            printf("\n");
        }
    }
    return 0;
}

// the empire in multiplayer (T4.12, D-060): one price of Rome per resource, the original buying price; buying from
// the empire costs it plus the portorium of 50%, selling to it earns it minus the portorium; the price changes of the
// game still apply; a classic game keeps the buying and selling prices of the original
static int command_importprice(const char *file)
{
    if (!create_prepared(file, 2, 0)) {
        printf("Unable to create the prepared map\n");
        return 2;
    }
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-62s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    game_rules_settings rules;
    game_rules_default_multiplayer_settings(&rules);
    rules.territories = 1;
    rules.ai_invasions = 0;
    game_rules_set_multiplayer(&rules);
    figure *m = first_missionary(0);
    int mx = m->x, my = m->y;
    build_as(0, BUILDING_MISSION_POST, mx + 4, my, mx + 4, my);
    run_trace(50, 50, 0, 0);
    build_as(0, BUILDING_WAREHOUSE, mx - 6, my - 6, mx - 6, my - 6);
    building *space = building_next(building_get(map_building_at(map_grid_offset(mx - 6, my - 6))));
    int rome = trade_price_rome(RESOURCE_MARBLE);
    printf("marble: price of Rome %d, portorium %d, bought %d, sold %d\n", rome, trade_price_duty(RESOURCE_MARBLE),
        trade_price_buy(RESOURCE_MARBLE), trade_price_sell(RESOURCE_MARBLE));
    CHECK(rome == 200, "marble: the price of Rome is the original buying price, 200");
    CHECK(trade_price_duty(RESOURCE_MARBLE) == 100, "the portorium is 50% of it: 100");
    int treasury = city_finance_treasury();
    int imports = city_data.finance.this_year.expenses.imports;
    building_warehouse_space_add_import(space, RESOURCE_MARBLE);
    CHECK(treasury - city_finance_treasury() == 300 && city_data.finance.this_year.expenses.imports - imports == 300,
        "a load bought from the empire costs 300 (1.5 times)");
    treasury = city_finance_treasury();
    int exports = city_data.finance.this_year.income.exports;
    building_warehouse_space_remove_export(space, RESOURCE_MARBLE);
    CHECK(city_finance_treasury() - treasury == 100 && city_data.finance.this_year.income.exports - exports == 100,
        "a load sold to the empire earns 100 (0.5 times)");
    int all = 1;
    for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        int price = trade_price_rome(r);
        int duty = price / 2;
        all &= trade_price_duty(r) == duty && trade_price_buy(r) == price + duty && trade_price_sell(r) == price - duty;
    }
    CHECK(all, "every resource: the price of Rome plus or minus the portorium");
    trade_price_change(RESOURCE_MARBLE, 20);
    CHECK(trade_price_rome(RESOURCE_MARBLE) == 220 && trade_price_buy(RESOURCE_MARBLE) == 330 &&
        trade_price_sell(RESOURCE_MARBLE) == 110, "a price change of the game moves the price of Rome");
    trade_price_change(RESOURCE_MARBLE, -20);

    game_rules_set_classic();
    printf("classic game: buying price %d, selling price %d\n", trade_price_buy(RESOURCE_MARBLE),
        trade_price_sell(RESOURCE_MARBLE));
    CHECK(trade_price_buy(RESOURCE_MARBLE) == 200 && trade_price_sell(RESOURCE_MARBLE) == 140 &&
        trade_price_duty(RESOURCE_MARBLE) == 0, "classic game: the original prices, 200 and 140, no portorium");
    player_context_switch(0);
    player_context_set_num_players(1);
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: the prices of the empire are not those of Rome and the portorium" :
        "Identical: the empire trades at the price of Rome, with the portorium, in multiplayer only");
    return failures ? 1 : 0;
}

static void city_action(int player_id, int action, int a1, int a2, int a3)
{
    mp_command command = { .type = MP_COMMAND_CITY_ACTION, .player_id = player_id, .args = { action, a1, a2, a3 } };
    mp_command_execute(&command);
}

static int fire_message_place(void)
{
    for (int i = 0; i < city_message_count(); i++) {
        const city_message *msg = city_message_get(i);
        if (msg->message_type == MESSAGE_FIRE && msg->param1 == BUILDING_PREFECTURE) {
            return msg->param2;
        }
    }
    return -1;
}

// a disaster far on a large map keeps its place: the button of the message goes to it (Alexandre, 2026-10-06)
static int command_messagelocation(const char *file)
{
    if (!create_prepared(file, 4, 0)) {
        printf("Unable to create the prepared map\n");
        return 2;
    }
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-62s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    int far = map_grid_offset(map_grid_width() - 20, map_grid_height() - 20);
    CHECK(far > 32767, "a place of the map for 4 beyond 16 bits");
    city_message_post_with_popup_delay(MESSAGE_CAT_FIRE, MESSAGE_FIRE, BUILDING_PREFECTURE, far);
    // the sidebar and the message window go to the place kept in the message (the test tool has no game texts to
    // tell which messages are disasters)
    CHECK(fire_message_place() == far, "the message of a fire there keeps its place");
    if (!mp_savegame_write("messagelocation.mpsav") || !mp_savegame_read("messagelocation.mpsav")) {
        printf("Unable to write and read the game\n");
        return 2;
    }
    remove("messagelocation.mpsav");
    CHECK(fire_message_place() == far, "still after the game is saved and loaded");

    player_context_switch(0);
    player_context_set_num_players(1);
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: messages lose the place of a disaster" :
        "Identical: messages keep the place of a disaster");
    return failures ? 1 : 0;
}

// prices between players (M8.3, D-043): a price per resource and buyer, a notice to the buyer when it changes
static int command_tradeprices(const char *file)
{
    if (!create_prepared(file, 3, 0)) {
        printf("Unable to create the prepared map\n");
        return 2;
    }
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-62s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    game_rules_settings rules;
    game_rules_default_multiplayer_settings(&rules);
    rules.ai_invasions = 0;
    game_rules_set_multiplayer(&rules);
    // by default, the price of Rome, without the portorium: trade between players stays in the province (D-060)
    int base = trade_price_rome(RESOURCE_MARBLE);
    CHECK(base == 200 && mp_trade_price(1, 0, RESOURCE_MARBLE) == base && mp_trade_price(0, 2, RESOURCE_MARBLE) == base,
        "by default, the price of Rome: 200");
    // no portorium between players: checked at the end by a delivery, paid at the price of the seller
    city_action(1, MP_ACTION_SET_SELL_PRICE, 0, RESOURCE_MARBLE, 150);
    city_action(1, MP_ACTION_SET_SELL_PRICE, 2, RESOURCE_MARBLE, 250);
    CHECK(mp_trade_price(1, 0, RESOURCE_MARBLE) == 150 && mp_trade_price(1, 2, RESOURCE_MARBLE) == 250,
        "player 2 sells marble 150 to player 1 and 250 to player 3");
    int told = mp_trade_notifications();
    city_action(1, MP_ACTION_SET_SELL_PRICE, 0, RESOURCE_MARBLE, 160);
    CHECK(mp_trade_notifications() == told, "player 1 does not buy it: no notice");
    city_action(0, MP_ACTION_SET_BUYS_FROM, 1, RESOURCE_MARBLE, 1);
    CHECK(mp_trade_buys_from(0, 1, RESOURCE_MARBLE), "player 1 buys marble from player 2");
    city_action(1, MP_ACTION_SET_SELL_PRICE, 0, RESOURCE_MARBLE, 180);
    CHECK(mp_trade_notifications() == told + 1, "the price changes: player 1 is told");
    city_action(1, MP_ACTION_SET_SELL_PRICE, 0, RESOURCE_MARBLE, 180);
    CHECK(mp_trade_notifications() == told + 1, "the same price again: no notice");
    city_action(1, MP_ACTION_SET_SELL_PRICE, 0, RESOURCE_TIMBER, 99);
    CHECK(mp_trade_notifications() == told + 1, "another resource he does not buy: no notice");
    // the full-screen alert of the buyer: one line per seller and resource, from the price he knew
    const mp_price_alert *alert = mp_trade_num_price_alerts() == 1 ? mp_trade_price_alert(0) : 0;
    CHECK(alert && alert->seller == 1 && alert->resource == RESOURCE_MARBLE && alert->old_price == 160 &&
        alert->new_price == 180, "player 1 has an alert: marble from player 2, 160 to 180");
    city_action(1, MP_ACTION_SET_SELL_PRICE, 0, RESOURCE_MARBLE, 200);
    alert = mp_trade_num_price_alerts() == 1 ? mp_trade_price_alert(0) : 0;
    CHECK(alert && alert->old_price == 160 && alert->new_price == 200, "a second change updates the same line");
    city_action(1, MP_ACTION_SET_SELL_PRICE, 0, RESOURCE_MARBLE, 160);
    CHECK(mp_trade_num_price_alerts() == 0, "back to the price he knew: no alert left");
    city_action(1, MP_ACTION_SET_SELL_PRICE, 0, RESOURCE_MARBLE, 180);
    mp_trade_clear_price_alerts();
    CHECK(mp_trade_num_price_alerts() == 0, "the player closes the alert");
    city_action(1, MP_ACTION_SET_SELL_PRICE, 1, RESOURCE_MARBLE, 10);
    CHECK(mp_trade_price(1, 1, RESOURCE_MARBLE) == base, "nobody sells to himself");

    if (!mp_savegame_write("tradeprices.mpsav") || !mp_savegame_read("tradeprices.mpsav")) {
        printf("Unable to write and read the game\n");
        return 2;
    }
    remove("tradeprices.mpsav");
    CHECK(mp_trade_price(1, 0, RESOURCE_MARBLE) == 180 && mp_trade_price(1, 2, RESOURCE_MARBLE) == 250 &&
        mp_trade_buys_from(0, 1, RESOURCE_MARBLE), "prices and purchases are saved");

    // K-review-fixes (T4.12): a delivery of 4 loads from player 2 to player 1 is paid 180 a load, the price of the
    // seller, by one and to the other: no portorium between players (the empire would take 50 %)
    int x0, y0, x1, y1;
    mp_mapgen_city_center(0, &x0, &y0);
    mp_mapgen_city_center(1, &x1, &y1);
    build_as(0, BUILDING_WAREHOUSE, x0 - 6, y0 + 1, x0 - 6, y0 + 1);
    build_as(1, BUILDING_WAREHOUSE, x1 - 6, y1 + 1, x1 - 6, y1 + 1);
    player_context_switch(1);
    building *store = building_get(map_building_at(map_grid_offset(x1 - 6, y1 + 1)));
    for (int i = 0; i < 4; i++) {
        building_warehouse_add_resource(store, RESOURCE_MARBLE);
    }
    player_context_switch(0);
    city_action(0, MP_ACTION_PROPOSE_ROUTE, 1, 1, 0);
    city_action(1, MP_ACTION_PROPOSE_ROUTE, 0, 1, 0);
    run_trace(50, 50, 0, 0); // the warehouses come into use
    int buyer_money = treasury_of(0), seller_money = treasury_of(1);
    int days = 0;
    for (; days < 300 && stock_of(0, RESOURCE_MARBLE) < 4; days++) {
        run_trace(50, 50, 0, 0);
    }
    int paid = buyer_money - treasury_of(0), earned = treasury_of(1) - seller_money;
    printf("after %d days: %d loads delivered, player 1 paid %d, player 2 earned %d (portorium: %d a load)\n", days,
        stock_of(0, RESOURCE_MARBLE), paid, earned, trade_price_buy(RESOURCE_MARBLE) - base);
    CHECK(stock_of(0, RESOURCE_MARBLE) == 4, "player 2 delivers 4 loads of marble to player 1");
    CHECK(paid == 4 * 180 && earned == 4 * 180, "paid 180 a load by one and to the other: no portorium between players");

    player_context_switch(0);
    player_context_set_num_players(1);
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: prices between players do not work as designed" :
        "Identical: prices between players work as designed");
    return failures ? 1 : 0;
}

static int count_caravans(int player_id)
{
    int count = 0;
    for (int i = player_id * MAX_FIGURES + 1; i < (player_id + 1) * MAX_FIGURES; i++) {
        figure *f = figure_get(i);
        count += f->state == FIGURE_STATE_ALIVE && mp_trade_is_caravan(f);
    }
    return count;
}

static int stock_of(int player_id, int resource)
{
    player_context_switch(player_id);
    int count = 0;
    for (int i = BUILDING_FIRST; i < BUILDING_END; i++) {
        building *b = building_get(i);
        if (b->state == BUILDING_STATE_IN_USE && b->type == BUILDING_WAREHOUSE_SPACE &&
            b->subtype.warehouse_resource_id == resource) {
            count += b->loads_stored;
        }
    }
    player_context_switch(0);
    return count;
}

static int marble_of(int player_id)
{
    return stock_of(player_id, RESOURCE_MARBLE);
}

// loads of a resource carried by the caravans between players
static int in_transit(int resource)
{
    int loads = 0;
    for (int p = 0; p < player_context_num_players(); p++) {
        for (int i = p * MAX_FIGURES + 1; i < (p + 1) * MAX_FIGURES; i++) {
            figure *f = figure_get(i);
            if (f->state == FIGURE_STATE_ALIVE && mp_trade_is_caravan(f) && f->resource_id == resource) {
                loads += f->loads_sold_or_carrying;
            }
        }
    }
    return loads;
}

// the trade scenario of the tests: a warehouse by the main road in each city of the map for 2, loads of marble and
// iron in the one of player 2, who sells them to player 1 at 150 and 120 over an open route
static void setup_trade(int marble, int iron, int open_route)
{
    int x0, y0, x1, y1;
    mp_mapgen_city_center(0, &x0, &y0);
    mp_mapgen_city_center(1, &x1, &y1);
    build_as(0, BUILDING_WAREHOUSE, x0 - 6, y0 + 1, x0 - 6, y0 + 1);
    build_as(1, BUILDING_WAREHOUSE, x1 - 6, y1 + 1, x1 - 6, y1 + 1);
    player_context_switch(1);
    building *store = building_get(map_building_at(map_grid_offset(x1 - 6, y1 + 1)));
    for (int i = 0; i < marble; i++) {
        building_warehouse_add_resource(store, RESOURCE_MARBLE);
    }
    for (int i = 0; i < iron; i++) {
        building_warehouse_add_resource(store, RESOURCE_IRON);
    }
    player_context_switch(0);
    city_action(1, MP_ACTION_SET_SELL_PRICE, 0, RESOURCE_MARBLE, 150);
    city_action(1, MP_ACTION_SET_SELL_PRICE, 0, RESOURCE_IRON, 120);
    city_action(0, MP_ACTION_SET_BUYS_FROM, 1, RESOURCE_MARBLE, 1);
    city_action(0, MP_ACTION_SET_BUYS_FROM, 1, RESOURCE_IRON, marble && iron ? 1 : 0);
    if (open_route) {
        city_action(0, MP_ACTION_PROPOSE_ROUTE, 1, 1, 0);
        city_action(1, MP_ACTION_PROPOSE_ROUTE, 0, 1, 0);
    }
    run_trace(50, 50, 0, 0); // the warehouses come into use
}

static int start_trade_game(const char *file)
{
    if (!create_prepared(file, 2, 0)) {
        printf("Unable to create the prepared map\n");
        return 0;
    }
    game_rules_settings rules;
    game_rules_default_multiplayer_settings(&rules);
    rules.ai_invasions = 0;
    game_rules_set_multiplayer(&rules);
    return 1;
}

static int treasury_of(int player_id)
{
    player_context_switch(player_id);
    int treasury = city_finance_treasury();
    player_context_switch(0);
    return treasury;
}

// caravans between players (M8.4, M8.5, M8.9, D-019, D-043, D-048): player 2 sells marble and iron to player 1;
// a caravan for each resource, both on their way the same month
static int command_caravans(const char *file)
{
    if (!start_trade_game(file)) {
        return 2;
    }
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-62s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    setup_trade(8, 8, 0);
    run_trace(20 * 50, 20 * 50, 0, 0);
    CHECK(count_caravans(1) == 0 && marble_of(0) == 0, "no route yet: no caravan");

    city_action(0, MP_ACTION_PROPOSE_ROUTE, 1, 1, 0);
    CHECK(!mp_trade_route_is_open(0, 1), "a route proposed by one player only is not open");
    city_action(1, MP_ACTION_PROPOSE_ROUTE, 0, 1, 0);
    CHECK(mp_trade_route_is_open(0, 1), "proposed by both, it is open");
    int buyer_money = treasury_of(0), seller_money = treasury_of(1);
    int notices = mp_trade_notifications();
    int days = 0, most_caravans = 0, shown_on_the_way = 0;
    for (; days < 200 && (marble_of(0) < 8 || stock_of(0, RESOURCE_IRON) < 8); days++) {
        run_trace(50, 50, 0, 0);
        int caravans = count_caravans(1);
        if (caravans > most_caravans) {
            printf("day %d: %d caravans of player 2 on their way\n", days, caravans);
            most_caravans = caravans;
        }
        // the trade advisor shows what is on the way (D-051)
        if (mp_trade_loads_on_the_way(1, 0, RESOURCE_MARBLE) == 8 &&
            mp_trade_loads_on_the_way(1, 0, RESOURCE_IRON) == 8 && mp_trade_loads_on_the_way(0, 1, RESOURCE_MARBLE) == 0) {
            shown_on_the_way = 1;
        }
    }
    printf("after %d days: player 1 has %d marble and %d iron, player 2 has %d and %d\n", days, marble_of(0),
        stock_of(0, RESOURCE_IRON), marble_of(1), stock_of(1, RESOURCE_IRON));
    CHECK(most_caravans == 2, "a caravan for each resource, on the road together");
    CHECK(shown_on_the_way, "the advisor shows the 8 loads of each on the way to player 1");
    CHECK(marble_of(0) == 8 && stock_of(0, RESOURCE_IRON) == 8, "they delivered the 8 loads of each");
    CHECK(days <= 70, "both within 70 days (105 tiles of road)");
    printf("player 1 paid %d, player 2 earned %d\n", buyer_money - treasury_of(0), treasury_of(1) - seller_money);
    CHECK(buyer_money - treasury_of(0) == 8 * 150 + 8 * 120, "player 1 paid the prices of player 2");
    CHECK(treasury_of(1) - seller_money == 8 * 150 + 8 * 120, "player 2 earned them");
    CHECK(mp_trade_notifications() >= notices + 2, "each delivery is told");

    player_context_switch(0);
    player_context_set_num_players(1);
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: caravans between players do not work as designed" :
        "Identical: caravans between players work as designed");
    return failures ? 1 : 0;
}

// goods and money between players are neither made nor lost (M8.8): every day, the loads of marble in both cities
// and on the road, and the money of both players, stay the same; a buyer short of money gets what he can pay; a
// caravan stores what finds room, is paid for it, and takes the rest back
// at least `days` days, then until no caravan is on the road (120 days at most): the goods are somewhere every day
static int conserved_days(int days, int marble, int money)
{
    int ok = 1;
    for (int day = 0; day < 120 && (day < days || in_transit(RESOURCE_MARBLE)); day++) {
        run_trace(50, 50, 0, 0);
        int goods = marble_of(0) + marble_of(1) + in_transit(RESOURCE_MARBLE);
        int cash = treasury_of(0) + treasury_of(1);
        if (ok && (goods != marble || cash != money)) {
            printf("  day %d: %d loads instead of %d, %d Dn instead of %d\n", day, goods, marble, cash, money);
            ok = 0;
        }
    }
    return ok;
}

static int command_tradeconservation(const char *file)
{
    if (!start_trade_game(file)) {
        return 2;
    }
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-62s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    setup_trade(16, 0, 1);
    int marble = marble_of(0) + marble_of(1);
    int money = treasury_of(0) + treasury_of(1);
    // a buyer who can pay for 2 loads only
    player_context_switch(0);
    int savings = city_data.finance.treasury - 300;
    city_data.finance.treasury = 300;
    player_context_switch(0);
    money -= savings;
    CHECK(conserved_days(40, marble, money), "short of money: goods and money conserved every day");
    CHECK(marble_of(0) == 2 && treasury_of(0) == 0, "he got the 2 loads he could pay for");

    // his warehouse full of timber but for the space of his 2 loads of marble: the next caravan stores 2 more, he pays
    // for them, the 6 others go back
    player_context_switch(0);
    city_data.finance.treasury += savings;
    money += savings;
    int x0, y0;
    mp_mapgen_city_center(0, &x0, &y0);
    building *store = building_get(map_building_at(map_grid_offset(x0 - 6, y0 + 1)));
    while (building_warehouse_add_resource(store, RESOURCE_TIMBER)) {
    }
    player_context_switch(0);
    int seller_money = treasury_of(1);
    CHECK(conserved_days(20, marble, money), "little room: goods and money conserved every day");
    printf("player 1: %d loads; player 2: %d loads, earned %d\n", marble_of(0), marble_of(1),
        treasury_of(1) - seller_money);
    CHECK(marble_of(0) == 4 && marble_of(1) == 12 && treasury_of(1) - seller_money == 2 * 150,
        "2 loads stored and paid, 6 back to the seller");

    // room again: the rest comes
    player_context_switch(0);
    building_warehouse_remove_resource(store, RESOURCE_TIMBER, 32);
    // the 12 loads left come in two caravans of 8 and 4
    int conserved = 1;
    for (int trip = 0; trip < 4 && marble_of(1) > 0; trip++) {
        conserved &= conserved_days(20, marble, money);
    }
    CHECK(conserved, "room again: goods and money conserved every day");
    CHECK(marble_of(0) == 16 && marble_of(1) == 0, "all the marble came");

    player_context_switch(0);
    player_context_set_num_players(1);
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: trade between players makes or loses goods or money" :
        "Identical: trade between players makes and loses nothing");
    return failures ? 1 : 0;
}

// a city of the empire, open to trade or not yet, that sells the resource, or -1
static int empire_city_selling(int resource)
{
    for (int i = 0; i < 41; i++) {
        empire_city *c = empire_city_get(i);
        if (c && c->in_use && (c->type == EMPIRE_CITY_TRADE || c->type == EMPIRE_CITY_FUTURE_TRADE) &&
            c->sells_resource[resource]) {
            return i;
        }
    }
    return -1;
}

// the empire always sells (T4.3, D-060, replaces D-048): its traders bring a city what it imports even when a
// player sells it cheaper over an open route, and when that player has none left
static int command_empiresells(const char *file)
{
    if (!start_trade_game(file)) {
        return 2;
    }
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-62s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    int empire_price = trade_price_buy(RESOURCE_MARBLE);
    printf("the empire sells marble at %d (portorium included), player 2 at 150\n", empire_price);
    CHECK(empire_price > 150, "the test needs player 2 cheaper than the empire");
    int city_id = empire_city_selling(RESOURCE_MARBLE);
    CHECK(city_id >= 0, "a city of the empire sells marble");
    if (city_id < 0) {
        return 1;
    }
    city_action(0, MP_ACTION_OPEN_TRADE_ROUTE, city_id, 0, 0);
    for (int i = 0; i < 3 && city_resource_trade_status(RESOURCE_MARBLE) != TRADE_STATUS_IMPORT; i++) {
        city_action(0, MP_ACTION_CYCLE_TRADE_STATUS, RESOURCE_MARBLE, 0, 0);
    }
    CHECK(empire_city_get(city_id)->is_open && city_resource_trade_status(RESOURCE_MARBLE) == TRADE_STATUS_IMPORT,
        "player 1 opens the route and imports marble");
    CHECK(empire_can_import_resource_from_city(city_id, RESOURCE_MARBLE), "no route between players: the empire sells");
    setup_trade(8, 0, 1);
    CHECK(mp_trade_cheaper_player(RESOURCE_MARBLE) == 1, "player 2 sells cheaper over an open route (shown green)");
    CHECK(empire_can_import_resource_from_city(city_id, RESOURCE_MARBLE), "the empire still sells");
    // player 2 out of marble: the empire still sells, as it always does
    int x1, y1;
    mp_mapgen_city_center(1, &x1, &y1);
    player_context_switch(1);
    building *store = building_get(map_building_at(map_grid_offset(x1 - 6, y1 + 1)));
    building_warehouse_remove_resource(store, RESOURCE_MARBLE, 8);
    player_context_switch(0);
    CHECK(marble_of(1) == 0 && empire_can_import_resource_from_city(city_id, RESOURCE_MARBLE),
        "player 2 has no marble left: the empire sells");
    city_action(1, MP_ACTION_SET_SELL_PRICE, 0, RESOURCE_MARBLE, empire_price + 50);
    CHECK(mp_trade_cheaper_player(RESOURCE_MARBLE) < 0 && empire_can_import_resource_from_city(city_id, RESOURCE_MARBLE),
        "player 2 dearer: the empire is the cheaper, and sells");
    player_context_switch(0);
    player_context_set_num_players(1);
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: the empire does not always sell" : "Identical: the empire always sells");
    return failures ? 1 : 0;
}

// the state of the trade of a player with the empire (D-061): the cities and routes of his empire, the amounts
// traded, the prices, his trade settings and what he paid and earned with the empire this year
static int empire_trade_state(int player_id, uint8_t *data, int size)
{
    player_context_switch(player_id);
    buffer buf, limits, traded;
    int part = size / 4;
    buffer_init(&buf, data, part);
    empire_city_save_state(&buf);
    trade_prices_save_state(&buf);
    for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        buffer_write_i32(&buf, city_resource_trade_status(r));
        buffer_write_i32(&buf, city_resource_export_over(r));
        buffer_write_i32(&buf, city_resource_is_stockpiled(r));
    }
    buffer_write_i32(&buf, city_data.finance.this_year.expenses.imports);
    buffer_write_i32(&buf, city_data.finance.this_year.income.exports);
    buffer_init(&limits, data + part, part);
    buffer_init(&traded, data + 2 * part, part);
    trade_routes_save_state(&limits, &traded);
    player_context_switch(0);
    return !buf.overflow && !limits.overflow && !traded.overflow;
}

// each player trades with the empire on his own (T4.12, D-061): player 1 opens a route of the empire, imports and
// exports, buys and sells loads, traders come for 60 days; the trade of player 2 with the empire stays the same
static int command_tradeisolation(const char *file)
{
    if (!start_trade_game(file)) {
        return 2;
    }
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-62s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    setup_trade(0, 0, 0);
    // the routes the map opens from the start are closed: player 2 trades with nobody, player 1 opens his own
    for (int p = 0; p < 2; p++) {
        player_context_switch(p);
        for (int i = 0; i < 41; i++) {
            empire_city_get(i)->is_open = 0;
        }
    }
    player_context_switch(0);
    static uint8_t before[4 * 8192], after[4 * 8192], first[4 * 8192];
    memset(before, 0, sizeof(before));
    memset(after, 0, sizeof(after));
    memset(first, 0, sizeof(first));
    CHECK(empire_trade_state(1, before, sizeof(before)) && empire_trade_state(0, first, sizeof(first)),
        "the trade state of each player fits the test buffers");

    int city_id = empire_city_selling(RESOURCE_MARBLE);
    CHECK(city_id >= 0, "a city of the empire sells marble");
    if (city_id < 0) {
        return 1;
    }
    city_action(0, MP_ACTION_OPEN_TRADE_ROUTE, city_id, 0, 0);
    for (int i = 0; i < 3 && city_resource_trade_status(RESOURCE_MARBLE) != TRADE_STATUS_IMPORT; i++) {
        city_action(0, MP_ACTION_CYCLE_TRADE_STATUS, RESOURCE_MARBLE, 0, 0);
    }
    city_action(0, MP_ACTION_CYCLE_TRADE_STATUS, RESOURCE_POTTERY, 0, 0);
    city_action(0, MP_ACTION_CHANGE_EXPORT_OVER, RESOURCE_POTTERY, 5, 0);
    // a load bought and a load sold by player 1, as a trader would
    int x0, y0;
    mp_mapgen_city_center(0, &x0, &y0);
    building *space = building_next(building_get(map_building_at(map_grid_offset(x0 - 6, y0 + 1))));
    building_warehouse_space_add_import(space, RESOURCE_MARBLE);
    building_warehouse_space_remove_export(space, RESOURCE_MARBLE);
    run_trace(60 * 50, 60 * 50, 0, 0);
    CHECK(empire_city_get(city_id)->is_open && city_resource_trade_status(RESOURCE_MARBLE) == TRADE_STATUS_IMPORT,
        "player 1 opened the route and imports marble");
    CHECK(empire_trade_state(0, after, sizeof(after)) && memcmp(first, after, sizeof(after)) != 0,
        "the trade of player 1 with the empire changed");

    memset(after, 0, sizeof(after));
    CHECK(empire_trade_state(1, after, sizeof(after)) && memcmp(before, after, sizeof(after)) == 0,
        "the trade of player 2 with the empire is unchanged");
    player_context_switch(1);
    CHECK(!empire_city_get(city_id)->is_open && city_resource_trade_status(RESOURCE_MARBLE) != TRADE_STATUS_IMPORT,
        "player 2: the route is closed, he does not import marble");
    player_context_switch(0);

    player_context_switch(0);
    player_context_set_num_players(1);
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: the trade of a player with the empire leaks to another" :
        "Identical: each player trades with the empire on his own");
    return failures ? 1 : 0;
}

// a game saved while caravans travel goes on as if it had not been saved (M8.8): the resumed game has the same state
// every tick, and the goods and money arrive the same
static int command_traderesume(const char *file)
{
    if (!start_trade_game(file)) {
        return 2;
    }
    setup_trade(8, 8, 1);
    for (int day = 0; day < 60 && !count_caravans(1); day++) {
        run_trace(50, 50, 0, 0);
    }
    run_trace(3 * 50, 3 * 50, 0, 0);
    int travelling = count_caravans(1);
    printf("caravans on their way when saving: %d\n", travelling);
    const char *mpsav = "traderesume.mpsav";
    if (!travelling || !mp_savegame_write(mpsav)) {
        printf("No caravan on its way, or unable to save\n");
        return 2;
    }
    static uint64_t continued[MAX_SAMPLES], resumed[MAX_SAMPLES];
    int ticks = 60 * 50;
    int n = run_trace(ticks, 1, continued, 0);
    int marble = marble_of(0), iron = stock_of(0, RESOURCE_IRON), money = treasury_of(0);
    if (!mp_savegame_read(mpsav)) {
        printf("Unable to read the saved game\n");
        return 2;
    }
    run_trace(ticks, 1, resumed, 0);
    int same_goods = marble_of(0) == marble && stock_of(0, RESOURCE_IRON) == iron && treasury_of(0) == money;
    printf("delivered: %d marble, %d iron, %d Dn left (continued) / %d, %d, %d (resumed)\n", marble, iron, money,
        marble_of(0), stock_of(0, RESOURCE_IRON), treasury_of(0));
    remove(mpsav);
    player_context_switch(0);
    player_context_set_num_players(1);
    for (int i = 0; i < n && i < MAX_SAMPLES; i++) {
        if (continued[i] != resumed[i]) {
            printf("DIFFERENT: the resumed game diverges at tick %d after loading\n", i);
            return 1;
        }
    }
    if (!same_goods || marble != 8 || iron != 8) {
        printf("DIFFERENT: the goods did not all arrive, or not the same\n");
        return 1;
    }
    printf("Identical: a game saved while caravans travel goes on the same\n");
    return 0;
}

// Caesar the judge (M9.1, CESAR.md): the laurels of every city and the common wrath of Caesar are in the checksum, a
// saved game brings them back and goes on the same tick by tick, and a classic game has none
static int command_caesarstate(const char *file)
{
    if (!start_trade_game(file)) {
        return 2;
    }
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-62s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    CHECK(mp_caesar_is_active() && mp_caesar_laurels(0) == 0 && mp_caesar_laurels(1) == 0 && mp_caesar_wrath() == 0,
        "a new game: Caesar judges, no laurels, no wrath");

    uint64_t before = mp_checksum_state();
    mp_caesar_add_laurels(0, MP_LAURELS_PROSPERITY, 25);
    mp_caesar_add_laurels(0, MP_LAURELS_FESTIVALS, 40);
    mp_caesar_add_laurels(1, MP_LAURELS_TRADE, 13);
    mp_caesar_add_laurels(1, MP_LAURELS_WARS, MP_CAESAR_UNJUST_WAR_LAURELS);
    mp_caesar_add_laurels(2, MP_LAURELS_GIFTS, 70);
    CHECK(mp_checksum_state() != before, "the laurels are in the checksum");
    CHECK(mp_caesar_laurels(0) == 65 && mp_caesar_city_laurels(0) == 25 &&
        mp_caesar_laurels_from(0, MP_LAURELS_FESTIVALS) == 40, "player 1: 6.5 laurels, 2.5 of them from his city");
    CHECK(mp_caesar_laurels(1) == 13 + MP_CAESAR_UNJUST_WAR_LAURELS && mp_caesar_city_laurels(1) == 13,
        "player 2: an unjust war takes him below zero");
    CHECK(mp_caesar_laurels(2) == 0, "no laurels for a player who is not in the game");

    before = mp_checksum_state();
    mp_caesar_add_wrath(1, 240);
    mp_caesar_add_wrath(0, 60);
    CHECK(mp_checksum_state() != before, "the wrath is in the checksum");
    CHECK(mp_caesar_wrath() == 300 && mp_caesar_belligerence(1) == 240 && mp_caesar_belligerence(0) == 60,
        "one gauge for all, a share for each");
    mp_caesar_add_wrath(1, 2 * MP_CAESAR_WRATH_MAX);
    CHECK(mp_caesar_wrath() == MP_CAESAR_WRATH_MAX, "the gauge stops at its top");
    mp_caesar_add_wrath(1, -5 * MP_CAESAR_WRATH_MAX);
    CHECK(mp_caesar_wrath() == 0 && mp_caesar_belligerence(1) == 0 && mp_caesar_belligerence(0) == 60,
        "and at zero, as the share of player 2");
    mp_caesar_add_wrath(1, 240);

    const char *mpsav = "caesarstate.mpsav";
    if (!mp_savegame_write(mpsav)) {
        printf("Unable to save\n");
        return 2;
    }
    static uint64_t continued[MAX_SAMPLES], resumed[MAX_SAMPLES];
    int ticks = 4 * 50;
    int n = run_trace(ticks, 1, continued, 0);
    mp_caesar_add_laurels(0, MP_LAURELS_GIFTS, 100); // gone when the game is loaded
    if (!mp_savegame_read(mpsav)) {
        printf("Unable to read the saved game\n");
        return 2;
    }
    remove(mpsav);
    CHECK(mp_caesar_laurels(0) == 65 && mp_caesar_laurels_from(0, MP_LAURELS_PROSPERITY) == 25 &&
        mp_caesar_laurels(1) == 13 + MP_CAESAR_UNJUST_WAR_LAURELS, "the saved game brings the laurels back");
    CHECK(mp_caesar_wrath() == 240 && mp_caesar_belligerence(1) == 240 && mp_caesar_belligerence(0) == 60,
        "and the wrath with the share of each");
    run_trace(ticks, 1, resumed, 0);
    int same = 1;
    for (int i = 0; i < n && i < MAX_SAMPLES && same; i++) {
        if (continued[i] != resumed[i]) {
            printf("the resumed game diverges at tick %d after loading\n", i);
            same = 0;
        }
    }
    CHECK(same, "the resumed game goes on the same, tick by tick");

    // a classic game: nothing from the multiplayer game, and Caesar does not judge
    player_context_switch(0);
    player_context_set_num_players(1);
    if (!load(file)) {
        return 2;
    }
    mp_caesar_add_laurels(0, MP_LAURELS_FESTIVALS, 40);
    mp_caesar_add_wrath(0, 100);
    CHECK(!mp_caesar_is_active() && mp_caesar_laurels(0) == 0 && mp_caesar_wrath() == 0,
        "a classic game: no laurels, no wrath");
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: the laurels and the wrath of Caesar are not kept as designed" :
        "Identical: the laurels and the wrath of Caesar are kept as designed");
    return failures ? 1 : 0;
}

static void set_caesar_rules(int end_condition, int score)
{
    game_rules_settings rules;
    game_rules_default_multiplayer_settings(&rules);
    rules.ai_invasions = 0;
    rules.end_condition = end_condition;
    rules.caesar_score = score;
    game_rules_set_multiplayer(&rules);
}

static void run_to_next_month(void)
{
    int month = game_time_month();
    for (int i = 0; i < 40 * 50 && game_time_month() == month; i++) {
        run_trace(1, 1, 0, 0);
    }
}

// Caesar the judge, without its complex mechanics (M9.2 provisional, M9.5): every month a city gets the laurels of
// its five notes; its rank follows its laurels, with a letter when it rises; the first city to the score wins
static int command_caesarlaurels(const char *file)
{
    if (!load(file)) {
        return 2;
    }
    set_caesar_rules(GAME_END_CAESAR, 500);
    mp_caesar_reset();
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-62s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    static const int weights[MP_NOTE_MAX] = {
        MP_CAESAR_WEIGHT_PROSPERITY, MP_CAESAR_WEIGHT_TRADE, MP_CAESAR_WEIGHT_HOUSING, MP_CAESAR_WEIGHT_CULTURE,
        MP_CAESAR_WEIGHT_GREATNESS
    };
    run_to_next_month();
    int expected = 0, in_range = 1;
    for (int note = 0; note < MP_NOTE_MAX; note++) {
        int value = mp_caesar_note(0, note);
        in_range &= value >= 0 && value <= 100;
        expected += weights[note] * value / 100;
    }
    printf("notes: prosperity %d, trade %d, housing %d, culture %d, greatness %d (population %d)\n",
        mp_caesar_note(0, MP_NOTE_PROSPERITY), mp_caesar_note(0, MP_NOTE_TRADE), mp_caesar_note(0, MP_NOTE_HOUSING),
        mp_caesar_note(0, MP_NOTE_CULTURE), mp_caesar_note(0, MP_NOTE_GREATNESS), city_population());
    CHECK(in_range && mp_caesar_note(0, MP_NOTE_HOUSING) > 0 && mp_caesar_note(0, MP_NOTE_GREATNESS) > 0,
        "a city with people: five notes from 0 to 100");
    CHECK(mp_caesar_note(0, MP_NOTE_PROSPERITY) == city_rating_prosperity() &&
        mp_caesar_note(0, MP_NOTE_CULTURE) == city_rating_culture(), "prosperity and culture: the original ratings");
    int population = city_population();
    CHECK(mp_caesar_note(0, MP_NOTE_GREATNESS) == 100 * population / (population + MP_CAESAR_GREATNESS_REFERENCE),
        "greatness: 100 * population / (population + 8000)");
    CHECK(mp_caesar_city_laurels(0) == expected && expected > 0, "the first month brings the laurels of the notes");
    int first_month = mp_caesar_city_laurels(0);
    run_to_next_month();
    CHECK(mp_caesar_city_laurels(0) > first_month, "and the next month more");
    CHECK(mp_caesar_laurels(0) == mp_caesar_city_laurels(0), "no laurels of Caesar without serving him");

    // ranks: one for each tenth of the score, a letter to the local player when he rises
    CHECK(mp_caesar_rank_laurels(1) == 50 && mp_caesar_rank_laurels(10) == 500, "a rank for each tenth of the score");
    int letters_before = mp_caesar_num_letters();
    mp_caesar_add_laurels(0, MP_LAURELS_GIFTS, 10 * 160);
    CHECK(mp_caesar_rank(0) == 3, "160 laurels and more: the fourth rank");
    run_to_next_month();
    const mp_caesar_letter *letter = mp_caesar_get_letter(mp_caesar_num_letters() - 1);
    CHECK(mp_caesar_num_letters() == letters_before + 1 && letter && letter->type == MP_CAESAR_LETTER_PROMOTION &&
        letter->param == mp_caesar_rank(0), "Caesar writes to the player he promotes");
    run_to_next_month();
    CHECK(mp_caesar_num_letters() == letters_before + 1, "once only");

    // the score wins
    CHECK(!mp_endgame_is_over(), "below the score, the game goes on");
    mp_caesar_add_laurels(0, MP_LAURELS_GIFTS, 10 * 500);
    run_trace(1, 1, 0, 0);
    CHECK(mp_endgame_is_over() && mp_endgame_winner() == 0 && mp_endgame_score(0) == mp_caesar_laurels(0) / 10,
        "at the score, the city wins alone");

    // two cities at the score the same month: the most laurels win, then the most laurels of the city
    mp_endgame_reset();
    if (!start_trade_game("brugle-massilia-start.sav")) {
        return 2;
    }
    set_caesar_rules(GAME_END_CAESAR, 500);
    mp_caesar_add_laurels(0, MP_LAURELS_PROSPERITY, 1000);
    mp_caesar_add_laurels(0, MP_LAURELS_GIFTS, 4000);
    mp_caesar_add_laurels(1, MP_LAURELS_PROSPERITY, 2000);
    mp_caesar_add_laurels(1, MP_LAURELS_GIFTS, 3000);
    run_trace(1, 1, 0, 0);
    CHECK(mp_endgame_is_over() && mp_endgame_winner() == 1 && mp_endgame_score(0) == 500 &&
        mp_endgame_score(1) == 500, "a tie: the city with the most laurels of its own");
    int ranking[PLAYER_CONTEXT_MAX_PLAYERS];
    CHECK(mp_caesar_ranking(ranking) == 2 && ranking[0] == 1 && ranking[1] == 0, "the ranking says the same");

    // the score of the lobby travels with the saved game
    const char *mpsav = "caesarlaurels.mpsav";
    if (!mp_savegame_write(mpsav) || !mp_savegame_read(mpsav)) {
        printf("Unable to save or read the saved game\n");
        return 2;
    }
    remove(mpsav);
    CHECK(game_rules_caesar_score() == 500 && game_rules_end_condition() == GAME_END_CAESAR &&
        mp_caesar_laurels(1) == 5000, "the score of the lobby is in the saved game");
    player_context_switch(0);
    player_context_set_num_players(1);
    mp_endgame_reset();

    // a classic game: no notes, no laurels
    options.multiplayer = 0;
    if (!load(file)) {
        return 2;
    }
    run_to_next_month();
    CHECK(!mp_caesar_is_active() && mp_caesar_laurels(0) == 0 && mp_caesar_note(0, MP_NOTE_GREATNESS) == 0,
        "a classic game: no notes, no laurels");
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: Caesar does not judge as designed" : "Identical: Caesar judges as designed");
    return failures ? 1 : 0;
}

// the personal savings of a player (his governor, not his treasury)
static int savings_of(int player_id)
{
    player_context_switch(player_id);
    int savings = city_emperor_personal_savings();
    player_context_switch(0);
    return savings;
}

static void set_savings(int player_id, int savings)
{
    player_context_switch(player_id);
    city_data.emperor.personal_savings = savings;
    player_context_switch(0);
}

static int salary_rank_of(int player_id)
{
    player_context_switch(player_id);
    int rank = city_emperor_salary_rank();
    player_context_switch(0);
    return rank;
}

static int salary_amount_of(int player_id)
{
    player_context_switch(player_id);
    int amount = city_emperor_salary_amount();
    player_context_switch(0);
    return amount;
}

// the sim to the next month, without a checksum at every tick (a large map makes it slow)
static void run_to_next_month_fast(void)
{
    setting_reset_speeds(500, setting_scroll_speed());
    int month = game_time_month();
    for (int i = 0; i < 40 * 50 && game_time_month() == month; i++) {
        run_one_tick();
    }
}

// months of Caesar's laurels for the city of a player, without waiting for the sim
static void caesar_months(int player_id, int months)
{
    player_context_switch(player_id);
    for (int i = 0; i < months; i++) {
        mp_caesar_update_city_month();
    }
    player_context_switch(0);
}

// One scripted game of gifts, salary and donations (T4.2, D-067). With verify, it checks every step; without, it
// only plays it, so that a second machine can be compared with the first.
static int play_gifts(int verify)
{
    int failures = 0;
#define CHECK(condition, text) do { if (verify) { int ok_ = (condition); printf("%-62s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } } while (0)
    // a gift costs savings / 8 + 20, / 4 + 50 or / 2 + 100, and is paid from the sender's savings only
    int cost = 1000 / 8 + 20;
    city_action(0, MP_ACTION_SEND_GIFT, GIFT_MODEST, 0, 0);
    CHECK(savings_of(0) == 1000 - cost, "a modest gift lowers the savings of the sender");
    CHECK(savings_of(1) == 1000 && mp_caesar_laurels(1) == 0, "and nothing for the other player");
    CHECK(mp_caesar_laurels_from(0, MP_LAURELS_GIFTS) == 40 && mp_caesar_laurels(0) == 3000 + 40,
        "it brings 4 laurels of gifts, to him only");
    CHECK(mp_caesar_gift_cooldown(0) == MP_CAESAR_GIFT_PERIOD && mp_caesar_gift_cooldown(1) == 0,
        "only one gift counts every 12 months");

    // a second gift costs again but counts for nothing
    int savings = savings_of(0);
    city_action(0, MP_ACTION_SEND_GIFT, GIFT_GENEROUS, 0, 0);
    CHECK(savings_of(0) == savings - (savings / 4 + 50), "a second gift is paid all the same");
    CHECK(mp_caesar_laurels_from(0, MP_LAURELS_GIFTS) == 40, "but it brings no laurels within the 12 months");

    // the other player, a lavish one
    city_action(1, MP_ACTION_SEND_GIFT, GIFT_LAVISH, 0, 0);
    CHECK(savings_of(1) == 1000 - (1000 / 2 + 100) && mp_caesar_laurels_from(1, MP_LAURELS_GIFTS) == 100,
        "a lavish gift of player 2: 10 laurels, his savings only");
    CHECK(mp_caesar_laurels_from(0, MP_LAURELS_GIFTS) == 40, "player 1 is not touched by it");

    // what the savings cannot pay is not sent
    set_savings(1, 10);
    caesar_months(1, MP_CAESAR_GIFT_PERIOD);
    city_action(1, MP_ACTION_SEND_GIFT, GIFT_MODEST, 0, 0);
    CHECK(savings_of(1) == 10 && mp_caesar_laurels_from(1, MP_LAURELS_GIFTS) == 100,
        "a gift above the savings is refused, and gives nothing");
    savings = savings_of(0);
    city_action(0, MP_ACTION_SEND_GIFT, 7, 0, 0);
    city_action(0, MP_ACTION_SEND_GIFT, -1, 0, 0);
    CHECK(savings_of(0) == savings, "an unknown size of gift is refused");

    // the salary is limited by the rank (3 at 300 laurels), and takes effect for him only
    int rank_of_1 = salary_rank_of(1);
    CHECK(mp_caesar_rank(0) == 3, "player 1 is of rank 3");
    city_action(0, MP_ACTION_SET_SALARY, 5, 0, 0);
    CHECK(salary_rank_of(0) != 5, "a salary above the rank is refused");
    city_action(0, MP_ACTION_SET_SALARY, 3, 0, 0);
    CHECK(salary_rank_of(0) == 3 && salary_amount_of(0) == 8, "the salary of his rank, 8 denarii, is accepted");
    city_action(0, MP_ACTION_SET_SALARY, 11, 0, 0);
    city_action(0, MP_ACTION_SET_SALARY, -1, 0, 0);
    CHECK(salary_rank_of(0) == 3, "no such rank, no change");
    CHECK(salary_rank_of(1) == rank_of_1, "the salary of the other player stays");

    // the salary is paid every month, from the treasury to the savings
    int savings_0 = savings_of(0), savings_1 = savings_of(1);
    run_to_next_month_fast();
    CHECK(savings_of(0) == savings_0 + 8, "a month later, 8 denarii more in his savings");
    CHECK(savings_of(1) == savings_1 && salary_amount_of(1) == 0, "while the player without rank is paid nothing");
    CHECK(mp_caesar_gift_cooldown(0) == MP_CAESAR_GIFT_PERIOD - 1, "and the next gift is a month nearer");

    // a donation: savings to the treasury
    int treasury = treasury_of(0);
    savings = savings_of(0);
    city_action(0, MP_ACTION_DONATE, 300, 0, 0);
    CHECK(savings_of(0) == savings - 300 && treasury_of(0) == treasury + 300, "a donation of 300: savings to treasury");
    treasury = treasury_of(1);
    city_action(1, MP_ACTION_DONATE, 1000000, 0, 0);
    CHECK(savings_of(1) == 0 && treasury_of(1) == treasury + 10, "a donation cannot exceed the savings");
    city_action(1, MP_ACTION_DONATE, -5, 0, 0);
    CHECK(savings_of(1) == 0 && treasury_of(1) == treasury + 10, "nor be negative");

    // a rank lost takes the salary with it
    mp_caesar_add_laurels(0, MP_LAURELS_PROSPERITY, -3000);
    savings_0 = savings_of(0);
    run_to_next_month_fast();
    CHECK(mp_caesar_rank(0) == 0 && salary_rank_of(0) == 0 && savings_of(0) == savings_0,
        "a lost rank lowers the salary: nothing paid");

    // after the 12 months, a gift counts again
    caesar_months(0, MP_CAESAR_GIFT_PERIOD);
    CHECK(mp_caesar_gift_cooldown(0) == 0, "12 months after the gift that counted, the next one counts");
    set_savings(0, 1000);
    city_action(0, MP_ACTION_SEND_GIFT, GIFT_LAVISH, 0, 0);
    CHECK(mp_caesar_laurels_from(0, MP_LAURELS_GIFTS) == 40 + 100 && mp_caesar_gift_cooldown(0) == MP_CAESAR_GIFT_PERIOD,
        "a lavish gift: 10 laurels more, and the wait begins again");
#undef CHECK
    return failures;
}

// state of version 2 of the piece of Caesar, as written before the gifts had a waiting time
static void write_old_caesar_state(buffer *buf)
{
    buffer_write_i32(buf, 2);
    buffer_write_i32(buf, PLAYER_CONTEXT_MAX_PLAYERS);
    buffer_write_i32(buf, MP_LAURELS_MAX_SOURCE);
    for (int p = 0; p < PLAYER_CONTEXT_MAX_PLAYERS; p++) {
        for (int source = 0; source < MP_LAURELS_MAX_SOURCE; source++) {
            buffer_write_i32(buf, source == MP_LAURELS_GIFTS ? 100 * (p + 1) : 0);
        }
    }
    buffer_write_i32(buf, 250); // wrath
    for (int p = 0; p < PLAYER_CONTEXT_MAX_PLAYERS; p++) {
        buffer_write_i32(buf, 10 * p);
    }
    for (int i = 0; i < PLAYER_CONTEXT_MAX_PLAYERS * MP_NOTE_MAX * 2 + PLAYER_CONTEXT_MAX_PLAYERS; i++) {
        buffer_write_i32(buf, 0); // notes, remainders, ranks
    }
}

// Gifts, salary and donations to Caesar are network commands (T4.2, D-067): they act in the city of the sender,
// give laurels as designed, and two machines playing them end up the same
static int command_caesargifts(const char *file)
{
    if (!start_trade_game(file)) {
        return 2;
    }
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-62s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    set_savings(0, 1000);
    set_savings(1, 1000);
    mp_caesar_add_laurels(0, MP_LAURELS_PROSPERITY, 3000); // 300 laurels: rank 3
    const char *start = "caesargifts.mpsav";
    if (!mp_savegame_write(start)) {
        printf("Unable to save\n");
        return 2;
    }
    CHECK(mp_caesar_gift_laurels(GIFT_MODEST) == 40 && mp_caesar_gift_laurels(GIFT_GENEROUS) == 70 &&
        mp_caesar_gift_laurels(GIFT_LAVISH) == 100 && mp_caesar_gift_laurels(3) == 0,
        "the gifts bring 4, 7 and 10 laurels");
    failures += play_gifts(1);
    uint64_t first = mp_checksum_state();
    int laurels_0 = mp_caesar_laurels(0), laurels_1 = mp_caesar_laurels(1);
    int savings_0 = savings_of(0), savings_1 = savings_of(1);

    // the second machine plays the same commands from the same start
    if (!mp_savegame_read(start)) {
        printf("Unable to read the saved game\n");
        return 2;
    }
    CHECK(mp_caesar_laurels(0) == 3000 && savings_of(0) == 1000, "the start is back");
    play_gifts(0);
    CHECK(mp_checksum_state() == first, "two machines playing the same commands end with the same checksum");
    CHECK(mp_caesar_laurels(0) == laurels_0 && mp_caesar_laurels(1) == laurels_1 && savings_of(0) == savings_0 &&
        savings_of(1) == savings_1, "the same laurels and the same savings");

    // the waiting time is in the saved game; K-review-fixes (T4.2): the favor of the original stays frozen (D-026,
    // D-067), and so do the state that moves it (penalty of the gifts, months since the last gift)
    set_savings(1, 1000);
    player_context_switch(1);
    city_data.ratings.favor = 50;
    city_data.emperor.gift_overdose_penalty = 0;
    city_data.emperor.months_since_gift = 7;
    player_context_switch(0);
    city_action(1, MP_ACTION_SEND_GIFT, GIFT_MODEST, 0, 0);
    int cooldown = mp_caesar_gift_cooldown(1);
    player_context_switch(1);
    printf("after a gift: favor %d, penalty %d, months since the gift %d, savings %d\n", city_rating_favor(),
        city_data.emperor.gift_overdose_penalty, city_data.emperor.months_since_gift, city_emperor_personal_savings());
    CHECK(city_rating_favor() == 50 && city_data.emperor.gift_overdose_penalty == 0 &&
        city_data.emperor.months_since_gift == 7, "a gift leaves the favor of the original as it was");
    CHECK(city_emperor_personal_savings() == 1000 - (1000 / 8 + 20), "and is paid from the savings");
    player_context_switch(0);
    static uint8_t bytes[1024];
    buffer buf;
    buffer_init(&buf, bytes, sizeof(bytes));
    mp_caesar_save_state(&buf);
    mp_caesar_reset();
    CHECK(mp_caesar_gift_cooldown(1) == 0, "(a game without gifts has no waiting time)");
    buffer_set(&buf, 0);
    mp_caesar_load_state(&buf);
    CHECK(cooldown > 0 && mp_caesar_gift_cooldown(1) == cooldown, "the waiting time of the gifts is saved and loaded");

    // a game saved before: no waiting time, everything else kept
    buffer_init(&buf, bytes, sizeof(bytes));
    write_old_caesar_state(&buf);
    buffer_set(&buf, 0);
    mp_caesar_load_state(&buf);
    CHECK(mp_caesar_laurels(0) == 100 && mp_caesar_laurels(1) == 200 && mp_caesar_wrath() == 250 &&
        mp_caesar_belligerence(1) == 10, "an older saved game still loads, with its laurels and its wrath");
    CHECK(mp_caesar_gift_cooldown(0) == 0 && mp_caesar_gift_cooldown(3) == 0, "and no waiting time for the gifts");
    remove(start);

    // a classic game: a gift costs savings, as before, and Caesar gives no laurels
    player_context_switch(0);
    player_context_set_num_players(1);
    if (!load(file)) {
        return 2;
    }
    set_savings(0, 1000);
    city_action(0, MP_ACTION_SEND_GIFT, GIFT_MODEST, 0, 0);
    CHECK(!mp_caesar_is_active() && mp_caesar_laurels(0) == 0 && savings_of(0) == 1000 - (1000 / 8 + 20),
        "a classic game: the gift is paid, no laurels");
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: the gifts, the salary and the donations are not as designed" :
        "Identical: the gifts, the salary and the donations are as designed");
    return failures ? 1 : 0;
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
                if (f->state == FIGURE_STATE_ALIVE) { // every type fits: figure_type is a byte
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
        } else if (strcmp(argv[first], "--map") == 0 && first + 1 < argc &&
            atoi(argv[first + 1]) >= 1 && atoi(argv[first + 1]) <= MP_MAPGEN_NUM_PREPARED_MAPS) {
            options.prepared_map = atoi(argv[first + 1]) - 1;
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
    } else if (strcmp(command, "lobbyrules") == 0) {
        result = command_lobbyrules(file);
    } else if (strcmp(command, "resumerules") == 0 && argc > 3) {
        result = command_resumerules(file, atoi(argv[3]), argc > 4 ? atoi(argv[4]) : 0);
    } else if (strcmp(command, "badrules") == 0) {
        result = command_badrules(atoi(file));
    } else if (strcmp(command, "attacksource") == 0 && argc > 4) {
        result = command_attacksource(file, argv[3], atoi(argv[4]));
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
    } else if (strcmp(command, "drying") == 0 && argc > 3) {
        result = command_drying(file, atoi(argv[3]));
    } else if (strcmp(command, "inlandwater") == 0 && argc > 3) {
        result = command_inlandwater(file, atoi(argv[3]));
    } else if (strcmp(command, "farinvasion") == 0) {
        result = command_farinvasion(file);
    } else if (strcmp(command, "mapchoice") == 0) {
        result = command_mapchoice(file);
    } else if (strcmp(command, "longroutes") == 0) {
        result = command_longroutes(file, argc > 3 ? atoi(argv[3]) : 4);
    } else if (strcmp(command, "menuowner") == 0) {
        result = command_menuowner(file);
    } else if (strcmp(command, "caravans") == 0) {
        result = command_caravans(file);
    } else if (strcmp(command, "tradeprices") == 0) {
        result = command_tradeprices(file);
    } else if (strcmp(command, "importprice") == 0) {
        result = command_importprice(file);
    } else if (strcmp(command, "notrade") == 0) {
        result = command_notrade(file);
    } else if (strcmp(command, "terrain") == 0 && argc > 7) {
        result = command_terrain(file, atoi(argv[3]), atoi(argv[4]), atoi(argv[5]), atoi(argv[6]), atoi(argv[7]),
            argc > 8 && strcmp(argv[8], "raw") == 0);
    } else if (strcmp(command, "empiresells") == 0) {
        result = command_empiresells(file);
    } else if (strcmp(command, "tradeisolation") == 0) {
        result = command_tradeisolation(file);
    } else if (strcmp(command, "tradeconservation") == 0) {
        result = command_tradeconservation(file);
    } else if (strcmp(command, "traderesume") == 0) {
        result = command_traderesume(file);
    } else if (strcmp(command, "caesarstate") == 0) {
        result = command_caesarstate(file);
    } else if (strcmp(command, "caesarlaurels") == 0) {
        result = command_caesarlaurels(file);
    } else if (strcmp(command, "caesargifts") == 0) {
        result = command_caesargifts(file);
    } else if (strcmp(command, "inspect") == 0) {
        result = command_inspect(file);
    } else if (strcmp(command, "tradecities") == 0) {
        result = command_tradecities(file);
    } else if (strcmp(command, "fog") == 0) {
        result = command_fog(file);
    } else if (strcmp(command, "messagelocation") == 0) {
        result = command_messagelocation(file);
    } else if (strcmp(command, "missionarybridge") == 0) {
        result = command_missionarybridge(file);
    } else if (strcmp(command, "placement") == 0) {
        result = command_placement(file);
    } else if (strcmp(command, "reachable") == 0 && argc > 3) {
        result = command_reachable(file, atoi(argv[3]));
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
