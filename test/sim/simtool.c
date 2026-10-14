// Headless simulation tool: loads saved games, runs ticks exactly like the autopilot and
// prints state checksums. See doc/mp/TESTING.md.
#include "building/barracks.h"
#include "building/building.h"
#include "building/construction.h"
#include "figure/figure.h"
#include "figure/formation.h"
#include "figure/properties.h"
#include "building/construction_clear.h"
#include "building/destruction.h"
#include "building/type.h"
#include "building/count.h"
#include "building/menu.h"
#include "building/model.h"
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
#include "scenario/editor.h"
#include "scenario/empire.h"
#include "scenario/editor_map.h"
#include "scenario/invasion.h"
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
#include "core/random.h"
#include "core/time.h"
#include "game/difficulty.h"
#include "game/file.h"
#include "game/file_editor.h"
#include "game/game.h"
#include "game/rules.h"
#include "game/settings.h"
#include "core/image.h"
#include "map/bridge.h"
#include "map/building_tiles.h"
#include "map/elevation.h"
#include "map/aqueduct.h"
#include "map/image.h"
#include "map/terrain.h"
#include "map/grid.h"
#include "figure/route.h"
#include "mp/audit.h"
#include "translation/translation.h"
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
#include "mp/expedition.h"
#include "mp/permissions.h"
#include "mp/mapgen.h"
#include "city/map.h"
#include "mp/checksum.h"
#include "game/time.h"
#include "game/player_context.h"
#include "map/data.h"
#include "mp/actions.h"
#include "mp/assault.h"
#include "mp/compose.h"
#include "mp/savegame.h"
#include "mp/trade.h"
#include "mp/war.h"
#include "mp/wrath.h"
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
    printf("                                         'war' (with 'cities'): a scripted war, then peace (T5.5)\n");
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
    printf("  simtool restrictiveness SAVE [blank|MAP...] the same small city built by commands and played two years under\n");
    printf("                                         classic and multiplayer rules, alone and with four cities, easy\n");
    printf("                                         and hard, and on free land of a classic map (blank: made here;\n");
    printf("                                         MAP: a .map of the game): table, fails if they differ (T4.16)\n");
    printf("  simtool troops                         javelin throwers: 136 x 10 in a multiplayer game, the table of\n");
    printf("                                         the original in a classic one; the other troops unchanged (MG.1)\n");
    printf("  simtool barrackstimber SAVE            prepared map: the barracks recruits a javelin thrower with a load of\n");
    printf("                                         timber, a legionary with weapons, a horseman for nothing (MG.2)\n");
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
    printf("  simtool tradegame SAVE [PLAYERS [DRAW]]  every player sells to every other in a game started as from the\n");
    printf("                                         lobby: a caravan every month for each resource bought (T5.7)\n");
    printf("  simtool tradesave SAVE OUT             writes the game of tools/mp-trade3-test.sh: three players, a\n");
    printf("                                         warehouse each, marble and iron to sell for player 2\n");
    printf("  simtool tradewarehouse SAVE [PLAYERS [DRAW]]  a first warehouse apart does not stop the caravans\n");
    printf("  simtool tradestatus SAVE [PLAYERS [DRAW]]  why no caravan leaves, as the trade page shows it\n");
    printf("  simtool caesarstate SAVE               laurels and wrath of Caesar: in the checksum, saved, resumed\n");
    printf("  simtool caesarlaurels SAVE             notes and monthly laurels of a city, ranks, the score wins\n");
    printf("  simtool caesarhistory SAVE             monthly history of the laurels of each city: gain, trend, saved\n");
    printf("  simtool war SAVE                       prepared map: war between players by commands, saved; legions\n");
    printf("                                         fight in the enemy city, pull down buildings, take caravans (T5.5)\n");
    printf("  simtool warattack SAVE                 the order \"attack\" of a legion at war (M10.2, D-087)\n");
    printf("  simtool attacksave SAVE OUT            writes the game of tools/mp-attack-test.sh: two players at war\n");
    printf("  simtool warmotives SAVE                motives and price of a war, triumphs (M10.4)\n");
    printf("  simtool caesarwrath SAVE               Caesar's wrath: examples of CESAR 7.2, ultimatum (M10.5)\n");
    printf("  simtool armytolerance SAVE             legions beyond what Caesar tolerates cost laurels (M10.7)\n");
    printf("  simtool caesarexpedition SAVE          Caesar's punitive expedition, disgrace, second one (M10.6)\n");
    printf("  simtool terrain MAP PLAYERS X Y W H [raw]  the terrain of a part of the prepared map (or of an .mpsav),\n");
    printf("                                         one letter a tile; raw: terrain bits, image and elevation too\n");
    printf("  simtool tradecities MAP                trade cities of the empire of a map, by land and by sea\n");
    printf("  simtool fog SAVE                       prepared map: what a player discovers and sees (D-038)\n");
    printf("  simtool outside SAVE                   prepared map: buildings outside the zone collapse in 3 months\n");
    printf("  simtool missions SAVE                  prepared map: missions near a missionary, marble, training\n");
    printf("  simtool caesarland SAVE PLAYERS        prepared map: nothing is built on the land of Caesar (D-082)\n");
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
    printf("  simtool windowwrites SAVE              a window must not write into the game: the religion advisor of a\n"
           "                                         network game, with an angry god, leaves the checksum alone (T5.3)\n");
    printf("  simtool salaries SAVE                 three players: Rome pays each the salary of his rank, the same\n"
           "                                         start and table for everybody (T5.2)\n");
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
// and every success proves the map is the chosen one: its width is the one of that map, so a test run with --map 2
// can not silently run on map 1
static int create_prepared(const char *file, int num_players, unsigned int placement_seed)
{
    if (!mp_mapgen_create_prepared_map(file, num_players, options.prepared_map, placement_seed)) {
        return 0;
    }
    int expected = mp_mapgen_prepared_map_size(options.prepared_map, num_players);
    if (map_grid_width() != expected) {
        printf("FAILED: map %d for %d players should be %d tiles wide, it is %d\n", options.prepared_map + 1,
            num_players, expected, map_grid_width());
        return 0;
    }
    return 1;
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

// Scripted player of the save-and-resume test (T5.4): trade with the next player (prices, purchases, route, limits
// of the stocks), so that the saved game holds a state of the trade between players
static void mpnode_trade(int tick_in_game)
{
    int player = mp_session_local_player_id();
    int other = (player + 1) % player_context_num_players();
    if (other == player) {
        return;
    }
    switch (tick_in_game - player) {
        case 20: mp_action_set_sell_price(other, RESOURCE_MARBLE, 120 + 10 * player); break;
        case 24: mp_action_set_buys_from(other, RESOURCE_TIMBER, 1); break;
        case 28: mp_action_propose_route(other, 1); break;
        case 32: mp_action_change_buy_limit(RESOURCE_POTTERY, 2 + player); break;
        case 36: mp_action_change_export_over(RESOURCE_WINE, 1 + player); break;
        default: break;
    }
}

// 'war' (T5.5): player 1 declares a brutal war on player 2 and sends his first legion to the copy of his second fort
// in the city of player 2; at three quarters of the game both propose peace. Every computer must end with the same
// war, the same soldiers lost and the same checksum.
static int mpnode_war_ticks;

static void mpnode_play_war(int tick_in_game)
{
    int player = mp_session_local_player_id();
    int peace_tick = mpnode_war_ticks * 3 / 4;
    // a threat first: an honourable war, withdrawn during its notice (D-080)
    if (player == 0 && tick_in_game == 8) {
        mp_action_declare_war(1, MP_WAR_HONOURABLE);
    }
    if (player == 0 && tick_in_game == 14) {
        mp_action_withdraw_war(1);
    }
    if (player == 0 && tick_in_game == 20) {
        mp_action_declare_war(1, MP_WAR_BRUTAL);
    }
    if (player == 0 && tick_in_game == 24 && formation_get_num_legions() >= 2) {
        int x0, y0, x1, y1, size;
        mp_compose_city_area(0, MP_COMPOSE_CITY_GAP, &x0, &y0, &size);
        mp_compose_city_area(1, MP_COMPOSE_CITY_GAP, &x1, &y1, &size);
        const formation *attacker = formation_get(formation_for_legion(1));
        const formation *target = formation_get(formation_for_legion(2));
        mp_action_legion_move(attacker->id, target->x_home + x1 - x0, target->y_home + y1 - y0 - 3);
    }
    if (player == 0 && tick_in_game == peace_tick) {
        mp_action_propose_peace(1, 1);
    }
    if (player == 1 && tick_in_game == peace_tick + 8) {
        mp_action_propose_peace(0, 1);
    }
}

static int mpnode_soldiers_of(int player_id)
{
    int count = 0;
    for (int i = player_id * MAX_FIGURES + 1; i < (player_id + 1) * MAX_FIGURES; i++) {
        figure *f = figure_get(i);
        count += f->state == FIGURE_STATE_ALIVE && figure_is_legion(f) && f->action_state != FIGURE_ACTION_149_CORPSE;
    }
    return count;
}

// T4.11: the rules chosen in the lobby, as a player sees them
static int same_lobby_rules(const game_rules_settings *a, const game_rules_settings *b)
{
    return a->difficulty == b->difficulty && a->gods_enabled == b->gods_enabled &&
        a->ai_invasions == b->ai_invasions && a->fog_of_war == b->fog_of_war &&
        a->end_condition == b->end_condition && a->caesar_score == b->caesar_score &&
        a->prepared_map == b->prepared_map && a->rome_takes_province == b->rome_takes_province;
}

static void print_lobby_rules(const char *label, const game_rules_settings *r)
{
    printf("%s: difficulty %d gods %d invasions %d fog %d end %d score %d map %d second expedition ends %d\n", label,
        r->difficulty, r->gods_enabled, r->ai_invasions, r->fog_of_war, r->end_condition, r->caesar_score,
        r->prepared_map, r->rome_takes_province);
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
    mp_lobby_change_rule(MP_LOBBY_RULE_SECOND_EXPEDITION);
}

static int every_lobby_rule_differs(const game_rules_settings *a, const game_rules_settings *b)
{
    return a->difficulty != b->difficulty && a->gods_enabled != b->gods_enabled &&
        a->ai_invasions != b->ai_invasions && a->fog_of_war != b->fog_of_war && a->caesar_score != b->caesar_score &&
        a->prepared_map != b->prepared_map && a->rome_takes_province != b->rome_takes_province;
}

// the state of the trade between players, which every player of a game must see the same (T5.4)
static void print_trade_state(const char *when)
{
    int n = player_context_num_players();
    for (int p = 0; p < n; p++) {
        int other = (p + 1) % n;
        if (other == p) {
            continue;
        }
        printf("trade of city %d (%s): marble price %d, buys timber %d, route %d, pottery limit %d\n", p, when,
            mp_trade_price(p, other, RESOURCE_MARBLE), mp_trade_buys_from(p, other, RESOURCE_TIMBER),
            mp_trade_route_is_proposed(p, other), mp_trade_buy_limit(p, RESOURCE_POTTERY));
    }
}

// T5.1, T5.2: what each city starts with, as every machine of the network sees it
static void set_local_difficulty(int difficulty);

static void count_trade_cities(int *land, int *sea);
static int check_start_cities(int expected_difficulty, int from_map)
{
    int ok = 1;
    int local = player_context_current();
    if (game_rules_difficulty() != expected_difficulty) {
        printf("WRONG: the game is played at difficulty %d, not %d\n", game_rules_difficulty(), expected_difficulty);
        ok = 0;
    }
    printf("start difficulty %d\n", game_rules_difficulty());
    // what the difficulty decides while playing: the base sentiment, the size of the armies, the bite of the wolves
    int easy = expected_difficulty == DIFFICULTY_EASY;
    printf("sentiment %d, enemies %d%%, wolf attack %d\n", difficulty_sentiment(), difficulty_adjust_enemies(100),
        difficulty_adjust_wolf_attack(8));
    if (difficulty_sentiment() != (easy ? 70 : 50) || difficulty_adjust_enemies(100) != (easy ? 60 : 100) ||
        difficulty_adjust_wolf_attack(8) != (easy ? 4 : 8)) {
        printf("WRONG: the sentiment, enemies or wolves are not those of the difficulty of the game\n");
        ok = 0;
    }
    for (int p = 0; p < player_context_num_players(); p++) {
        player_context_switch(p);
        int funds = difficulty_adjust_money(scenario_initial_funds());
        printf("start city %d: treasury %d favor %d savings %d salary rank %d amount %d rank %d\n", p,
            city_finance_treasury(), city_rating_favor(), city_emperor_personal_savings(),
            city_emperor_salary_rank(), city_emperor_salary_amount(), city_emperor_rank());
        if (city_finance_treasury() != funds) {
            printf("WRONG: city %d starts with %d denarii, the funds of the scenario at this difficulty are %d\n", p,
                city_finance_treasury(), funds);
            ok = 0;
        }
        if (from_map && city_rating_favor() != difficulty_starting_favor()) {
            printf("WRONG: city %d starts with favor %d, not %d\n", p, city_rating_favor(),
                difficulty_starting_favor());
            ok = 0;
        }
        if (city_emperor_personal_savings() || city_emperor_salary_rank() || city_emperor_salary_amount() ||
            city_emperor_rank()) {
            printf("WRONG: city %d does not start with the rank, salary and savings of everybody\n", p);
            ok = 0;
        }
    }
    player_context_switch(local);
    return ok;
}

// T5.3 review: the local player drags a road while a month starts; the monthly saved game written then must not
// hold the preview of that road (which nobody built nor paid)
#define DRAG_LENGTH 6
static struct {
    int state; // 0: not yet, 1: dragging, 2: done, -1: no place to drag
    int month;
    int x, y;
    char autosave[64];
    char copy[80];
} month_drag;

static int count_roads_of_drag(void)
{
    int roads = 0;
    for (int i = 0; i < DRAG_LENGTH; i++) {
        roads += map_terrain_is(map_grid_offset(month_drag.x + i, month_drag.y), TERRAIN_ROAD) ? 1 : 0;
    }
    return roads;
}

static int copy_file(const char *from, const char *to)
{
    FILE *in = fopen(from, "rb");
    FILE *out = in ? fopen(to, "wb") : 0;
    int ok = in && out;
    char chunk[4096];
    size_t n;
    while (ok && (n = fread(chunk, 1, sizeof(chunk), in)) > 0) {
        ok = fwrite(chunk, 1, n, out) == n;
    }
    if (in) {
        fclose(in);
    }
    if (out) {
        fclose(out);
    }
    return ok;
}

// between two frames, as the mouse would: start dragging a road on clear land of the local city, away from the
// buildings of mpnode_play; when the month changed, copy the monthly saved game and release the drag
static void month_drag_frame(void)
{
    if (month_drag.state == 0) {
        int x0 = 0, y0 = 0, size = 0;
        mp_compose_city_area(mp_session_local_player_id(), MP_COMPOSE_CITY_GAP, &x0, &y0, &size);
        month_drag.state = -1;
        for (int y = y0 + 2; y < y0 + 19 && month_drag.state < 0; y++) {
            for (int x = x0 + 2; x + DRAG_LENGTH < x0 + size && month_drag.state < 0; x++) {
                int clear = 1;
                for (int i = 0; i < DRAG_LENGTH && clear; i++) {
                    clear = map_grid_is_inside(x + i, y, 1) &&
                        !map_terrain_is(map_grid_offset(x + i, y), TERRAIN_NOT_CLEAR);
                }
                if (!clear) {
                    continue;
                }
                month_drag.x = x;
                month_drag.y = y;
                building_construction_set_type(BUILDING_ROAD);
                building_construction_start(x, y, map_grid_offset(x, y));
                building_construction_update(x + DRAG_LENGTH - 1, y, map_grid_offset(x + DRAG_LENGTH - 1, y));
                if (building_construction_in_progress() && count_roads_of_drag() == DRAG_LENGTH) {
                    month_drag.state = 1;
                    month_drag.month = game_time_month();
                    printf("dragging a road at %d,%d in month %d\n", x, y, month_drag.month);
                } else {
                    building_construction_cancel();
                    building_construction_clear_type();
                }
            }
        }
    } else if (month_drag.state == 1 && game_time_month() == month_drag.month && !building_construction_in_progress()) {
        // the preview could not come back after the ticks of a frame (a building of the game waiting in the undo
        // state blocks the start of any construction): the player presses the mouse again
        int x = month_drag.x, y = month_drag.y;
        building_construction_set_type(BUILDING_ROAD);
        building_construction_start(x, y, map_grid_offset(x, y));
        building_construction_update(x + DRAG_LENGTH - 1, y, map_grid_offset(x + DRAG_LENGTH - 1, y));
    } else if (month_drag.state == 1 && game_time_month() != month_drag.month) {
        int still_dragging = building_construction_in_progress() && count_roads_of_drag() == DRAG_LENGTH;
        int copied = copy_file(month_drag.autosave, month_drag.copy);
        printf("month %d started while dragging: %s (in progress %d, %d road tiles), monthly save %s\n",
            game_time_month(), still_dragging ? "still dragging" : "DRAG LOST", building_construction_in_progress(),
            count_roads_of_drag(), copied ? "copied" : "NOT WRITTEN");
        building_construction_cancel();
        building_construction_clear_type();
        month_drag.month = game_time_month();
        month_drag.state = still_dragging && copied ? 2 : -1;
    }
}

// the monthly saved game written while the road was dragged: the month that started, without the road
static int month_drag_check(void)
{
    if (month_drag.state != 2) {
        printf("WRONG: no road was dragged while a month started\n");
        return 0;
    }
    int ok = mp_savegame_read(month_drag.copy);
    int roads = ok ? count_roads_of_drag() : -1;
    printf("monthly save written while dragging: month %d, %d road tiles of the drag\n", ok ? game_time_month() : -1,
        roads);
    remove(month_drag.copy);
    if (!ok || game_time_month() != month_drag.month || roads != 0) {
        printf("WRONG: the monthly saved game holds the road being dragged, or is not that of the month\n");
        return 0;
    }
    return 1;
}

static int command_mpnode(int argc, char **argv)
{
    // argv: mpnode host PORT PLAYERS SAVE TICKS [cities] | mpnode join ADDRESS PORT TICKS
    int is_host = argc >= 7 && strcmp(argv[2], "host") == 0;
    int cities = 0, generate = 0, lobby_rules = 0, map2 = 0, save_resume = 0, drag_month = 0, war = 0, from_autosave = 0;
    for (int i = 6; i < argc; i++) {
        war |= strcmp(argv[i], "war") == 0; // 'war' (T5.5): see mpnode_play_war
        cities |= i >= 7 && strcmp(argv[i], "cities") == 0;
        generate |= i >= 7 && strcmp(argv[i], "generate") == 0;
        lobby_rules |= i >= 7 && strcmp(argv[i], "rules") == 0;
        map2 |= i >= 7 && strcmp(argv[i], "map2") == 0; // 'map2' (T4.14): the host plays the generated map on map 2
        // 'saveresume' (T5.4): at the end, every player saves the game as the File menu does; the host then hosts
        // that saved game as the lobby does, in the same process, and the clients join it again
        save_resume |= strcmp(argv[i], "saveresume") == 0;
        // 'dragmonth' (with 'saveresume', host): the host drags a road while a month starts (T5.3 review)
        drag_month |= strcmp(argv[i], "dragmonth") == 0;
        // 'autosaveresume' (with 'saveresume', host): the host resumes its monthly saved game (the one the desync
        // status names) instead of the one of the File menu (T5.3 review)
        from_autosave |= strcmp(argv[i], "autosaveresume") == 0;
    }
    int is_join = argc >= 6 && strcmp(argv[2], "join") == 0;
    if (!is_host && !is_join) {
        return usage();
    }
    // 'difficulty-easy' / 'difficulty-hard' (T5.1, T5.2): the lobby sets this difficulty on a host whose own setting is
    // the other end of the scale and on clients whose setting is "normal"; the template of the host ('blank') is a free
    // map with a rank and funds of its own. Every city of every machine must start with the values of the game rules
    int expected_difficulty = -1;
    for (int i = 6; i < argc; i++) {
        if (strcmp(argv[i], "difficulty-easy") == 0) {
            expected_difficulty = DIFFICULTY_EASY;
        } else if (strcmp(argv[i], "difficulty-hard") == 0) {
            expected_difficulty = DIFFICULTY_HARD;
        }
    }
    int map_template = 0; // 'map-template': the saved game becomes a free map (rank 5, funds 3000) for the host
    for (int i = 6; i < argc; i++) {
        map_template |= strcmp(argv[i], "map-template") == 0;
    }
    char template_file[64] = "";
    if (map_template && is_host) {
        snprintf(template_file, sizeof(template_file), "mp-template-%s.map", argv[3]);
        if (!game_file_load_saved_game(argv[5])) {
            printf("FAILED: cannot load %s\n", argv[5]);
            return 1;
        }
        scenario_editor_set_player_rank(5);
        scenario_editor_set_initial_funds(3000);
        // the empire of a free map is one of the empires of the game, which must trade by land and by sea
        for (int empire = 0; empire < 40; empire++) {
            int land, sea;
            empire_load(1, scenario_empire_id());
            empire_init_scenario();
            count_trade_cities(&land, &sea);
            if (land > 0 && sea > 0) {
                break;
            }
            scenario_editor_change_empire(1);
        }
        if (!game_file_editor_write_scenario(template_file)) {
            printf("FAILED: cannot write the template\n");
            return 1;
        }
        argv[5] = template_file;
    }
    if (expected_difficulty >= 0) {
        set_local_difficulty(is_host ? (expected_difficulty == DIFFICULTY_EASY ? DIFFICULTY_VERY_HARD :
            DIFFICULTY_VERY_EASY) : DIFFICULTY_NORMAL);
    }
    int ticks = atoi(is_host ? argv[6] : argv[5]);
    war |= is_join && argc >= 7 && strcmp(argv[6], "war") == 0;
    mpnode_war_ticks = ticks;
    int soldiers_at_start[2] = { -1, -1 };
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
    int lobby_players_in_lobby = 0; // the number of players of the host, as the lobby of a client showed it (T4.11)
    if (lobby_rules && is_host) {
        mp_lobby_rules_init();
        mp_lobby_rules_settings(&rules_at_host);
        mp_lockstep_set_rules(&rules_at_host);
    }
    if ((map2 || expected_difficulty >= 0) && is_host) {
        game_rules_settings rules;
        game_rules_default_multiplayer_settings(&rules);
        if (map2) {
            rules.prepared_map = GAME_MAP_2;
        }
        if (expected_difficulty >= 0) {
            rules.difficulty = expected_difficulty;
        }
        mp_lockstep_set_rules(&rules);
    }
    // the monthly saved game of this process, apart from those of the other tests running in this folder
    snprintf(month_drag.autosave, sizeof(month_drag.autosave), "lan-autosave-%s-%d.mpsav", is_host ? argv[3] : argv[4],
        (int) getpid());
    snprintf(month_drag.copy, sizeof(month_drag.copy), "%s.drag", month_drag.autosave);
    mp_lockstep_test_set_autosave_name(month_drag.autosave);
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
    int session = 0;
    int resumed_ok = 1;
next_session:;
    time_t deadline = time(0) + 120 + ticks / 25; // long games (the war of T5.5) on a loaded computer
    int start_tick = -1;
    int start_tax = 0;
    int start_cities_wrong = 0;
    int last_played = -1;
    while (time(0) < deadline) {
        mp_lockstep_state state = mp_lockstep_get_state();
        if (state == MP_LOCKSTEP_DESYNC || state == MP_LOCKSTEP_DISCONNECTED) {
            break;
        }
        if (expect_reject && (mp_lockstep_status_key() == TR_MP_STATUS_REFUSED_DATA ||
            mp_lockstep_status_key() == TR_MP_STATUS_REFUSED_VERSION)) {
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
        if (!lobby_players_in_lobby && state == MP_LOCKSTEP_WAITING_FOR_PLAYERS) {
            lobby_players_in_lobby = mp_lockstep_lobby_num_players();
        }
        if (state == MP_LOCKSTEP_RUNNING && start_tick < 0) {
            start_tick = mp_lockstep_base_tick();
            if (is_host && save_resume) {
                printf("game %d starts at tick %d\n", session + 1, start_tick);
            }
            mp_lockstep_set_tick_limit(ticks);
            start_tax = city_finance_tax_percentage();
            if (expected_difficulty >= 0 && !check_start_cities(expected_difficulty, map_template)) {
                start_cities_wrong = 1;
            }
            if (war && player_context_num_players() > 1) {
                soldiers_at_start[0] = mpnode_soldiers_of(0);
                soldiers_at_start[1] = mpnode_soldiers_of(1);
            }
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
                if (save_resume) {
                    mpnode_trade(t);
                }
                if (war) {
                    mpnode_play_war(t);
                }
                if (cheat && t == ticks / 2) {
                    city_finance_change_tax_percentage(3); // changed on this computer only
                }
            }
            last_played = tick_in_game;
        }
        clock_millis += 2;
        time_set_millis(clock_millis);
        game_run();
        if (drag_month && save_resume && is_host && session == 0 && start_tick >= 0 &&
            game_time_absolute_tick() - start_tick >= 8) {
            month_drag_frame();
        }
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
        // T5.3 review: the status names the game to resume only when this game wrote it (from its start on)
        if (detected) {
            int with_save = mp_lockstep_status_key() == TR_MP_STATUS_DESYNC;
            printf("desync status: %s\n", mp_lockstep_status());
            if (with_save != mp_savegame_is_needed() || (with_save && (access(month_drag.autosave, F_OK) != 0 ||
                !strstr(mp_lockstep_status(), month_drag.autosave)))) {
                printf("WRONG: the desync status does not name the saved game of this game\n");
                detected = 0;
            }
        }
        if (is_host) {
            usleep(500 * 1000); // let the clients read the notice before the connection closes
        }
        mp_lockstep_stop();
        return detected ? 0 : 1;
    }
    if (bad_data) {
        printf("status: %s\n", mp_lockstep_status());
        return mp_lockstep_status_key() == TR_MP_STATUS_REFUSED_BY_HOST_DATA ||
            mp_lockstep_status_key() == TR_MP_STATUS_REFUSED_BY_HOST_VERSION ? 0 : 1;
    }
    if (leaver) {
        printf("left the game at tick %d\n", game_time_absolute_tick() - start_tick);
        mp_lockstep_stop();
        return 0;
    }
    printf("tick %d checksum %016" PRIx64 "%s\n", start_tick >= 0 ? game_time_absolute_tick() - start_tick : -1,
        mp_checksum_state(), session ? " resumed" : "");
    // the prepared map of the game, the same for every player (T4.14)
    printf("map %d, %d tiles wide\n", game_rules_multiplayer_settings()->prepared_map + 1, map_grid_width());
    printf("pause seen: %d, ticks run while paused: %d\n", paused_seen, ticks_while_paused);
    int result = mp_lockstep_get_state() == MP_LOCKSTEP_RUNNING && start_tick >= 0 &&
        game_time_absolute_tick() - start_tick == ticks && !start_cities_wrong;
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
            // a resumed game starts with the taxes of the saved game, different in every city
            int expected = session > 0 || (p % 2 ? tax > start_tax : tax < start_tax);
            printf("city %d: tax %d%% (start %d%%), treasury %d%s\n", p, tax, start_tax, city_finance_treasury(),
                expected ? "" : " UNEXPECTED");
            result = result && expected;
        }
        player_context_switch(local);
    }
    if (is_host) {
        result = result && mp_lockstep_last_verified_turn() == (ticks / 4) - 1;
    }
    if (war) {
        // the same on every computer (lan_test.sh compares these lines): peace signed after the war, soldiers lost
        int lost0 = soldiers_at_start[0] - mpnode_soldiers_of(0);
        int lost1 = soldiers_at_start[1] - mpnode_soldiers_of(1);
        printf("war: status %d, soldiers lost by player 1: %d, by player 2: %d\n", mp_war_status(0, 1), lost0, lost1);
        printf("war announcements: %d\n", mp_war_announcements());
        if (mp_war_status(0, 1) != MP_WAR_PEACE || mp_war_announcements() < 5 || lost0 + lost1 <= 0) {
            printf("WRONG: the war did not go as scripted (a war with losses, then peace)\n");
            result = 0;
        }
    }
    if (lobby_rules) {
        const game_rules_settings *game = game_rules_multiplayer_settings();
        print_lobby_rules("game rules", game);
        if (is_host) {
            print_lobby_rules("rules when hosting", &rules_at_host);
            print_lobby_rules("rules at the start", &rules_changed);
            printf("lobby players: %d\n", mp_lockstep_lobby_num_players());
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
            // a client who joined by address (as here) sees the number of players of the host, which came with the
            // rules, while he was still in the lobby; lan_test.sh compares it with the number the host asked for
            printf("lobby players: %d\n", lobby_players_in_lobby);
            if (lobby_players_in_lobby != mp_lockstep_lobby_num_players()) {
                printf("WRONG: the number of players of the host changed after the lobby\n");
                result = 0;
            }
        }
    }
    // a pause stops every computer within the turns already issued (2 turns of 4 ticks)
    if (ticks_while_paused > 8) {
        printf("WRONG: %d ticks ran during the pause\n", ticks_while_paused);
        result = 0;
    }
    if (save_resume && session == 0 && result) {
        // T5.4: every player saves as the File menu does (a .sav name, written as .mpsav), the host hosts its saved
        // game as the lobby does, the players who played join again; a player who did not play the first game
        // (lan_test.sh) joins with a new process
        char name[64];
        snprintf(name, sizeof(name), "lan-save-%s-p%d.sav", is_host ? argv[3] : argv[4], mp_session_local_player_id());
        int saved = game_file_write_saved_game(name);
        snprintf(name, sizeof(name), "lan-save-%s-p%d.mpsav", is_host ? argv[3] : argv[4], mp_session_local_player_id());
        printf("session 1 saved in %s: %s\n", name, saved ? "yes" : "NO");
        if (from_autosave && is_host) {
            // the host resumes the monthly saved game of its game: the file that host_start_game reads, then writes
            // again (the game goes on from the month start, not from the end)
            snprintf(name, sizeof(name), "%s", month_drag.autosave);
            saved = saved && access(name, F_OK) == 0;
            printf("resuming the monthly saved game %s: %s\n", name, saved ? "yes" : "NO");
        }
        print_trade_state("saved");
        int was_host = is_host;
        if (!was_host) {
            usleep(1500 * 1000); // the host still verifies the last turns
        }
        mp_lockstep_stop();
        if (!saved) {
            return 1;
        }
        if (was_host && drag_month && !month_drag_check()) {
            return 1;
        }
        if (was_host) {
            mp_lockstep_set_started_callback(0);
            ok = mp_lockstep_host(atoi(argv[3]), atoi(argv[4]), name, 1);
            if (ok) {
                mp_lockstep_set_manual_start(1);
                mp_lockstep_set_generated_map(0, 7);
            }
            // the lobby starts the game when every player is there
            while (ok && mp_lockstep_get_state() == MP_LOCKSTEP_WAITING_FOR_PLAYERS && time(0) < deadline + 60) {
                mp_lockstep_poll();
                if (mp_lockstep_connected_players() == atoi(argv[4])) {
                    mp_lobby_start_game();
                }
                usleep(1000);
            }
        } else {
            usleep(1000 * 1000); // the host hosts again
            ok = mp_lockstep_join(argv[3], atoi(argv[4]));
        }
        if (!ok) {
            printf("FAILED to resume: %s\n", mp_lockstep_status());
            return 1;
        }
        session = 1;
        goto next_session;
    }
    if (player_context_num_players() > 1) {
        print_trade_state("end");
    }
    if (save_resume) {
        resumed_ok = session == 1;
        printf("resumed game: %s\n", resumed_ok ? "played to its end" : "NOT RESUMED");
    }
    mp_lockstep_stop();
    return result && resumed_ok ? 0 : 1;
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
    if (lobby.rome_takes_province || defaults.rome_takes_province) {
        printf("WRONG: a second punitive expedition must strike like the first one by default (D-053)\n");
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
    // the status is a translated text (T4.11); the tests, which play in French, compare the key and the text
    if (mp_lockstep_status_key() != TR_MP_STATUS_ALL_HERE || strcmp(mp_lockstep_status(), "Tous les joueurs sont là")) {
        printf("WRONG: status %d \"%s\" of a game hosted alone\n", mp_lockstep_status_key(), mp_lockstep_status());
        result = 1;
    }
    // the number of players of the game, which the lobby of the host shows read-only (T4.11)
    if (mp_lockstep_lobby_num_players() != 1) {
        printf("WRONG: %d players in the lobby of a game hosted alone\n", mp_lockstep_lobby_num_players());
        result = 1;
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
    if (mp_lockstep_status_key() != TR_MP_STATUS_STARTED || strcmp(mp_lockstep_status(), "Partie lancée")) {
        printf("WRONG: status %d \"%s\" after the start\n", mp_lockstep_status_key(), mp_lockstep_status());
        result = 1;
    }
    mp_lockstep_stop();
    char name[64];
    snprintf(name, sizeof(name), "mp-session-%d-p0.mpsav", port);
    remove(name);
    // two players awaited: the status counts them, with their numbers inside the translated text (T4.11)
    player_context_switch(0);
    player_context_set_num_players(1);
    mp_lockstep_set_rules(&lobby);
    if (!mp_lockstep_host(port, 2, file, 1)) {
        printf("FAILED: %s\n", mp_lockstep_status());
        return 1;
    }
    char waiting[80];
    snprintf(waiting, sizeof(waiting), "En attente de 1 joueur(s) sur le port %d", port);
    if (mp_lockstep_status_key() != TR_MP_STATUS_WAITING_PLAYERS || strcmp(mp_lockstep_status(), waiting) ||
        mp_lockstep_lobby_num_players() != 2) {
        printf("WRONG: status %d \"%s\", %d players, for a game of 2 players hosted alone\n",
            mp_lockstep_status_key(), mp_lockstep_status(), mp_lockstep_lobby_num_players());
        result = 1;
    }
    mp_lockstep_stop();
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
    // (seeds 1 and 2 draw map 2, seed 7 draws map 1: the host follows the draw both ways)
    const unsigned int seeds[] = { 1, 2, 7 };
    CHECK(mp_mapgen_choose_prepared_map(GAME_MAP_RANDOM, 7) == 0, "seed 7 draws map 1");
    for (int s = 0; s < 3; s++) {
        unsigned int seed = seeds[s];
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

// K-review-fixes: a host sends the lobby of a player some rules; NUMBERS are the ten integers of the message, then
// the prepared map, the second expedition of Caesar (M10.6) and the number of players of the host (T4.11)
static int send_raw_rules(int socket, const int *numbers, int rome_takes_province, int num_players)
{
    uint8_t payload[4 + 1 + 13 * 4];
    buffer buf;
    buffer_init(&buf, payload, sizeof(payload));
    buffer_write_i32(&buf, 1 + 13 * 4);
    buffer_write_u8(&buf, 10); // MSG_RULES of mp/lockstep.c
    for (int i = 0; i < 10; i++) {
        buffer_write_i32(&buf, numbers[i]);
    }
    buffer_write_i32(&buf, GAME_MAP_1); // the prepared map (T4.14)
    buffer_write_i32(&buf, rome_takes_province);
    buffer_write_i32(&buf, num_players);
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
        send_raw_rules(host, bad[i], 0, 3);
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
    // the second expedition of Caesar is a yes or a no (M10.6)
    send_raw_rules(host, good, 2, 3);
    poll_lockstep_for(100);
    if (mp_lockstep_lobby_rules() && mp_lockstep_lobby_rules()->difficulty == DIFFICULTY_VERY_HARD) {
        printf("WRONG: a rule of the second expedition out of its bounds reaches the lobby of the player\n");
        result = 1;
    }
    // a number of players out of 1 to 4 spoils the message (T4.11): nothing of it reaches the lobby
    const int bad_players[] = { 0, 5, -2, 1000000 };
    for (int i = 0; i < (int) (sizeof(bad_players) / sizeof(bad_players[0])); i++) {
        send_raw_rules(host, good, 0, bad_players[i]);
        poll_lockstep_for(100);
        if (mp_lockstep_lobby_num_players() != 0 ||
            (mp_lockstep_lobby_rules() && mp_lockstep_lobby_rules()->difficulty == DIFFICULTY_VERY_HARD)) {
            printf("WRONG: %d players of the host reach the lobby of the player\n", bad_players[i]);
            result = 1;
        }
    }
    send_raw_rules(host, good, 1, 3);
    poll_lockstep_for(100);
    if (mp_lockstep_lobby_num_players() != 3) {
        printf("WRONG: the lobby of the player shows %d players, the host sent 3\n", mp_lockstep_lobby_num_players());
        result = 1;
    }
    const game_rules_settings *seen = mp_lockstep_lobby_rules();
    if (!seen) {
        printf("WRONG: valid rules do not reach the lobby of the player\n");
        result = 1;
    } else {
        print_lobby_rules("valid rules seen", seen);
        if (seen->difficulty != DIFFICULTY_VERY_HARD || seen->gods_enabled != 0 ||
            seen->end_condition != GAME_END_CAESAR || seen->caesar_score != 1500 || seen->score_years != 5 ||
            seen->rome_takes_province != 1) {
            printf("WRONG: valid rules are not shown as sent\n");
            result = 1;
        }
    }
    // welcome messages with rules out of their bounds, then with a player index or a number of players out of
    // theirs: the player leaves before reading the game
    const int welcome_rules[13] = {
        GAME_MODE_MULTIPLAYER, DIFFICULTY_EASY, 1, 0, 0, 1, 0, 10, 1, 1, 1000, GAME_MAP_1, 0
    };
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
        for (int i = 0; i < 13; i++) {
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
            mp_lockstep_status_key() != TR_MP_STATUS_HOST_MESSAGE_INVALID) {
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
    // the score the lobby shows: a game saved before the score has 0, which means the default score (D-073)
    int shown_score = game_rules_settings_caesar_score(mp_lobby_rules_shown());
    int expected_score = saved.caesar_score > 0 ? saved.caesar_score : MP_CAESAR_DEFAULT_SCORE;
    printf("score shown by the lobby: %d (expected %d)\n", shown_score, expected_score);
    if (shown_score != expected_score) {
        printf("WRONG: the lobby does not show the score of the saved game\n");
        result = 1;
    }
    change_every_lobby_rule();
    mp_lobby_start_game();
    for (int frame = 0; frame < 100 && mp_lockstep_get_state() == MP_LOCKSTEP_WAITING_FOR_PLAYERS; frame++) {
        mp_lockstep_poll();
    }
    const game_rules_settings *game = game_rules_multiplayer_settings();
    if (game_rules_caesar_score() != expected_score) {
        printf("WRONG: the resumed game plays for %d laurels, not %d\n", game_rules_caesar_score(), expected_score);
        result = 1;
    }
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

// the aqueducts of Caesar must not follow a road closely (T5.8): along a row (or a column) of aqueduct, a road in the
// same column (or row) nearer than `gap` tiles leaves no room to build between them. An aqueduct that crosses a road
// is not beside it: only the tiles that are not on the road count, and a road has to run in the direction of their
// run. Returns the number of aqueduct tiles beside a road, the nearest distance found in `closest`
#define AQUEDUCT_ROAD_GAP 6 // the maps leave 8 (mapgen.c)
static int count_aqueduct_beside_road(int size, int gap, int *closest, int *aqueduct_tiles)
{
    int beside = 0;
    *closest = size;
    *aqueduct_tiles = 0;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int offset = map_grid_offset(x, y);
            if (!map_terrain_is(offset, TERRAIN_AQUEDUCT)) {
                continue;
            }
            (*aqueduct_tiles)++;
            if (map_terrain_is(offset, TERRAIN_ROAD)) {
                continue; // the crossing
            }
            int along_row = (x > 0 && map_terrain_is(map_grid_offset(x - 1, y), TERRAIN_AQUEDUCT)) ||
                (x < size - 1 && map_terrain_is(map_grid_offset(x + 1, y), TERRAIN_AQUEDUCT));
            int along_column = (y > 0 && map_terrain_is(map_grid_offset(x, y - 1), TERRAIN_AQUEDUCT)) ||
                (y < size - 1 && map_terrain_is(map_grid_offset(x, y + 1), TERRAIN_AQUEDUCT));
            for (int d = 1; d < size; d++) {
                int road = 0;
                for (int sign = -1; sign <= 1; sign += 2) {
                    int yy = y + sign * d, xx = x + sign * d;
                    road |= along_row && yy >= 0 && yy < size && map_terrain_is(map_grid_offset(x, yy), TERRAIN_ROAD);
                    road |= along_column && xx >= 0 && xx < size &&
                        map_terrain_is(map_grid_offset(xx, y), TERRAIN_ROAD);
                }
                if (road) {
                    *closest = d < *closest ? d : *closest;
                    beside += d < gap;
                    break;
                }
            }
        }
    }
    return beside;
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
// the rows of the houses, of the start mission and of the reservoirs that the test builds, up to the aqueduct of
// Caesar (rows cy - 8 to cy - 4)
static int find_farm_place(int player_id, int *fx, int *fy)
{
    int cx, cy;
    mp_mapgen_city_center(player_id, &cx, &cy);
    for (int d = 4; d <= 22; d++) {
        for (int y = cy - d; y <= cy + d; y++) {
            for (int x = cx - d; x <= cx + d; x++) {
                int dx = x > cx ? x - cx : cx - x, dy = y > cy ? y - cy : cy - y;
                if ((dx > dy ? dx : dy) != d || (y + 2 >= cy - 10 && y <= cy + 3)) {
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
    // the aqueduct of Caesar keeps its distance from the roads, room to build along them (T5.8)
    int closest, aqueduct_tiles;
    int beside = count_aqueduct_beside_road(size, AQUEDUCT_ROAD_GAP, &closest, &aqueduct_tiles);
    printf("aqueduct of Caesar: %d tiles, nearest road along it %d tiles away, %d tiles within %d of a road\n",
        aqueduct_tiles, closest, beside, AQUEDUCT_ROAD_GAP);
    failures += beside != 0 || aqueduct_tiles == 0;
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
    char map_file[64]; // one file per test and map: ctest runs them in parallel
    snprintf(map_file, sizeof(map_file), "prepared-%d-%d.mpmap", options.prepared_map + 1, num_players);
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

// troops of the table of figures (MG.1, D-058): the javelin throwers are stronger in a multiplayer game only
static int command_troops(void)
{
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-70s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    const figure_properties *javelin = figure_properties_for_type(FIGURE_FORT_JAVELIN);
    CHECK(javelin->max_damage == 80 && javelin->attack_value == 4 && javelin->missile_attack_value == 4 &&
        javelin->missile_delay == 100, "classic: javelin throwers 80 hit points, attack 4, missile 4 every 100");
    game_rules_settings rules;
    game_rules_default_multiplayer_settings(&rules);
    game_rules_set_multiplayer(&rules);
    javelin = figure_properties_for_type(FIGURE_FORT_JAVELIN);
    CHECK(javelin->max_damage == 136 && javelin->attack_value == 10, "multiplayer: javelin throwers 136 hit points, attack 10");
    CHECK(javelin->missile_attack_value == 4 && javelin->missile_delay == 100 && javelin->defense_value == 0 &&
        javelin->missile_defense_value == 0 && javelin->category == FIGURE_CATEGORY_ARMED,
        "multiplayer: missile (4 damage, every 100), defences and category unchanged");
    const figure_properties *legionary = figure_properties_for_type(FIGURE_FORT_LEGIONARY);
    const figure_properties *mounted = figure_properties_for_type(FIGURE_FORT_MOUNTED);
    CHECK(legionary->max_damage == 150 && legionary->attack_value == 10, "multiplayer: legionaries 150 x 10 unchanged");
    CHECK(mounted->max_damage == 120 && mounted->attack_value == 8, "multiplayer: mounted 120 x 8 unchanged");
    CHECK(figure_properties_for_type(FIGURE_ENEMY_CAESAR_JAVELIN)->max_damage == 90 &&
        figure_properties_for_type(FIGURE_ENEMY_CAESAR_JAVELIN)->attack_value == 4, "multiplayer: enemy javelin throwers unchanged");
    game_rules_set_classic();
    javelin = figure_properties_for_type(FIGURE_FORT_JAVELIN);
    CHECK(javelin->max_damage == 80 && javelin->attack_value == 4, "back to classic: the table of the original");
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: the troops are not as designed" : "Identical: the troops are as designed");
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
    snprintf(map_file, sizeof(map_file), "drying-%d-%d.mpmap", options.prepared_map + 1, num_players);
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
    // the aqueduct comes from the east along a row: a reservoir at its end, the cut a few tiles upstream (the row of the aqueduct is 8 rows north of the main road, the fountain and the house stay north of it)
    int cut_x = ax + 5;
    int cut = map_grid_offset(cut_x, ay);
    if (!map_terrain_is(cut, TERRAIN_AQUEDUCT) || map_owner_get_claimed(cut) != MAP_OWNER_CAESAR) {
        printf("No aqueduct of Caesar at (%d, %d)\n", cut_x, ay);
        return 2;
    }
    mp_command reservoir = { .type = MP_COMMAND_BUILD, .player_id = p, .args = { BUILDING_DRAGGABLE_RESERVOIR, 0, ax - 2, ay, ax - 2, ay, 0, 0 } };
    mp_command_execute(&reservoir);
    int ranged = map_grid_offset(ax - 2, ay + 7); // a fountain there would have water from the reservoir
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
    snprintf(map_file, sizeof(map_file), "inlandwater-%d-%d-%d.mpmap", options.prepared_map + 1, num_players, p);
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
    built &= build_as(p, BUILDING_FOUNTAIN, ax - 2, ay + 5, ax - 2, ay + 5);
    built &= build_as(p, BUILDING_HOUSE_VACANT_LOT, ax - 2, ay + 7, ax - 2, ay + 7);
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
            building_get(map_building_at(map_grid_offset(ax - 2, ay + 5)))->type,
            building_get(map_building_at(map_grid_offset(ax - 2, ay + 7)))->type);
        return -1;
    }
    setting_reset_speeds(500, setting_scroll_speed());
    int reservoir_id = map_building_at(map_grid_offset(ax - 2, ay));
    int fountain_id = map_building_at(map_grid_offset(ax - 2, ay + 5));
    int house_id = map_building_at(map_grid_offset(ax - 2, ay + 7));
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

// T5.9: Rome is one for every player. Its wages change by a random event of the month; in a multiplayer game they
// change for every city at once, by the same amount and as often as with fewer players, so that no player has to
// pay more than the others to keep his people happy (Alexandre: the players who were not the host had riots, Rome
// paying them more). Returns the month the cities first differ (0: never), the wages of each month in `wages`.
#define ROME_MONTHS (12 * 12)
static int rome_wages_game(const char *file, int num_players, int *wages)
{
    player_context_switch(0);
    player_context_set_num_players(1);
    // seed 0 keeps the luck of the template, as every game had it before T5.9: Rome raised its wages in the cities of
    // players 2 to 4 at month 16, not in the first one
    if (!create_prepared(file, num_players, 0)) {
        printf("Unable to create the prepared map\n");
        return -1;
    }
    game_rules_settings rules;
    game_rules_default_multiplayer_settings(&rules);
    rules.territories = 1;
    rules.difficulty = DIFFICULTY_EASY;
    game_rules_set_multiplayer(&rules);
    setting_reset_speeds(500, setting_scroll_speed());
    int different_month = 0;
    for (int month = 0; month < ROME_MONTHS; month++) {
        for (int tick = 0; tick < 800; tick++) {
            run_one_tick();
        }
        player_context_switch(0);
        wages[month] = city_data.labor.wages_rome;
        for (int p = 1; p < num_players; p++) {
            player_context_switch(p);
            if (city_data.labor.wages_rome != wages[month] && !different_month) {
                printf("month %d: Rome pays %d Dn in the city of player 1 and %d in the city of player %d\n",
                    month + 1, wages[month], city_data.labor.wages_rome, p + 1);
                different_month = month + 1;
            }
        }
        player_context_switch(0);
    }
    player_context_switch(0);
    player_context_set_num_players(1);
    return different_month;
}

static int command_romewages(const char *file, int num_players)
{
    // the reference: the first city of a game with one player fewer on the same map (the map for 4 also holds 3
    // players; alone, one plays on the map for 2), whose wage events are those of its own draws
    static int alone[ROME_MONTHS], together[ROME_MONTHS];
    int reference_players = num_players - 1;
    if (rome_wages_game(file, reference_players, alone) < 0) {
        return 2;
    }
    int different = rome_wages_game(file, num_players, together);
    if (different < 0) {
        return 2;
    }
    int changes = 0, as_alone = 1;
    for (int month = 0; month < ROME_MONTHS; month++) {
        if (month && together[month] != together[month - 1]) {
            printf("month %d: Rome now pays %d Dn in every city\n", month + 1, together[month]);
            changes++;
        }
        if (together[month] != alone[month] && as_alone) {
            printf("month %d: Rome pays %d Dn with %d players, %d with %d\n", month + 1, together[month], num_players,
                alone[month], reference_players);
            as_alone = 0;
        }
    }
    if (different) {
        printf("DIFFERENT: Rome does not pay the same wages to every player\n");
        return 1;
    }
    if (!as_alone) {
        printf("DIFFERENT: Rome changes its wages more often with more players\n");
        return 1;
    }
    if (!changes) {
        printf("FAILED: Rome never changed its wages in %d months, the test proves nothing\n", ROME_MONTHS);
        return 1;
    }
    printf("Rome pays the same wages to the %d players, as with %d: %d changes in %d months\n", num_players,
        reference_players, changes, ROME_MONTHS);
    return 0;
}

// the luck of a city: its next random numbers, as the simulation will read them
static void city_luck(int player_id, int32_t *luck)
{
    player_context_switch(player_id);
    luck[0] = random_short();
    for (int i = 1; i < 8; i++) {
        luck[i] = random_from_pool(i);
    }
    player_context_switch(0);
}

// T5.9: each city has its own luck, drawn from the seed of the lobby: no two cities share their random numbers, the
// same seed gives the same luck on every computer and after a save, another seed another luck
static int command_cityluck(const char *file)
{
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-66s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    int32_t luck[2][4][8];
    for (int run = 0; run < 2; run++) {
        player_context_switch(0);
        player_context_set_num_players(1);
        if (!create_prepared(file, 4, run ? 8 : 7)) {
            printf("Unable to create the prepared map\n");
            return 2;
        }
        for (int p = 0; p < 4; p++) {
            city_luck(p, luck[run][p]);
        }
    }
    int shared = 0;
    for (int p = 0; p < 4; p++) {
        for (int q = 0; q < p; q++) {
            shared |= memcmp(luck[0][p], luck[0][q], sizeof(luck[0][p])) == 0;
        }
    }
    CHECK(!shared, "no two cities share their random numbers");
    int same_as_other_seed = 0;
    for (int p = 0; p < 4; p++) {
        same_as_other_seed |= memcmp(luck[0][p], luck[1][p], sizeof(luck[0][p])) == 0;
    }
    CHECK(!same_as_other_seed, "another seed of the lobby gives every city another luck");
    // the same seed again, then written and read back as the players receive the game
    player_context_switch(0);
    player_context_set_num_players(1);
    int32_t again[4][8];
    int same = create_prepared(file, 4, 7);
    for (int p = 0; p < 4 && same; p++) {
        city_luck(p, again[p]);
        same &= memcmp(again[p], luck[0][p], sizeof(again[p])) == 0;
    }
    CHECK(same, "the same seed gives the same luck (every computer)");
    const char *map_file = "cityluck.mpmap";
    int saved = mp_savegame_write(map_file) && mp_savegame_read(map_file);
    for (int p = 0; p < 4 && saved; p++) {
        city_luck(p, again[p]);
        saved &= memcmp(again[p], luck[0][p], sizeof(again[p])) == 0;
    }
    remove(map_file);
    CHECK(saved, "the saved game keeps the luck of every city");
    player_context_switch(0);
    player_context_set_num_players(1);
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: the cities do not each have their own luck" : "every city has its own luck");
    return failures ? 1 : 0;
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

// the land of Caesar around his bridge (M10.2, D-082): no zone, no building that needs one, no mission, no wall;
// roads stay allowed
static int caesar_land_count(void)
{
    int count = 0;
    for (int y = 0; y < map_grid_height(); y++) {
        for (int x = 0; x < map_grid_width(); x++) {
            count += mp_territory_is_caesar_land(map_grid_offset(x, y));
        }
    }
    return count;
}

static int caesar_land_expected(int bx, int ye_north, int ye_south)
{
    int count = 0;
    for (int y = 0; y < map_grid_height(); y++) {
        for (int x = 0; x < map_grid_width(); x++) {
            int near_x = abs(x - bx) <= MP_TERRITORY_CAESAR_LAND_RADIUS;
            count += near_x && (abs(y - ye_north) <= MP_TERRITORY_CAESAR_LAND_RADIUS ||
                abs(y - ye_south) <= MP_TERRITORY_CAESAR_LAND_RADIUS);
        }
    }
    return count;
}

static int walls_on_caesar_land(void)
{
    int count = 0;
    for (int y = 0; y < map_grid_height(); y++) {
        for (int x = 0; x < map_grid_width(); x++) {
            int o = map_grid_offset(x, y);
            count += mp_territory_is_caesar_land(o) && map_terrain_is(o, TERRAIN_WALL);
        }
    }
    return count;
}

static int has_terrain_at(int x, int y, int mask)
{
    return map_terrain_is(map_grid_offset(x, y), mask) ? 1 : 0;
}

// one end of the bridge: ye is the row of the shore tile next to it, dir is -1 on the north shore, +1 on the south
static int caesar_land_end(int bx, int ye, int dir)
{
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("  %-70s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    int r = MP_TERRITORY_CAESAR_LAND_RADIUS;
    CHECK(mp_territory_is_caesar_land(map_grid_offset(bx, ye)), "the end of the bridge is Caesar's land");
    CHECK(mp_territory_is_caesar_land(map_grid_offset(bx + r, ye + dir * r)) &&
        mp_territory_is_caesar_land(map_grid_offset(bx - r, ye + dir * r)), "15 tiles away in x and y, still");
    CHECK(!mp_territory_is_caesar_land(map_grid_offset(bx + r + 1, ye)) &&
        !mp_territory_is_caesar_land(map_grid_offset(bx, ye + dir * (r + 1))), "16 tiles away, no longer");
    // clear the ground of the tests, except the water and what Caesar built
    for (int y = ye - 2 * dir; y != ye + dir * 40; y += dir) {
        for (int x = bx - 30; x <= bx + 30; x++) {
            int o = map_grid_offset(x, y);
            if (!map_terrain_is(o, TERRAIN_WATER | TERRAIN_ROAD | TERRAIN_AQUEDUCT | TERRAIN_BUILDING)) {
                map_terrain_set(o, 0);
            }
        }
    }
    map_routing_update_land();
    // the first rows beyond the land where nothing of Caesar's lies across (a cross road, a reservoir)
    int distance = 17;
    for (; distance < 24; distance++) {
        int free = 1;
        for (int k = -3; k <= 5 && free; k++) {
            for (int x = bx + 2; x <= bx + 22 && free; x++) {
                free = !map_terrain_is(map_grid_offset(x, ye + dir * (distance + k)),
                    TERRAIN_WATER | TERRAIN_ROAD | TERRAIN_AQUEDUCT | TERRAIN_BUILDING);
            }
        }
        if (free) {
            break;
        }
    }
    int inside_y = ye + dir * 8, outside_y = ye + dir * distance, mission_y = ye + dir * (distance + 4);
    int ix = bx + 6;
    // the zone of player 1 comes from a mission just outside, as the start mission of a city does
    for (int yy = mission_y; yy < mission_y + 2; yy++) {
        for (int xx = ix; xx < ix + 2; xx++) {
            map_terrain_set(map_grid_offset(xx, yy), 0);
        }
    }
    building *mission = building_create(BUILDING_MISSION_POST, ix, mission_y);
    mission->state = BUILDING_STATE_IN_USE;
    map_building_tiles_add(mission->id, ix, mission_y, mission->size, image_group(GROUP_BUILDING_MISSION_POST),
        TERRAIN_BUILDING);
    int land_before = caesar_land_count();
    mp_territory_update_city();
    CHECK(caesar_land_count() == land_before, "the zone of the mission does not take any of it");
    CHECK(mp_territory_owner(map_grid_offset(ix, outside_y)) == 0, "the zone covers the land outside, near the mission");
    CHECK(mp_territory_owner(map_grid_offset(ix, inside_y)) == -1 &&
        mp_territory_owner(map_grid_offset(ix, ye + dir * (r + 1))) == 0, "and stops where Caesar's land begins");

    // the buildings that need a zone: refused inside, allowed outside, by the command
    CHECK(has_terrain_at(bx, inside_y, TERRAIN_ROAD) && has_terrain_at(bx, outside_y, TERRAIN_ROAD),
        "the road of Caesar runs through the land and beyond");
    CHECK(!build_as(0, BUILDING_PREFECTURE, ix, inside_y, ix, inside_y), "a prefecture is refused inside");
    CHECK(build_as(0, BUILDING_PREFECTURE, ix, outside_y, ix, outside_y), "and allowed just outside, in the zone");
    CHECK(!build_as(0, BUILDING_GATEHOUSE, bx, inside_y, bx, inside_y), "a gatehouse on the road of Caesar is refused inside");
    int gate_outside = build_as(0, BUILDING_GATEHOUSE, bx, outside_y, bx, outside_y);
    printf("  (outside the land of Caesar, in the zone of the player, a gatehouse on the road of Caesar: %s)\n",
        gate_outside ? "ALLOWED" : "refused");
    // a tower needs walls around it: they are put there by hand, so that only the zone says no
    for (int row = 0; row < 2; row++) {
        int y0 = row ? outside_y : inside_y;
        for (int yy = y0 - 2; yy <= y0 + 3; yy++) {
            for (int xx = ix + 2; xx <= ix + 7; xx++) {
                map_terrain_add(map_grid_offset(xx, yy), TERRAIN_WALL);
            }
        }
    }
    CHECK(!build_as(0, BUILDING_TOWER, ix + 4, inside_y, ix + 4, inside_y), "a tower is refused inside, walls around it");
    CHECK(build_as(0, BUILDING_TOWER, ix + 4, outside_y, ix + 4, outside_y), "and allowed just outside");
    // the mission needs no zone but a missionary: one stands next to the places
    mp_missionary_create(0, ix, inside_y);
    mp_missionary_create(0, ix, ye + dir * (r + 2));
    CHECK(!mp_mission_allows_place(ix + 2, inside_y, 2), "a mission is refused inside, a missionary beside");
    CHECK(mp_mission_allows_place(ix + 2, ye + dir * (r + 3), 2), "and allowed outside, a missionary beside");
    CHECK(!mp_territory_allows_building(BUILDING_MISSION_POST, ix + 2, inside_y, 2), "the build rule says the same");
    // walls: refused inside, allowed outside; long walls across the land put no stone on it
    int walls_before = walls_on_caesar_land();
    int wall_x = bx + 2;
    build_as(0, BUILDING_WALL, wall_x, inside_y, wall_x + 3, inside_y);
    CHECK(!has_terrain_at(wall_x, inside_y, TERRAIN_WALL), "a wall inside is refused");
    build_as(0, BUILDING_WALL, wall_x + 12, outside_y, wall_x + 15, outside_y);
    CHECK(has_terrain_at(wall_x + 12, outside_y, TERRAIN_WALL) && has_terrain_at(wall_x + 15, outside_y, TERRAIN_WALL),
        "a wall just outside is allowed");
    build_as(0, BUILDING_WALL, bx + 2, ye + dir * 6, bx + 25, ye + dir * 6); // from inside to outside
    build_as(0, BUILDING_WALL, bx + 25, ye + dir * 10, bx + 2, ye + dir * 10); // and the other way round
    build_as(0, BUILDING_WALL, bx - 25, ye + dir * 4, bx + 25, ye + dir * 4);
    build_as(0, BUILDING_WALL, bx - 25, ye + dir * 12, bx + 25, ye + dir * 12);
    CHECK(walls_on_caesar_land() == walls_before, "walls from, to and across the land put no stone on it");
    // roads stay allowed
    build_as(0, BUILDING_ROAD, ix + 4, inside_y + dir * 4, ix + 9, inside_y + dir * 4);
    CHECK(has_terrain_at(ix + 6, inside_y + dir * 4, TERRAIN_ROAD), "but a road of the player is allowed inside");
#undef CHECK
    return failures;
}

static int command_caesarland(const char *file, int num_players)
{
    if (!create_prepared(file, num_players, 0)) {
        printf("Unable to create the prepared map\n");
        return 2;
    }
    game_rules_settings rules;
    game_rules_default_multiplayer_settings(&rules);
    rules.territories = 1;
    rules.ai_invasions = 0;
    game_rules_set_multiplayer(&rules);
    int bx, by_north, by_south;
    mp_mapgen_caesar_bridge(&bx, &by_north, &by_south);
    int ye_north = by_north - 1, ye_south = by_south + 1;
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-72s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    printf("map %d, %d player(s): bridge at column %d, from row %d to row %d\n", options.prepared_map + 1,
        num_players, bx, by_north, by_south);
    int expected = caesar_land_expected(bx, ye_north, ye_south);
    CHECK(caesar_land_count() == expected && expected > 0, "Caesar's land is the tiles within 15 of each end, no more");
    for (int p = 0; p < num_players; p++) {
        int cx, cy;
        mp_mapgen_city_center(p, &cx, &cy);
        CHECK(!mp_territory_is_caesar_land(map_grid_offset(cx, cy)), "no city starts on it");
    }
    printf("north shore:\n");
    failures += caesar_land_end(bx, ye_north, -1);
    printf("south shore:\n");
    failures += caesar_land_end(bx, ye_south, 1);

    // a saved and reloaded game keeps the land of Caesar
    char map_file[64]; // one file per test: ctest runs them in parallel
    snprintf(map_file, sizeof(map_file), "caesarland-%d-%d.mpsav", options.prepared_map + 1, num_players);
    int before = caesar_land_count();
    int saved = mp_savegame_write(map_file) && mp_savegame_read(map_file);
    remove(map_file);
    CHECK(saved, "the game is saved and read again");
    CHECK(caesar_land_count() == before && before == expected, "it keeps the land of Caesar");
    CHECK(!build_as(0, BUILDING_PREFECTURE, bx + 6, ye_north - 8, bx + 6, ye_north - 8),
        "and still refuses a prefecture there");
    player_context_switch(0);
    player_context_set_num_players(1);
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: the land of Caesar does not work as designed" :
        "Identical: the land of Caesar works as designed");
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

// T5.3: "windowsweep" proves in the real game that no window writes into the simulated state, but needs the game
// data. Here, without graphics: the religion advisor computes the least happy god of the city (the original stores it
// in the city each time it is drawn, while the simulation does it only at some month starts); with an angry god other
// than the stored one, drawing the advisor in a network game must not change the checksum of this computer, which
// would then differ from the others'
static int command_windowwrites(const char *file)
{
    if (!load(file)) {
        return 2;
    }
    game_rules_settings rules;
    game_rules_default_multiplayer_settings(&rules);
    rules.gods_enabled = 1;
    game_rules_set_multiplayer(&rules);
    mp_session_init_network(0, ignore_command);
    // an angry god other than the one the city holds for the least happy, as the automation "angrygod" does
    int stored = city_god_least_happy();
    int god = stored == GOD_MARS ? GOD_VENUS : GOD_MARS;
    for (int i = 0; i < MAX_GODS; i++) {
        city_data.religion.gods[i].wrath_bolts = i == god ? 30 : 0;
    }
    uint64_t before = mp_checksum_state();
    // what the religion advisor computes when it is drawn
    int shown = city_gods_least_happy_shown();
    uint64_t after = mp_checksum_state();
    printf("network game: angry god %d (held %d), advisor shows %d, checksum %016" PRIx64 " -> %016" PRIx64 "\n",
        god, stored, shown, before, after);
    int result = 1;
    if (shown != god) {
        printf("WRONG: the advisor does not show the angry god\n");
        result = 0;
    }
    if (before != after) {
        printf("WRONG: drawing the religion advisor changed the state of the simulation\n");
        result = 0;
    }
    // control: the same in a game offline (classic), where the original stores the god in the city; if the checksum
    // did not change here, the test above could not see a window writing
    mp_session_init_offline();
    shown = city_gods_least_happy_shown();
    uint64_t offline = mp_checksum_state();
    printf("offline game: advisor shows %d, checksum %016" PRIx64 "\n", shown, offline);
    if (shown != god || offline == after) {
        printf("WRONG: the test does not see the god stored in the city (no control)\n");
        result = 0;
    }
    return result ? 0 : 1;
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

    // his warehouse full of timber but for the space of his 2 loads of marble: the next caravan brings only the 2 he
    // has room for (T5.7), he pays for them, the rest stays with the seller
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
        "2 loads sent, stored and paid, the rest stays with the seller");

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

// ---------- trade between every player, in a game started as from the lobby (T5.7) ----------

#define TRADEGAME_MONTHS 14
#define TRADEGAME_FUNDS 100000
static const int TRADEGAME_GOODS[4] = { RESOURCE_MARBLE, RESOURCE_TIMBER, RESOURCE_IRON, RESOURCE_CLAY };

static const char *tradegame_name(int resource)
{
    return resource == RESOURCE_MARBLE ? "marble" : resource == RESOURCE_TIMBER ? "timber" :
        resource == RESOURCE_IRON ? "iron" : "clay";
}

static int is_caesar_road(int x, int y)
{
    if (!map_grid_is_inside(x, y, 1)) {
        return 0;
    }
    int o = map_grid_offset(x, y);
    return map_terrain_is(o, TERRAIN_ROAD) && map_owner_get_claimed(o) == MAP_OWNER_CAESAR;
}

static int next_to_caesar_road(const building *b)
{
    for (int y = b->y - 1; y <= b->y + b->size; y++) {
        for (int x = b->x - 1; x <= b->x + b->size; x++) {
            if (is_caesar_road(x, y)) {
                return 1;
            }
        }
    }
    return 0;
}

static int tiles_are_clear(int x, int y, int dx, int dy, int length)
{
    for (int i = 0; i < length; i++) {
        if (map_terrain_is(map_grid_offset(x + i * dx, y + i * dy), TERRAIN_NOT_CLEAR)) {
            return 0;
        }
    }
    return 1;
}

static int tradegame_warehouse_at(int player_id, int x, int y)
{
    player_context_switch(player_id);
    building *b = building_main(building_get(map_building_at(map_grid_offset(x, y))));
    int id = b->type == BUILDING_WAREHOUSE && !next_to_caesar_road(b) ? b->id : 0;
    player_context_switch(0);
    return id;
}

// As a player does: a road of his own leaving the main road of Caesar near his city, and a warehouse at its end, out
// of reach of the main road; returns the id of the warehouse. With `joined` 0, the road stops one tile short of the
// main road: the warehouse stands on a road network of its own. The roads leave the main road at least 12 tiles away
// from (avoid_x, avoid_y).
static int build_warehouse_away(int player_id, int joined, int avoid_x, int avoid_y)
{
    static const int DIRS[4][2] = { { 0, 1 }, { 0, -1 }, { 1, 0 }, { -1, 0 } };
    int cx, cy;
    mp_mapgen_city_center(player_id, &cx, &cy);
    for (int r = 0; r < 16; r++) {
        for (int y = cy - r; y <= cy + r; y++) {
            for (int x = cx - r; x <= cx + r; x++) {
                if ((abs(x - cx) != r && abs(y - cy) != r) || !is_caesar_road(x, y) ||
                    (abs(x - avoid_x) < 12 && abs(y - avoid_y) < 12)) {
                    continue;
                }
                for (int d = 0; d < 4; d++) {
                    int dx = DIRS[d][0], dy = DIRS[d][1];
                    int ex = x + 7 * dx, ey = y + 7 * dy;
                    if (!map_grid_is_inside(ex - 4, ey - 4, 9) || !tiles_are_clear(x + dx, y + dy, dx, dy, 7)) {
                        continue;
                    }
                    int first = joined ? 1 : 2;
                    build_as(player_id, BUILDING_ROAD, x + first * dx, y + first * dy, ex, ey);
                    // beside the last three tiles of the road, four tiles or more away from the main road
                    int wx = dx ? (dx > 0 ? ex - 2 : ex) : ex + 1;
                    int wy = dy ? (dy > 0 ? ey - 2 : ey) : ey + 1;
                    if (build_as(player_id, BUILDING_WAREHOUSE, wx, wy, wx, wy)) {
                        int id = tradegame_warehouse_at(player_id, wx, wy);
                        if (id) {
                            return id;
                        }
                    }
                }
            }
        }
    }
    return 0;
}

static int build_warehouse_off_main_road(int player_id, int joined)
{
    return build_warehouse_away(player_id, joined, -100, -100);
}

static int stock_in(int player_id, int warehouse_id, int resource)
{
    player_context_switch(player_id);
    int loads = building_warehouse_get_amount(building_get(warehouse_id), resource);
    player_context_switch(0);
    return loads;
}

static void set_stock(int player_id, int warehouse_id, int resource, int loads)
{
    player_context_switch(player_id);
    building *store = building_get(warehouse_id);
    int have = building_warehouse_get_amount(store, resource);
    if (have > loads) {
        building_warehouse_remove_resource(store, resource, have - loads);
    }
    for (int i = have; i < loads; i++) {
        building_warehouse_add_resource(store, resource);
    }
    player_context_switch(0);
}

static void set_funds(int player_id, int funds)
{
    player_context_switch(player_id);
    city_data.finance.treasury = funds;
    player_context_switch(0);
}

static int start_tradegame(const char *file, int num_players, unsigned int seed, const char *test)
{
    // the game of the lobby: the map written and read back as the players receive it, easy (D-063); a file of its own
    // for each test, map, number of players and draw (ctest runs them together)
    char map_file[100];
    snprintf(map_file, sizeof(map_file), "trade%s-map%d-%d-%u.mpsav", test, options.prepared_map + 1, num_players,
        seed);
    if (num_players < 2 || num_players > 4 || !create_prepared(file, num_players, seed) ||
        !mp_savegame_write(map_file) || !mp_savegame_read(map_file)) {
        printf("Unable to create the prepared map\n");
        return 0;
    }
    remove(map_file);
    game_rules_settings rules;
    game_rules_default_multiplayer_settings(&rules);
    rules.difficulty = DIFFICULTY_EASY;
    rules.ai_invasions = 0;
    game_rules_set_multiplayer(&rules);
    printf("prepared map %d for %d players, draw %u\n", options.prepared_map + 1, num_players, seed);
    return 1;
}

// Every player sells his own material to every other player over an open route, as the trade page asks for it: both
// propose the route, the buyer marks the material bought. Each player has three warehouses at the end of roads of his
// own: two receive (a far seller has up to seven months of caravans on the road, and none leaves without room for
// it), the third holds what he sells, given back every month; what he receives is counted and taken away every month. Every pair must trade, month after month, as the trade page says: a caravan every month for
// every resource bought (T5.7).
static int command_tradegame(const char *file, int num_players, unsigned int seed)
{
    if (!start_tradegame(file, num_players, seed, "game")) {
        return 2;
    }
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-70s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    int receives[4] = { 0 }, receives_too[4] = { 0 }, sells[4] = { 0 };
    int all_built = 1;
    for (int p = 0; p < num_players; p++) {
        receives[p] = build_warehouse_off_main_road(p, 1);
        receives_too[p] = build_warehouse_off_main_road(p, 1);
        sells[p] = build_warehouse_off_main_road(p, 1);
        all_built &= receives[p] && receives_too[p] && sells[p];
        set_funds(p, TRADEGAME_FUNDS);
    }
    CHECK(all_built, "every player has three warehouses off the main road");
    if (!all_built) {
        player_context_set_num_players(1);
        return 1;
    }
    for (int p = 0; p < num_players; p++) {
        for (int q = 0; q < num_players; q++) {
            if (p != q) {
                city_action(p, MP_ACTION_PROPOSE_ROUTE, q, 1, 0);
                city_action(p, MP_ACTION_SET_BUYS_FROM, q, TRADEGAME_GOODS[q], 1);
            }
        }
    }
    int delivered[4][4] = { { 0 } };
    int months_with_delivery[4][4] = { { 0 } };
    int first_month[4][4] = { { 0 } };
    int most_on_the_way = 0;
    for (int month = 0; month < TRADEGAME_MONTHS; month++) {
        for (int p = 0; p < num_players; p++) {
            set_stock(p, sells[p], TRADEGAME_GOODS[p], 8 * (num_players - 1));
        }
        run_trace(16 * 50, 16 * 50, 0, 0);
        printf("month %2d: caravans of each player on the road:", month + 1);
        for (int p = 0; p < num_players; p++) {
            int caravans = count_caravans(p);
            printf(" %d", caravans);
            most_on_the_way = caravans > most_on_the_way ? caravans : most_on_the_way;
        }
        printf("\n");
        for (int b = 0; b < num_players; b++) {
            for (int s = 0; s < num_players; s++) {
                if (b == s) {
                    continue;
                }
                int got = stock_in(b, receives[b], TRADEGAME_GOODS[s]) +
                    stock_in(b, receives_too[b], TRADEGAME_GOODS[s]) + stock_in(b, sells[b], TRADEGAME_GOODS[s]);
                if (got && !delivered[b][s]) {
                    first_month[b][s] = month + 1;
                }
                // the last six months: a delivery every month
                months_with_delivery[b][s] += got && month >= TRADEGAME_MONTHS - 6;
                delivered[b][s] += got;
                set_stock(b, receives[b], TRADEGAME_GOODS[s], 0);
                set_stock(b, receives_too[b], TRADEGAME_GOODS[s], 0);
                set_stock(b, sells[b], TRADEGAME_GOODS[s], 0);
            }
        }
    }
    int all_trade = 1, steady = 1;
    for (int b = 0; b < num_players; b++) {
        for (int s = 0; s < num_players; s++) {
            if (b != s) {
                printf("player %d bought %3d loads of %s from player %d (first in month %d, %d of the last 6 months)\n",
                    b + 1, delivered[b][s], tradegame_name(TRADEGAME_GOODS[s]), s + 1, first_month[b][s],
                    months_with_delivery[b][s]);
                all_trade &= delivered[b][s] > 0;
                steady &= months_with_delivery[b][s] == 6;
            }
        }
    }
    CHECK(all_trade, "every player bought from every other");
    CHECK(most_on_the_way > num_players - 1, "a seller has several caravans on the road to a far buyer");
    CHECK(steady, "a delivery every month once the first caravans arrived");
    player_context_switch(0);
    player_context_set_num_players(1);
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: the players do not all trade with each other, month after month" :
        "Identical: every player trades with every other, month after month");
    return failures ? 1 : 0;
}

// The game of tools/mp-trade3-test.sh (T5.7, T5.6): three players on map 1, started as from the lobby (easy), each
// with a warehouse at the end of a road of his own joined to the main road of Caesar; player 2 has 16 loads of marble
// and 16 of iron to sell, players 1 and 3 keep theirs. Written to OUT, which the real game hosts from the lobby: the
// players open their routes and buy by clicks, and the caravans of player 2 must leave and arrive in the real game.
static int command_tradesave(const char *file, const char *out)
{
    if (!start_tradegame(file, 3, 0, "save")) {
        return 2;
    }
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-70s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    int warehouses[3] = { 0 };
    for (int p = 0; p < 3; p++) {
        warehouses[p] = build_warehouse_off_main_road(p, 1);
    }
    CHECK(warehouses[0] && warehouses[1] && warehouses[2], "every player has a warehouse joined to the main road");
    run_trace(50, 50, 0, 0); // the warehouses come into use, their road networks are known
    if (warehouses[1]) {
        set_stock(1, warehouses[1], RESOURCE_MARBLE, 16);
        set_stock(1, warehouses[1], RESOURCE_IRON, 16);
    }
    // players 1 and 3 keep what they receive (they sell nothing below 40 loads), as the script buys in a ring: the
    // pages show the stock delivered instead of passing it on
    for (int p = 0; p < 3; p += 2) {
        city_action(p, MP_ACTION_CHANGE_EXPORT_OVER, RESOURCE_MARBLE, 40, 0);
        city_action(p, MP_ACTION_CHANGE_EXPORT_OVER, RESOURCE_IRON, 40, 0);
    }
    player_context_switch(2);
    int kept = city_resource_export_over(RESOURCE_IRON);
    player_context_switch(0);
    CHECK(stock_of(1, RESOURCE_MARBLE) == 16 && stock_of(1, RESOURCE_IRON) == 16 &&
        city_resource_export_over(RESOURCE_MARBLE) == 40 && kept == 40,
        "player 2 has 16 loads of marble and 16 of iron, players 1 and 3 keep 40");
    CHECK(mp_savegame_write(out), "the game is written");
    uint64_t written = mp_checksum_state();
    CHECK(mp_savegame_read(out) && mp_checksum_state() == written && stock_of(1, RESOURCE_MARBLE) == 16 &&
        player_context_num_players() == 3, "and reads back the same, for three players");
    player_context_switch(0);
    player_context_set_num_players(1);
#undef CHECK
    printf("%s\n", failures ? "FAILED: the game of mp-trade3-test.sh was not written" :
        "Written: the game of mp-trade3-test.sh");
    return failures ? 1 : 0;
}

// A buyer whose first warehouse stands on a road of its own, not joined to the main road (a quarter not yet joined,
// a harbour), and whose second one is joined to it: the caravans go to the second (T5.7)
static int command_tradewarehouse(const char *file, int num_players, unsigned int seed)
{
    if (!start_tradegame(file, num_players, seed, "warehouse")) {
        return 2;
    }
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-70s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    int seller = num_players - 1;
    int apart = build_warehouse_off_main_road(0, 0);
    player_context_switch(0);
    int apart_x = building_get(apart)->x, apart_y = building_get(apart)->y;
    int joined = build_warehouse_away(0, 1, apart_x, apart_y);
    int store = build_warehouse_off_main_road(seller, 1);
    CHECK(apart && joined && store && apart < joined, "the buyer's first warehouse is apart, his second joined");
    if (!apart || !joined || !store) {
        player_context_set_num_players(1);
        return 1;
    }
    run_trace(50, 50, 0, 0); // the warehouses come into use, their road networks are known
    player_context_switch(0);
    int apart_network = building_get(apart)->road_network_id, joined_network = building_get(joined)->road_network_id;
    player_context_switch(seller);
    int store_network = building_get(store)->road_network_id;
    player_context_switch(0);
    printf("road networks: warehouse apart %d, joined %d, seller's %d\n", apart_network, joined_network, store_network);
    CHECK(apart_network != store_network && joined_network == store_network,
        "only the second warehouse of the buyer is on the road network of the seller");
    set_stock(seller, store, RESOURCE_MARBLE, 16);
    city_action(0, MP_ACTION_PROPOSE_ROUTE, seller, 1, 0);
    city_action(seller, MP_ACTION_PROPOSE_ROUTE, 0, 1, 0);
    city_action(0, MP_ACTION_SET_BUYS_FROM, seller, RESOURCE_MARBLE, 1);
    int on_the_way = 0, days = 0;
    for (; days < 300 && stock_of(0, RESOURCE_MARBLE) < 8; days++) {
        run_trace(50, 50, 0, 0);
        int loads = mp_trade_loads_on_the_way(seller, 0, RESOURCE_MARBLE);
        on_the_way = loads > on_the_way ? loads : on_the_way;
    }
    printf("after %d days: %d loads were on the way at most, player 1 has %d marble\n", days, on_the_way,
        stock_of(0, RESOURCE_MARBLE));
    CHECK(on_the_way >= 8, "a caravan of 8 loads leaves for the joined warehouse");
    CHECK(stock_of(0, RESOURCE_MARBLE) >= 8, "it arrives");
    player_context_switch(0);
    player_context_set_num_players(1);
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: a warehouse apart stops the caravans" :
        "Identical: a warehouse apart does not stop the caravans");
    return failures ? 1 : 0;
}

// Why no caravan leaves, as the trade page of the buyer shows it next to what he buys (T5.7): the seller has none, the
// buyer has no warehouse, or none on a road of the seller, no room, no money, his stock limit; then a caravan leaves.
// Display only: the same game with and without anyone looking at it.
static int command_tradestatus(const char *file, int num_players, unsigned int seed)
{
    if (!start_tradegame(file, num_players, seed, "status")) {
        return 2;
    }
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-70s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
#define STATE() mp_trade_purchase_state(seller, 0, RESOURCE_MARBLE)
    int seller = num_players - 1;
    int store = build_warehouse_off_main_road(seller, 1);
    set_funds(0, TRADEGAME_FUNDS);
    city_action(0, MP_ACTION_PROPOSE_ROUTE, seller, 1, 0);
    city_action(seller, MP_ACTION_PROPOSE_ROUTE, 0, 1, 0);
    city_action(0, MP_ACTION_SET_BUYS_FROM, seller, RESOURCE_MARBLE, 1);
    CHECK(STATE() == MP_TRADE_PURCHASE_UNKNOWN, "before the first month: nothing known");
    run_trace(16 * 50, 16 * 50, 0, 0);
    CHECK(STATE() == MP_TRADE_PURCHASE_NO_STOCK, "the seller has no marble: no stock");
    CHECK(mp_trade_purchase_state(0, seller, RESOURCE_MARBLE) == MP_TRADE_PURCHASE_UNKNOWN,
        "nothing for what is not bought");

    set_stock(seller, store, RESOURCE_MARBLE, 16);
    run_trace(16 * 50, 16 * 50, 0, 0);
    CHECK(STATE() == MP_TRADE_PURCHASE_NO_WAREHOUSE, "the buyer has no warehouse: no warehouse");

    int apart = build_warehouse_off_main_road(0, 0);
    player_context_switch(0);
    int apart_x = building_get(apart)->x, apart_y = building_get(apart)->y;
    run_trace(16 * 50, 16 * 50, 0, 0);
    CHECK(STATE() == MP_TRADE_PURCHASE_NO_ROAD, "his only warehouse is on a road of its own: no road");

    int joined = build_warehouse_away(0, 1, apart_x, apart_y);
    set_funds(0, 0);
    run_trace(16 * 50, 16 * 50, 0, 0);
    CHECK(STATE() == MP_TRADE_PURCHASE_NO_MONEY, "a warehouse on the road of the seller, but no money: no money");

    set_funds(0, TRADEGAME_FUNDS);
    set_stock(0, joined, RESOURCE_MARBLE, 4);
    city_action(0, MP_ACTION_CHANGE_BUY_LIMIT, RESOURCE_MARBLE, 4, 0);
    run_trace(16 * 50, 16 * 50, 0, 0);
    CHECK(STATE() == MP_TRADE_PURCHASE_LIMIT, "4 loads in stock, bought up to 4: limit");

    city_action(0, MP_ACTION_CHANGE_BUY_LIMIT, RESOURCE_MARBLE, -4, 0);
    set_stock(0, apart, RESOURCE_TIMBER, 32);
    set_stock(0, joined, RESOURCE_TIMBER, 28);
    run_trace(16 * 50, 16 * 50, 0, 0);
    CHECK(STATE() == MP_TRADE_PURCHASE_NO_ROOM, "his warehouses full: full");
    CHECK(mp_trade_loads_on_the_way(seller, 0, RESOURCE_MARBLE) == 0 && stock_in(seller, store, RESOURCE_MARBLE) == 16,
        "no caravan left, the seller keeps his marble");

    set_stock(0, joined, RESOURCE_TIMBER, 0);
    run_trace(16 * 50, 16 * 50, 0, 0);
    CHECK(STATE() == MP_TRADE_PURCHASE_SENT && mp_trade_loads_on_the_way(seller, 0, RESOURCE_MARBLE) == 8,
        "room again: a caravan of 8 loads leaves");

    // the state is shown, never played: a game where nobody reads it goes the same
    uint64_t looked = mp_checksum_state();
    for (int i = 0; i < 100; i++) {
        mp_trade_purchase_state(seller, 0, RESOURCE_MARBLE);
    }
    CHECK(mp_checksum_state() == looked, "reading it changes nothing");
    player_context_switch(0);
    player_context_set_num_players(1);
#undef STATE
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: the trade page does not tell why no caravan leaves" :
        "Identical: the trade page tells why no caravan leaves");
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

// the multiplayer saved game without one of its pieces, as an older version wrote it
static int remove_saved_piece(const char *mpsav, const char *name)
{
    FILE *fp = fopen(mpsav, "rb");
    if (!fp) {
        return 0;
    }
    static uint8_t data[32 * 1024 * 1024];
    int size = (int) fread(data, 1, sizeof(data), fp);
    fclose(fp);
    int length = (int) strlen(name);
    int index = 8; // magic and version
    while (index + 5 <= size) {
        int name_length = data[index];
        int piece_size = data[index + 1 + name_length] | data[index + 2 + name_length] << 8 |
            data[index + 3 + name_length] << 16 | data[index + 4 + name_length] << 24;
        int end = index + 5 + name_length + piece_size;
        if (name_length == length && memcmp(&data[index + 1], name, length) == 0) {
            memmove(&data[index], &data[end], size - end);
            size -= end - index;
            fp = fopen(mpsav, "wb");
            int ok = fp && fwrite(data, 1, size, fp) == (size_t) size;
            if (fp) {
                fclose(fp);
            }
            return ok;
        }
        index = end;
    }
    return 0;
}

// the bounds of the trade bring the marble where they say: player 1 stops buying from player 2 at 5 loads, then, with
// no limit, gets what player 2 sells beyond the 8 loads he keeps
static int play_trade_bounds(int show)
{
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); if (show) printf("%-62s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    city_action(0, MP_ACTION_CHANGE_BUY_LIMIT, RESOURCE_MARBLE, 5, 0);
    CHECK(mp_trade_buy_limit(0, RESOURCE_MARBLE) == 5, "player 1 buys marble up to 5 loads");
    run_trace(100 * 50, 50, 0, 0);
    if (show) {
        printf("after 100 days: player 1 has %d marble, player 2 has %d\n", marble_of(0), marble_of(1));
    }
    CHECK(marble_of(0) == 5 && marble_of(1) == 11 && in_transit(RESOURCE_MARBLE) == 0,
        "he stops buying from player 2 at 5 loads");

    city_action(0, MP_ACTION_CHANGE_BUY_LIMIT, RESOURCE_MARBLE, -5, 0);
    city_action(1, MP_ACTION_CHANGE_EXPORT_OVER, RESOURCE_MARBLE, 8, 0);
    CHECK(mp_trade_buy_limit(0, RESOURCE_MARBLE) == 0, "no limit any more");
    run_trace(100 * 50, 50, 0, 0);
    if (show) {
        printf("100 days later: player 1 has %d marble, player 2 has %d\n", marble_of(0), marble_of(1));
    }
    CHECK(marble_of(0) == 8 && marble_of(1) == 8 && in_transit(RESOURCE_MARBLE) == 0,
        "player 2 sells only beyond the 8 loads he keeps");
#undef CHECK
    return failures;
}

// stock minimum and maximum of the trade (T4.5, D-070): a buyer stops buying from a player and from the empire when
// his stock reaches his limit, a seller keeps his threshold for himself; the bounds are commands, in the checksum,
// saved and loaded; an older game loads with no limit; a classic game has none
static int command_tradebounds(const char *file)
{
    if (!start_trade_game(file)) {
        return 2;
    }
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-62s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    CHECK(mp_trade_buy_limit(0, RESOURCE_MARBLE) == 0 && mp_trade_buy_limit(1, RESOURCE_MARBLE) == 0,
        "a new game: no limit");
    setup_trade(16, 0, 1);
    const char *start = "tradebounds.mpsav";
    if (!mp_savegame_write(start)) {
        printf("Unable to save\n");
        return 2;
    }
    uint64_t before = mp_checksum_state();
    city_action(0, MP_ACTION_CHANGE_BUY_LIMIT, RESOURCE_MARBLE, 1, 0);
    CHECK(mp_checksum_state() != before, "the limit is in the checksum");
    city_action(0, MP_ACTION_CHANGE_BUY_LIMIT, RESOURCE_MARBLE, -1, 0);
    CHECK(mp_checksum_state() == before, "(back to no limit)");
    city_action(0, MP_ACTION_CHANGE_BUY_LIMIT, RESOURCE_MARBLE, -1, 0);
    city_action(0, MP_ACTION_CHANGE_BUY_LIMIT, 0, 5, 0);
    city_action(0, MP_ACTION_CHANGE_BUY_LIMIT, RESOURCE_MAX, 5, 0);
    CHECK(mp_checksum_state() == before && mp_trade_buy_limit(0, RESOURCE_MARBLE) == 0,
        "no limit below zero, nor for what is not a resource");

    failures += play_trade_bounds(1);
    uint64_t first = mp_checksum_state();
    // the second machine plays the same commands from the same start
    if (!mp_savegame_read(start)) {
        printf("Unable to read the saved game\n");
        return 2;
    }
    CHECK(mp_trade_buy_limit(0, RESOURCE_MARBLE) == 0 && marble_of(1) == 16, "the start is back");
    play_trade_bounds(0);
    CHECK(mp_checksum_state() == first, "two machines playing the same commands end with the same checksum");

    // the empire: player 1 imports marble from a city of the empire
    int city_id = empire_city_selling(RESOURCE_MARBLE);
    CHECK(city_id >= 0, "a city of the empire sells marble");
    if (city_id < 0) {
        return 1;
    }
    city_action(0, MP_ACTION_OPEN_TRADE_ROUTE, city_id, 0, 0);
    for (int i = 0; i < 3 && city_resource_trade_status(RESOURCE_MARBLE) != TRADE_STATUS_IMPORT; i++) {
        city_action(0, MP_ACTION_CYCLE_TRADE_STATUS, RESOURCE_MARBLE, 0, 0);
    }
    player_context_switch(0);
    printf("player 1 has %d loads of marble, and imports it\n", city_resource_count(RESOURCE_MARBLE));
    CHECK(city_resource_count(RESOURCE_MARBLE) == 8 && empire_can_import_resource_from_city(city_id, RESOURCE_MARBLE),
        "no limit: the empire sells, under the 10 loads of the original");
    city_action(0, MP_ACTION_CHANGE_BUY_LIMIT, RESOURCE_MARBLE, 8, 0);
    CHECK(!empire_can_import_resource_from_city(city_id, RESOURCE_MARBLE),
        "at his limit of 8 loads, he buys nothing from the empire");
    city_action(0, MP_ACTION_CHANGE_BUY_LIMIT, RESOURCE_MARBLE, 12, 0);
    int x0, y0;
    mp_mapgen_city_center(0, &x0, &y0);
    building *store = building_get(map_building_at(map_grid_offset(x0 - 6, y0 + 1)));
    for (int i = 0; i < 4; i++) {
        building_warehouse_add_resource(store, RESOURCE_MARBLE);
    }
    CHECK(city_resource_count(RESOURCE_MARBLE) == 12 && empire_can_import_resource_from_city(city_id, RESOURCE_MARBLE),
        "his limit of 20 replaces the 10 loads of the original");
    for (int i = 0; i < 8; i++) {
        building_warehouse_add_resource(store, RESOURCE_MARBLE);
    }
    CHECK(city_resource_count(RESOURCE_MARBLE) == 20 && !empire_can_import_resource_from_city(city_id, RESOURCE_MARBLE),
        "at 20 loads, he buys nothing from the empire");

    // saved and loaded
    const char *saved = "tradebounds-saved.mpsav";
    if (!mp_savegame_write(saved) || !mp_savegame_read(saved)) {
        printf("Unable to write and read the game\n");
        return 2;
    }
    player_context_switch(1);
    int kept = city_resource_export_over(RESOURCE_MARBLE);
    player_context_switch(0);
    CHECK(mp_trade_buy_limit(0, RESOURCE_MARBLE) == 20 && kept == 8, "the bounds are saved and loaded");

    // a game saved before the bounds: it loads, with no limit
    CHECK(remove_saved_piece(saved, "mp_trade_bounds"), "(the bounds taken out of the saved game)");
    CHECK(mp_savegame_read(saved) && mp_trade_buy_limit(0, RESOURCE_MARBLE) == 0 && marble_of(0) == 20,
        "an older saved game loads, with no limit");
    // a caravan already on its way when the buyer lowers his limit: on arrival he takes only what stays under it, the
    // rest goes back to the seller, and no load is lost
    if (!mp_savegame_read(start)) {
        printf("Unable to read the saved game\n");
        return 2;
    }
    for (int i = 0; i < 400 && in_transit(RESOURCE_MARBLE) == 0; i++) {
        run_trace(10, 10, 0, 0);
    }
    int on_the_way = in_transit(RESOURCE_MARBLE);
    printf("a caravan carries %d loads of marble; player 1 then limits his stock to 3\n", on_the_way);
    CHECK(on_the_way > 3 && marble_of(0) == 0, "a caravan is on its way, the buyer has no marble yet");
    city_action(0, MP_ACTION_CHANGE_BUY_LIMIT, RESOURCE_MARBLE, 3, 0);
    for (int i = 0; i < 400 && in_transit(RESOURCE_MARBLE) > 0; i++) {
        run_trace(10, 10, 0, 0);
    }
    printf("after the arrival: player 1 has %d marble, player 2 has %d\n", marble_of(0), marble_of(1));
    CHECK(in_transit(RESOURCE_MARBLE) == 0 && marble_of(0) == 3,
        "on arrival he takes only what stays under his limit of 3");
    CHECK(marble_of(1) == 13 && marble_of(0) + marble_of(1) == 16, "the rest went back to the seller, nothing is lost");

    remove(saved);
    remove(start);

    // a classic game: no limit, the command does nothing
    player_context_switch(0);
    player_context_set_num_players(1);
    if (!load(file)) {
        return 2;
    }
    city_action(0, MP_ACTION_CHANGE_BUY_LIMIT, RESOURCE_MARBLE, 5, 0);
    CHECK(mp_trade_buy_limit(0, RESOURCE_MARBLE) == 0, "a classic game: no limit");
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: the bounds of the trade are not as designed" :
        "Identical: the bounds of the trade are as designed");
    return failures ? 1 : 0;
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
    CHECK(mp_caesar_esteem_goal(0) == 500 && !mp_caesar_esteem_goal_reached(0),
        "the pillar of the laurels has no capital below the score");
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
    CHECK(mp_caesar_esteem_goal_reached(0), "and its pillar of laurels has its capital");

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

    // Rome pays the salary of the rank (3 at 300 laurels), whatever the command asks (D-076), to him only
    int rank_of_1 = salary_rank_of(1);
    CHECK(mp_caesar_rank(0) == 3, "player 1 is of rank 3");
    city_action(0, MP_ACTION_SET_SALARY, 5, 0, 0);
    CHECK(salary_rank_of(0) == 3 && salary_amount_of(0) == 8, "a salary above the rank is not paid: the rank's, 8 denarii");
    city_action(0, MP_ACTION_SET_SALARY, 0, 0, 0);
    CHECK(salary_rank_of(0) == 3 && salary_amount_of(0) == 8, "nor a lower one: Rome pays the salary of the rank");
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

// state of an older version of the piece of Caesar: 2, before the gifts had a waiting time; 3, before the monthly
// history of the laurels
static void write_old_caesar_state(buffer *buf, int version)
{
    buffer_write_i32(buf, version);
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
    if (version >= 3) {
        for (int p = 0; p < PLAYER_CONTEXT_MAX_PLAYERS; p++) {
            buffer_write_i32(buf, p + 1); // waiting time of the gifts
        }
    }
}

// T5.2 (D-076): Rome pays every player the salary of his rank, from the same table; every city starts with the same
// rank, salary and savings; nobody chooses a salary. Three players, one of each laurels.
static int command_salaries(const char *file)
{
    static const int LAURELS_TENTHS[3] = { 0, 1000, 3000 }; // rank 0, 1 and 3
    static const int SALARY_OF_RANK[11] = { 0, 2, 5, 8, 12, 20, 30, 40, 60, 80, 100 };
    if (!create_prepared(file, 3, 0)) {
        printf("Unable to create the prepared map\n");
        return 2;
    }
    game_rules_settings rules;
    game_rules_default_multiplayer_settings(&rules);
    rules.ai_invasions = 0;
    game_rules_set_multiplayer(&rules);
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-62s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    int same_start = 1;
    for (int p = 0; p < 3; p++) {
        player_context_switch(p);
        same_start &= city_emperor_rank() == 0 && city_emperor_salary_rank() == 0 && city_emperor_salary_amount() == 0 &&
            city_emperor_personal_savings() == 0;
    }
    player_context_switch(0);
    CHECK(same_start, "the three cities start with no rank, no salary and no savings");
    for (int p = 0; p < 3; p++) {
        mp_caesar_add_laurels(p, MP_LAURELS_PROSPERITY, LAURELS_TENTHS[p]);
    }
    // nobody chooses: asking for the highest salary, or for none, changes nothing
    city_action(0, MP_ACTION_SET_SALARY, 10, 0, 0);
    city_action(1, MP_ACTION_SET_SALARY, 10, 0, 0);
    city_action(2, MP_ACTION_SET_SALARY, 0, 0, 0);
    CHECK(salary_rank_of(0) == 0 && salary_rank_of(1) == 1 && salary_rank_of(2) == 3,
        "at once, each salary is the one of the rank, whatever was asked");
    int treasury[3];
    for (int p = 0; p < 3; p++) {
        treasury[p] = treasury_of(p);
    }
    run_to_next_month_fast();
    int paid = 1, favor_untouched = 1;
    for (int p = 0; p < 3; p++) {
        int rank = mp_caesar_rank(p);
        paid &= rank == (p == 0 ? 0 : p == 1 ? 1 : 3) && salary_rank_of(p) == rank &&
            salary_amount_of(p) == SALARY_OF_RANK[rank] && savings_of(p) == SALARY_OF_RANK[rank];
        printf("player %d: rank %d, salary %d, savings %d\n", p + 1, rank, salary_amount_of(p), savings_of(p));
        player_context_switch(p);
        // the favor of the original game finds the salary right for the rank: no penalty
        favor_untouched &= city_emperor_rank() == rank && city_data.ratings.favor_salary_penalty == 0;
        player_context_switch(0);
    }
    CHECK(paid, "every player is paid the salary of his rank, 0, 2 and 8 denarii");
    CHECK(favor_untouched, "and the rank of the original follows, with no penalty of favor");
    // the salary comes out of the treasury of the one who gets it
    int taxes_only = 1;
    for (int p = 0; p < 3; p++) {
        taxes_only &= treasury_of(p) <= treasury[p];
    }
    CHECK(taxes_only, "the salary is taken from the treasury of each player");
    // one more month: the savings add up; a rank gained or lost changes the salary the month after
    mp_caesar_add_laurels(0, MP_LAURELS_PROSPERITY, 1000);
    mp_caesar_add_laurels(2, MP_LAURELS_PROSPERITY, -3000);
    run_to_next_month_fast();
    CHECK(salary_amount_of(0) == 2 && savings_of(0) == 2 && salary_amount_of(1) == 2 && savings_of(1) == 4 &&
        salary_amount_of(2) == 0 && savings_of(2) == 8, "a rank gained or lost changes the salary the month after");
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: Rome does not pay everybody the salary of his rank" :
        "Identical: Rome pays everybody the salary of his rank");
    return failures ? 1 : 0;
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
    write_old_caesar_state(&buf, 2);
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

// the monthly records of a city, the last one first
static int history_of(int player_id, int *records)
{
    int count = mp_caesar_history_months(player_id);
    for (int i = 0; i < count; i++) {
        records[i] = mp_caesar_history(player_id, i);
    }
    return count;
}

// The monthly history of the laurels of each city (T4.1, D-071), for the ratings advisor: what a city gained last
// month and its trend; recorded by each city at the end of its month, deterministic, saved and loaded, older saved
// games still load, nothing in a classic game
static int command_caesarhistory(const char *file)
{
    if (!start_trade_game(file)) {
        return 2;
    }
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-62s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    CHECK(mp_caesar_history_months(0) == 0 && mp_caesar_history_months(1) == 0 && mp_caesar_laurels_gained(0, 1) == 0 &&
        mp_caesar_laurels_trend(0) == MP_CAESAR_TREND_STEADY, "a new game: no history, no gain, no trend");

    // a month of the game: each city records its laurels at the end of its month
    run_to_next_month_fast();
    CHECK(mp_caesar_history_months(0) == 1 && mp_caesar_history_months(1) == 1 &&
        mp_caesar_history(0, 0) == mp_caesar_laurels(0) && mp_caesar_history(1, 0) == mp_caesar_laurels(1),
        "a month later, each city has recorded its laurels");
    CHECK(mp_caesar_laurels_gained(0, 1) == 0, "one record: nothing to compare yet");

    // the gain of a month: the notes, and what Caesar gives between two records
    mp_caesar_add_laurels(0, MP_LAURELS_GIFTS, 70);
    caesar_months(0, 1);
    int gain = mp_caesar_laurels_gained(0, 1);
    printf("gained last month: %d tenths\n", gain);
    CHECK(mp_caesar_history_months(0) == 2 && gain == mp_caesar_history(0, 0) - mp_caesar_history(0, 1) &&
        gain >= 70 && mp_caesar_history(0, 0) == mp_caesar_laurels(0), "the gain of last month counts a gift of 7");
    CHECK(mp_caesar_history_months(1) == 1, "the month of player 1 is not recorded for player 2");
    mp_caesar_add_laurels(0, MP_LAURELS_PUNISHMENT, -500);
    caesar_months(0, 1);
    CHECK(mp_caesar_laurels_gained(0, 1) < 0 && mp_caesar_laurels_gained(0, 2) == mp_caesar_history(0, 0) -
        mp_caesar_history(0, 2), "a month of losses: a negative gain");

    // the trend: the last three months against the three before
    caesar_months(0, 6);
    CHECK(mp_caesar_laurels_trend(0) == MP_CAESAR_TREND_STEADY, "the same gain month after month: steady");
    for (int i = 0; i < 3; i++) {
        mp_caesar_add_laurels(0, MP_LAURELS_GIFTS, 100);
        caesar_months(0, 1);
    }
    CHECK(mp_caesar_laurels_trend(0) == MP_CAESAR_TREND_UP, "10 laurels more each month: rising");
    caesar_months(0, 3);
    CHECK(mp_caesar_laurels_trend(0) == MP_CAESAR_TREND_DOWN, "then back to the notes alone: falling");
    caesar_months(0, 3);
    CHECK(mp_caesar_laurels_trend(0) == MP_CAESAR_TREND_STEADY, "and steady again three months later");

    // a short history: the last twelve months
    static int expected[64];
    int n = 0;
    for (int i = 0; i < 20; i++) {
        mp_caesar_add_laurels(0, MP_LAURELS_GIFTS, 10 * i);
        caesar_months(0, 1);
        expected[n++] = mp_caesar_laurels(0);
    }
    int same = mp_caesar_history_months(0) == MP_CAESAR_HISTORY_MONTHS;
    for (int i = 0; i < MP_CAESAR_HISTORY_MONTHS; i++) {
        same &= mp_caesar_history(0, i) == expected[n - 1 - i];
    }
    CHECK(same, "twelve records are kept, the last one first");
    CHECK(mp_caesar_laurels_gained(0, 30) == mp_caesar_history(0, 0) - mp_caesar_history(0, 11),
        "a gain over more months than recorded: over the history");
    CHECK(mp_caesar_history(0, MP_CAESAR_HISTORY_MONTHS) == 0 && mp_caesar_history(0, -1) == 0 &&
        mp_caesar_history(2, 0) == 0, "nothing beyond the history, nothing for a player not in the game");

    // saved and loaded, and the game goes on the same on two machines
    run_to_next_month_fast();
    static int records_0[MP_CAESAR_HISTORY_MONTHS], records_1[MP_CAESAR_HISTORY_MONTHS];
    int count_0 = history_of(0, records_0), count_1 = history_of(1, records_1);
    uint64_t saved = mp_checksum_state();
    const char *mpsav = "caesarhistory.mpsav";
    if (!mp_savegame_write(mpsav)) {
        printf("Unable to save\n");
        return 2;
    }
    caesar_months(0, 2);
    caesar_months(1, 5);
    if (!mp_savegame_read(mpsav)) {
        printf("Unable to read the saved game\n");
        return 2;
    }
    static int loaded_0[MP_CAESAR_HISTORY_MONTHS], loaded_1[MP_CAESAR_HISTORY_MONTHS];
    same = history_of(0, loaded_0) == count_0 && history_of(1, loaded_1) == count_1 && count_1 >= 2;
    for (int i = 0; same && i < count_0; i++) {
        same &= loaded_0[i] == records_0[i];
    }
    for (int i = 0; same && i < count_1; i++) {
        same &= loaded_1[i] == records_1[i];
    }
    CHECK(same, "the saved game brings the history back, for each city");
    CHECK(mp_checksum_state() == saved, "and the same checksum");
    run_to_next_month_fast();
    uint64_t first = mp_checksum_state();
    int first_gain = mp_caesar_laurels_gained(1, 1);
    if (!mp_savegame_read(mpsav)) {
        printf("Unable to read the saved game\n");
        return 2;
    }
    remove(mpsav);
    run_to_next_month_fast();
    CHECK(mp_checksum_state() == first && mp_caesar_laurels_gained(1, 1) == first_gain,
        "two machines from the same game: the same history, the same checksum");

    // the esteem of Caesar, what the player sees instead of the favor: the laurels against the score
    set_caesar_rules(GAME_END_CAESAR, 500);
    mp_caesar_add_laurels(1, MP_LAURELS_GIFTS, 1250 - mp_caesar_laurels(1));
    CHECK(mp_caesar_esteem_goal(1) == 500 && mp_caesar_esteem(1) == 25, "125 laurels of 500: an esteem of 25");
    mp_caesar_add_laurels(1, MP_LAURELS_GIFTS, 10000);
    CHECK(mp_caesar_esteem(1) == 100, "above the score: 100");
    mp_caesar_add_laurels(1, MP_LAURELS_PUNISHMENT, -20000);
    CHECK(mp_caesar_esteem(1) == 0, "below zero: 0");
    // without a score, against the next rank (a rank for each 100 laurels of the default score)
    set_caesar_rules(GAME_END_NONE, 0);
    mp_caesar_add_laurels(1, MP_LAURELS_GIFTS, 2500 - mp_caesar_laurels(1));
    CHECK(mp_caesar_rank(1) == 2 && mp_caesar_esteem_goal(1) == 300 && mp_caesar_esteem(1) == 83,
        "an endless game: 250 laurels against the next rank at 300, 83");
    mp_caesar_add_laurels(1, MP_LAURELS_GIFTS, 10000);
    CHECK(mp_caesar_esteem_goal(1) == 1000 && mp_caesar_esteem(1) == 100, "the last rank: 100");

    // a game saved before the history: it loads, without history, everything else kept
    static uint8_t bytes[2048];
    buffer buf;
    buffer_init(&buf, bytes, sizeof(bytes));
    write_old_caesar_state(&buf, 3);
    buffer_set(&buf, 0);
    mp_caesar_load_state(&buf);
    CHECK(mp_caesar_laurels(0) == 100 && mp_caesar_laurels(1) == 200 && mp_caesar_wrath() == 250 &&
        mp_caesar_gift_cooldown(1) == 2, "an older saved game still loads, with its laurels and its gifts");
    CHECK(mp_caesar_history_months(0) == 0 && mp_caesar_history_months(1) == 0, "and an empty history");
    caesar_months(0, 1);
    CHECK(mp_caesar_history_months(0) == 1 && mp_caesar_history(0, 0) == mp_caesar_laurels(0),
        "which starts at its next month");

    // a classic game: no history
    player_context_switch(0);
    player_context_set_num_players(1);
    if (!load(file)) {
        return 2;
    }
    run_to_next_month_fast();
    CHECK(!mp_caesar_is_active() && mp_caesar_history_months(0) == 0, "a classic game: no history");
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: the history of the laurels is not kept as designed" :
        "Identical: the history of the laurels is kept as designed");
    return failures ? 1 : 0;
}

// ---------- war between players (T5.5, D-077) ----------

// a fort of the player built by command, with soldiers given to its legion as the barracks would give them
static int make_legion(int player_id, int fort_type, int x, int y, int soldiers)
{
    if (!build_as(player_id, fort_type, x, y, x, y)) {
        return 0;
    }
    run_one_tick(); // the fort comes into use
    player_context_switch(player_id);
    building *fort = building_get(map_building_at(map_grid_offset(x, y)));
    int formation_id = fort->formation_id;
    formation *m = formation_get(formation_id);
    for (int i = 0; i < soldiers && formation_id; i++) {
        figure *f = figure_create(m->figure_type, m->x_home, m->y_home, DIR_0_TOP);
        f->formation_id = formation_id;
        f->formation_at_rest = 1;
        f->action_state = FIGURE_ACTION_81_SOLDIER_GOING_TO_FORT;
    }
    formation_calculate_figures();
    player_context_switch(0);
    return formation_id;
}

static int alive_soldiers(int formation_id)
{
    int count = 0;
    int owner = FORMATION_OWNER(formation_id);
    for (int i = owner * MAX_FIGURES + 1; i < (owner + 1) * MAX_FIGURES; i++) {
        figure *f = figure_get(i);
        count += f->state == FIGURE_STATE_ALIVE && f->formation_id == formation_id &&
            f->action_state != FIGURE_ACTION_149_CORPSE && figure_is_legion(f);
    }
    return count;
}

static int buildings_of(int player_id)
{
    int count = 0;
    for (int i = player_id * MAX_BUILDINGS + 1; i < (player_id + 1) * MAX_BUILDINGS; i++) {
        count += building_get(i)->state == BUILDING_STATE_IN_USE;
    }
    return count;
}

// ticks without the checksums of run_trace, which cost more than the ticks on the prepared map
static void war_run(int ticks)
{
    setting_reset_speeds(500, setting_scroll_speed());
    for (int i = 0; i < ticks; i++) {
        run_one_tick();
    }
}

static int soldiers_at_standard(int formation_id)
{
    int count = 0;
    int owner = FORMATION_OWNER(formation_id);
    for (int i = owner * MAX_FIGURES + 1; i < (owner + 1) * MAX_FIGURES; i++) {
        figure *f = figure_get(i);
        count += f->state == FIGURE_STATE_ALIVE && f->formation_id == formation_id && figure_is_legion(f) &&
            f->action_state == FIGURE_ACTION_84_SOLDIER_AT_STANDARD;
    }
    return count;
}

// runs until the condition holds, `step` ticks at a time, at most `max_ticks`; war_ticks: the ticks run
#define RUN_UNTIL(condition, step, max_ticks) do { \
        war_ticks = 0; \
        while (!(condition) && war_ticks < (max_ticks)) { war_run((step)); war_ticks += (step); } \
    } while (0)

typedef struct {
    int legion[2];
    int target_x, target_y;    // the standard of player 1 in the city of player 2, by his buildings
    int buildings_at_peace;
    int destroyed;             // buildings of player 2 pulled down during the war
    int dead_attackers, dead_defenders;
    int ticks_to_destroy, ticks_to_fight;
    int lost_after_peace;      // soldiers and buildings lost after peace
    int home_after_peace;
    uint64_t checksum;
} war_result;

// The war of the tests, from the same start on every run: the legion of player 1 goes to the city of player 2 in
// peace, then player 1 declares a brutal war; the legion of player 2 comes out; peace is signed by both
static int play_war(war_result *r, int show)
{
    int failures = 0;
    int war_ticks = 0;
#define CHECK(condition, text) do { int ok_ = (condition); if (show) printf("%-70s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    int soldiers1 = alive_soldiers(r->legion[0]), soldiers2 = alive_soldiers(r->legion[1]);
    int buildings2 = buildings_of(1);
    city_action(0, MP_ACTION_LEGION_MOVE, r->legion[0], r->target_x, r->target_y);
    RUN_UNTIL(soldiers_at_standard(r->legion[0]) >= soldiers1, 50, 6000);
    war_run(500);
    if (show) {
        printf("in peace: the legion of player 1 reached the city of player 2 in %d ticks, %d soldiers at the standard\n",
            war_ticks, soldiers_at_standard(r->legion[0]));
    }
    r->buildings_at_peace = buildings_of(1);
    CHECK(soldiers_at_standard(r->legion[0]) == soldiers1, "in peace, the legion of player 1 goes to the city of player 2");
    CHECK(buildings_of(1) == buildings2 && alive_soldiers(r->legion[0]) == soldiers1 &&
        alive_soldiers(r->legion[1]) == soldiers2, "it harms nothing there: no building down, nobody dead");

    uint64_t before = mp_checksum_state();
    int announcements = mp_war_announcements();
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_BRUTAL, 0);
    CHECK(mp_war_status(0, 1) == MP_WAR_FIGHTING && mp_war_status(1, 0) == MP_WAR_FIGHTING &&
        mp_war_declarer(0, 1) == 0 && mp_war_form(1, 0) == MP_WAR_BRUTAL,
        "a brutal war, declared by command: they fight at once");
    CHECK(mp_checksum_state() != before, "the war is in the checksum");
    CHECK(mp_war_announcements() == announcements + 1, "the declaration is announced");

    RUN_UNTIL(buildings_of(1) < r->buildings_at_peace, 10, 3000);
    r->ticks_to_destroy = war_ticks;
    war_run(500);
    r->destroyed = r->buildings_at_peace - buildings_of(1);
    if (show) {
        printf("at war: the first building of player 2 down after %d ticks, %d down 500 ticks later\n",
            r->ticks_to_destroy, r->destroyed);
    }
    CHECK(r->destroyed > 0, "at war, the soldiers of player 1 pull down the buildings of player 2");

    city_action(1, MP_ACTION_LEGION_MOVE, r->legion[1], r->target_x + 1, r->target_y);
    RUN_UNTIL(alive_soldiers(r->legion[0]) < soldiers1 || alive_soldiers(r->legion[1]) < soldiers2, 10, 4000);
    r->ticks_to_fight = war_ticks;
    war_run(800);
    r->dead_attackers = soldiers1 - alive_soldiers(r->legion[0]);
    r->dead_defenders = soldiers2 - alive_soldiers(r->legion[1]);
    if (show) {
        printf("the legion of player 2 comes out: first death after %d ticks; 800 ticks later, %d of %d attackers and "
            "%d of %d defenders dead\n", r->ticks_to_fight, r->dead_attackers, soldiers1, r->dead_defenders, soldiers2);
    }
    CHECK(r->dead_attackers > 0 && r->dead_defenders > 0, "the two legions fight: soldiers die on both sides");

    city_action(0, MP_ACTION_PROPOSE_PEACE, 1, 1, 0);
    CHECK(mp_war_status(0, 1) == MP_WAR_FIGHTING && mp_war_peace_proposed(0, 1),
        "peace proposed by player 1 only: the war goes on");
    city_action(1, MP_ACTION_PROPOSE_PEACE, 0, 1, 0);
    CHECK(mp_war_status(0, 1) == MP_WAR_PEACE && !mp_war_any_fighting(), "proposed by both: peace is signed");
    int soldiers_left1 = alive_soldiers(r->legion[0]), soldiers_left2 = alive_soldiers(r->legion[1]);
    int buildings_left = buildings_of(1);
    war_run(3000);
    r->lost_after_peace = (soldiers_left1 - alive_soldiers(r->legion[0])) +
        (soldiers_left2 - alive_soldiers(r->legion[1])) + (buildings_left - buildings_of(1));
    r->home_after_peace = formation_get(r->legion[0])->is_at_fort && formation_get(r->legion[1])->is_at_fort;
    if (show) {
        printf("after peace: %d soldiers or buildings lost, both legions back at their fort: %d\n", r->lost_after_peace,
            r->home_after_peace);
    }
    CHECK(r->lost_after_peace == 0, "peace stops the fight: nothing lost after it");
    CHECK(r->home_after_peace, "and the legions go back to their forts");
    r->checksum = mp_checksum_state();
#undef CHECK
    return failures;
}

static int start_war_game(const char *file, int *legion1, int *legion2, int *tx, int *ty)
{
    if (!start_trade_game(file)) {
        return 0;
    }
    int x0, y0, x1, y1;
    mp_mapgen_city_center(0, &x0, &y0);
    mp_mapgen_city_center(1, &x1, &y1);
    *legion1 = make_legion(0, BUILDING_FORT_LEGIONARIES, x0 - 10, y0 - 8, 16);
    *legion2 = make_legion(1, BUILDING_FORT_LEGIONARIES, x1 + 6, y1 + 4, 16);
    // what player 1 will attack: houses and a prefecture of player 2 north of his main road (the warehouse of the
    // trade is south of it)
    build_as(1, BUILDING_HOUSE_VACANT_LOT, x1 - 7, y1 - 3, x1 - 2, y1 - 3);
    build_as(1, BUILDING_PREFECTURE, x1 - 4, y1 - 5, x1 - 4, y1 - 5);
    *tx = x1 - 4;
    *ty = y1 - 4;
    war_run(400); // the soldiers reach their forts
    return *legion1 && *legion2;
}

// ---------- timber of the barracks (MG.2, D-058) ----------

// one recruit of the barracks of the player: the legion that got him (0 for none)
static int recruit_one(building *barracks, int javelin, int legionary, int mounted)
{
    int before[3] = { alive_soldiers(javelin), alive_soldiers(legionary), alive_soldiers(mounted) };
    building_barracks_create_soldier(barracks, barracks->x, barracks->y + 3);
    int after[3] = { alive_soldiers(javelin), alive_soldiers(legionary), alive_soldiers(mounted) };
    for (int i = 0; i < 3; i++) {
        if (after[i] == before[i] + 1) {
            return i + 1; // 1 javelin throwers, 2 legionaries, 3 mounted
        }
    }
    return 0;
}

static void force_workers(int player_id)
{
    player_context_switch(player_id);
    for (int i = BUILDING_FIRST; i < BUILDING_END; i++) {
        building *b = building_get(i);
        if (b->state == BUILDING_STATE_IN_USE && (b->type == BUILDING_WAREHOUSE || b->type == BUILDING_BARRACKS)) {
            b->num_workers = 20;
        }
    }
    player_context_switch(0);
}

// ticks with the barracks and the warehouses staffed: nobody lives in these cities, the labour of the month would
// take their workers away
static void staffed_run(int ticks)
{
    for (int i = 0; i < ticks; i++) {
        force_workers(0);
        run_one_tick();
    }
}

static int command_barrackstimber(const char *file)
{
    if (!start_trade_game(file)) {
        return 2;
    }
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-72s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    int cx, cy;
    mp_mapgen_city_center(0, &cx, &cy);
    int fx = cx - 10, fy = cy - 8;
    // the road from the entry point of the city to its centre: the walkers come from there
    const map_tile *entry = city_map_entry_point();
    build_as(0, BUILDING_ROAD, entry->x, entry->y, cx, cy);
    war_run(120); // two days: the counts of the buildings are made again
    CHECK(build_as(0, BUILDING_BARRACKS, cx - 3, cy + 1, cx - 3, cy + 1), "a barracks by the main road");
    CHECK(build_as(0, BUILDING_WAREHOUSE, cx - 6, cy + 1, cx - 6, cy + 1), "a warehouse by the main road");
    map_road_network_update_grid(); // the roads are joined again only once a month
    map_road_network_update_largest();
    staffed_run(300);
    player_context_switch(0);
    building *barracks = building_get(city_buildings_get_barracks());
    building *store = building_get(map_building_at(map_grid_offset(cx - 6, cy + 1)));
    CHECK(barracks->type == BUILDING_BARRACKS && barracks->road_network_id > 0 &&
        barracks->road_network_id == store->road_network_id, "the barracks and the warehouse are on the same roads");
    const char *start = "barracks-timber-start.mpsav";
    CHECK(mp_savegame_write(start), "(the city saved, without forts)");

    // ---- recruits: a load of timber per javelin thrower ----
    int javelin = make_legion(0, BUILDING_FORT_JAVELIN, fx, fy, 1);
    int legionary = make_legion(0, BUILDING_FORT_LEGIONARIES, cx - 10, cy + 8, 1);
    int mounted = 0;
    for (int i = 0; i < 6 && !mounted; i++) { // the first free place
        mounted = make_legion(0, BUILDING_FORT_MOUNTED, cx - 22 + 7 * i, cy - 8 - (i % 2) * 8, 1);
    }
    CHECK(javelin && legionary && mounted, "three forts: javelin throwers, legionaries, horsemen");
    war_run(300); // the soldiers reach their forts
    player_context_switch(0);
    barracks = building_get(city_buildings_get_barracks());
    barracks->num_workers = 20;
    CHECK(formation_get(javelin)->legion_recruit_type == LEGION_RECRUIT_JAVELIN &&
        formation_get(legionary)->legion_recruit_type == LEGION_RECRUIT_LEGIONARY &&
        formation_get(mounted)->legion_recruit_type == LEGION_RECRUIT_MOUNTED, "the three forts ask for recruits");
    building_barracks_reset_extra_state();
    barracks->loads_stored = 0;
    CHECK(recruit_one(barracks, javelin, legionary, mounted) == 3, "without timber and weapons: a horseman");
    CHECK(recruit_one(barracks, javelin, legionary, mounted) == 3, "again a horseman (free)");
    building_barracks_add_timber(barracks);
    CHECK(recruit_one(barracks, javelin, legionary, mounted) == 1 && building_barracks_timber_stored() == 0,
        "one load of timber: one javelin thrower, the load is used");
    CHECK(recruit_one(barracks, javelin, legionary, mounted) == 3, "then a horseman again: one javelin thrower per load");
    for (int i = 0; i < 3; i++) {
        building_barracks_add_timber(barracks);
    }
    int ok = 1;
    for (int i = 0; i < 3; i++) {
        ok &= recruit_one(barracks, javelin, legionary, mounted) == 1;
    }
    CHECK(ok && building_barracks_timber_stored() == 0 && recruit_one(barracks, javelin, legionary, mounted) == 3,
        "three loads of timber: three javelin throwers");
    barracks->loads_stored = 2;
    ok = recruit_one(barracks, javelin, legionary, mounted) == 2 && recruit_one(barracks, javelin, legionary, mounted) == 2;
    CHECK(ok && barracks->loads_stored == 0 && building_barracks_timber_stored() == 0,
        "two loads of weapons: two legionaries, no timber used");
    CHECK(recruit_one(barracks, javelin, legionary, mounted) == 3, "weapons used up: a horseman");
    barracks->loads_stored = 1;
    building_barracks_add_timber(barracks);
    ok = recruit_one(barracks, javelin, legionary, mounted) == 2 && recruit_one(barracks, javelin, legionary, mounted) == 1;
    CHECK(ok && barracks->loads_stored == 0 && building_barracks_timber_stored() == 0,
        "one load of each: a legionary then a javelin thrower");

    // ---- the decision to deliver timber ----
    map_point dst;
    int network = barracks->road_network_id;
    staffed_run(60); // the barracks is counted as active (it recruits as it likes meanwhile)
    building_barracks_reset_extra_state();
    barracks->loads_stored = 0;
    CHECK(building_get_barracks_for_weapon(RESOURCE_TIMBER, network, &dst) == barracks->id, "room for timber: it is delivered");
    CHECK(building_get_barracks_for_weapon(RESOURCE_WEAPONS, network, &dst) == barracks->id, "and the weapons still are");
    CHECK(building_get_barracks_for_weapon(RESOURCE_MARBLE, network, &dst) == 0, "other goods are not");
    for (int i = 0; i < 8; i++) {
        building_barracks_add_timber(barracks);
    }
    CHECK(building_barracks_timber_stored() == BARRACKS_TIMBER_MAX, "at most 5 loads of timber");
    CHECK(building_get_barracks_for_weapon(RESOURCE_TIMBER, network, &dst) == 0, "five loads: no more timber is delivered");

    // ---- saved with the game, in the checksum; an older game has no timber ----
    building_barracks_reset_extra_state();
    for (int i = 0; i < 3; i++) {
        building_barracks_add_timber(barracks);
    }
    uint64_t with_timber = mp_checksum_state();
    const char *saved = "barracks-timber.mpsav";
    CHECK(mp_savegame_write(saved) && mp_savegame_read(saved), "(the game saved and read back)");
    player_context_switch(0);
    CHECK(building_barracks_timber_stored() == 3 && mp_checksum_state() == with_timber,
        "the saved game keeps the timber of the barracks, and its checksum");
    building_barracks_add_timber(building_get(city_buildings_get_barracks()));
    CHECK(mp_checksum_state() != with_timber, "the timber is in the checksum");
    {
        uint8_t data[64];
        buffer buf;
        buffer_init(&buf, data, sizeof(data));
        building_barracks_save_extra_state(&buf);
        int size = buf.index;
        buffer_init(&buf, data, size - 4); // as written before the timber: the piece ends earlier
        building_barracks_load_extra_state(&buf);
        CHECK(building_barracks_timber_stored() == 0, "an older saved game loads with no timber");
        buffer_init(&buf, data, size);
        building_barracks_load_extra_state(&buf);
        CHECK(building_barracks_timber_stored() == 4, "(and the current format reads back)");
    }
    remove(saved);

    // ---- classic: javelin throwers are free, timber is not delivered ----
    game_rules_set_classic();
    barracks = building_get(city_buildings_get_barracks());
    building_barracks_reset_extra_state();
    CHECK(recruit_one(barracks, javelin, legionary, mounted) == 1, "classic: a javelin thrower without any timber");
    CHECK(building_get_barracks_for_weapon(RESOURCE_TIMBER, network, &dst) == 0, "classic: no timber is delivered");
    game_rules_settings rules;
    game_rules_default_multiplayer_settings(&rules);
    rules.ai_invasions = 0;
    game_rules_set_multiplayer(&rules);

    // ---- by cart, without javelin throwers: the timber stays in the warehouse ----
    CHECK(mp_savegame_read(start), "(back to the city without forts)");
    player_context_switch(0);
    make_legion(0, BUILDING_FORT_LEGIONARIES, cx - 10, cy + 8, 1);
    store = building_get(map_building_at(map_grid_offset(cx - 6, cy + 1)));
    for (int i = 0; i < 8; i++) {
        building_warehouse_add_resource(store, RESOURCE_TIMBER);
    }
    CHECK(!building_barracks_has_javelin_legion(), "the city has no javelin throwers");
    staffed_run(1500);
    CHECK(building_barracks_timber_stored() == 0 && stock_of(0, RESOURCE_TIMBER) == 8,
        "without javelin throwers: no timber goes to the barracks");

    // ---- by cart, with javelin throwers: every load becomes a javelin thrower, five at most waiting ----
    javelin = make_legion(0, BUILDING_FORT_JAVELIN, fx, fy, 1);
    CHECK(javelin && building_barracks_has_javelin_legion(), "a fort of javelin throwers is built");
    int most = 0, ticks = 0;
    for (; ticks < 4000 && (stock_of(0, RESOURCE_TIMBER) > 0 || building_barracks_timber_stored() > 0 ||
        alive_soldiers(javelin) < 9); ticks++) {
        staffed_run(1);
        most = building_barracks_timber_stored() > most ? building_barracks_timber_stored() : most;
    }
    printf("after %d ticks: %d javelin throwers (1 + 8 loads), %d loads in the warehouse, %d in the barracks, at most %d waiting\n",
        ticks, alive_soldiers(javelin), stock_of(0, RESOURCE_TIMBER), building_barracks_timber_stored(), most);
    CHECK(alive_soldiers(javelin) == 9 && stock_of(0, RESOURCE_TIMBER) == 0 && building_barracks_timber_stored() == 0,
        "with javelin throwers: the carts bring timber, one javelin thrower per load");
    CHECK(most > 0 && most <= BARRACKS_TIMBER_MAX, "and the barracks never keeps more than 5 loads");
    remove(start);
    player_context_switch(0);
    player_context_set_num_players(1);
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: the timber of the barracks is not as designed" :
        "Identical: the timber of the barracks is as designed");
    return failures ? 1 : 0;
}

// figures of a player in a fight with a figure of the other player
static int fighting_against(int player_id, int other)
{
    int count = 0;
    for (int i = player_id * MAX_FIGURES + 1; i < (player_id + 1) * MAX_FIGURES; i++) {
        figure *f = figure_get(i);
        if (f->state == FIGURE_STATE_ALIVE && f->action_state == FIGURE_ACTION_150_ATTACK &&
            ((f->opponent_id > 0 && FIGURE_OWNER(f->opponent_id) == other) ||
            (f->attacker_id1 > 0 && FIGURE_OWNER(f->attacker_id1) == other) ||
            (f->attacker_id2 > 0 && FIGURE_OWNER(f->attacker_id2) == other))) {
            count++;
        }
    }
    return count;
}

// the first caravan between players in the slice of the player
static figure *caravan_of(int player_id)
{
    for (int i = player_id * MAX_FIGURES + 1; i < (player_id + 1) * MAX_FIGURES; i++) {
        figure *f = figure_get(i);
        if (f->state == FIGURE_STATE_ALIVE && mp_trade_is_caravan(f)) {
            return f;
        }
    }
    return 0;
}

// the tile of the path of a figure `before` steps before its end
static int path_tile_before_end(const figure *f, int before, int *x, int *y)
{
    if (f->routing_path_id <= 0) {
        return 0;
    }
    int px = f->x, py = f->y;
    static const int DX[8] = { 0, 1, 1, 1, 0, -1, -1, -1 };
    static const int DY[8] = { -1, -1, 0, 1, 1, 1, 0, -1 };
    for (int i = f->routing_path_current_tile; i < f->routing_path_length - before; i++) {
        int dir = figure_route_get_direction(f->routing_path_id, i);
        if (dir < 0 || dir > 7) {
            return 0;
        }
        px += DX[dir];
        py += DY[dir];
    }
    *x = px;
    *y = py;
    return 1;
}

// war between players (T5.5, D-077): a war state per pair of players, changed by commands, in the checksum, saved;
// legions fight in the city of the enemy, pull down his buildings and take his caravans; peace stops it all
static int command_war(const char *file)
{
    int failures = 0;
    int war_ticks = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-70s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    war_result first = { { 0, 0 } }, second = { { 0, 0 } };
    if (!start_war_game(file, &first.legion[0], &first.legion[1], &first.target_x, &first.target_y)) {
        printf("Unable to prepare the game\n");
        return 2;
    }
    CHECK(mp_war_status(0, 1) == MP_WAR_PEACE && !mp_war_any_fighting(), "a new game: peace");
    printf("legions: %d soldiers of player 1, %d of player 2\n", alive_soldiers(first.legion[0]),
        alive_soldiers(first.legion[1]));
    const char *start = "war-start.mpsav";
    if (!mp_savegame_write(start)) {
        printf("Unable to save\n");
        return 2;
    }

    // an honourable war: three months of notice, the defender may strike first
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_HONOURABLE, 0);
    CHECK(mp_war_status(0, 1) == MP_WAR_NOTICE && !mp_war_is_fighting(0, 1) && mp_war_days_until_fighting(1, 0) == 48,
        "an honourable war: nobody fights during 48 days (3 months)");
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_HONOURABLE, 0);
    city_action(1, MP_ACTION_DECLARE_WAR, 1, MP_WAR_BRUTAL, 0);
    city_action(0, MP_ACTION_DECLARE_WAR, 7, MP_WAR_BRUTAL, 0);
    CHECK(mp_war_status(0, 1) == MP_WAR_NOTICE && mp_war_days_until_fighting(1, 0) == 48,
        "declaring it again, on oneself or on nobody changes nothing");
    // saved and loaded during the notice
    city_action(1, MP_ACTION_PROPOSE_PEACE, 0, 1, 0);
    uint64_t notice_checksum = mp_checksum_state();
    const char *saved = "war-saved.mpsav";
    CHECK(mp_savegame_write(saved) && mp_savegame_read(saved), "(the game saved and read back during the notice)");
    CHECK(mp_war_status(0, 1) == MP_WAR_NOTICE && mp_war_declarer(1, 0) == 0 &&
        mp_war_form(0, 1) == MP_WAR_HONOURABLE && mp_war_days_until_fighting(0, 1) == 48 &&
        mp_war_peace_proposed(1, 0) && !mp_war_peace_proposed(0, 1),
        "the saved game keeps the war: form, declarer, notice, proposal of peace");
    CHECK(mp_checksum_state() == notice_checksum, "and its checksum");
    city_action(1, MP_ACTION_PROPOSE_PEACE, 0, 0, 0);
    RUN_UNTIL(mp_war_is_fighting(0, 1), 1, 3000);
    printf("the fighting begins %d ticks after the declaration\n", war_ticks);
    CHECK(war_ticks == MP_WAR_NOTICE_TICKS, "the fighting begins at the end of the notice");
    // a game saved before the war between players loads at peace
    CHECK(remove_saved_piece(saved, "mp_war") && mp_savegame_read(saved) && mp_war_status(0, 1) == MP_WAR_PEACE,
        "an older saved game, without war, loads at peace");
    if (!mp_savegame_read(start)) {
        return 2;
    }
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_HONOURABLE, 0);
    war_run(100);
    city_action(1, MP_ACTION_DECLARE_WAR, 0, MP_WAR_BRUTAL, 0);
    CHECK(mp_war_is_fighting(0, 1) && mp_war_declarer(0, 1) == 1 && mp_war_form(0, 1) == MP_WAR_BRUTAL,
        "during the notice, the defender strikes first by a brutal war");
    city_action(0, MP_ACTION_PROPOSE_PEACE, 1, 1, 0);
    city_action(1, MP_ACTION_PROPOSE_PEACE, 0, 1, 0);
    CHECK(mp_war_status(0, 1) == MP_WAR_PEACE, "(peace)");
    city_action(1, MP_ACTION_WITHDRAW_WAR, 0, 0, 0);
    CHECK(mp_war_status(0, 1) == MP_WAR_PEACE, "(at peace, nothing to withdraw)");
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_BRUTAL, 0);
    city_action(0, MP_ACTION_WITHDRAW_WAR, 1, 0, 0);
    CHECK(mp_war_is_fighting(0, 1), "a brutal war is not withdrawn: only peace signed by both ends it");

    // the declarer withdraws his declaration during the notice (D-080): the threat stopped the trade between them,
    // the withdrawal brings it back
    if (!mp_savegame_read(start)) {
        return 2;
    }
    setup_trade(16, 0, 1);
    RUN_UNTIL(marble_of(0) >= 8, 10, 3000);
    CHECK(marble_of(0) == 8 && marble_of(1) == 0 && caravan_of(1), "(in peace, a first caravan delivers 8 loads, "
        "the next one is on its way)");
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_HONOURABLE, 0);
    city_action(0, MP_ACTION_LEGION_MOVE, first.legion[0], first.target_x, first.target_y);
    war_run(1);
    CHECK(caravan_of(1) && BUILDING_OWNER(caravan_of(1)->destination_building_id) == 1,
        "an honourable war declared: the caravan on its way turns back");
    war_run(600);
    city_action(1, MP_ACTION_WITHDRAW_WAR, 0, 0, 0);
    CHECK(mp_war_status(0, 1) == MP_WAR_NOTICE && !mp_war_may_withdraw(1, 0) && mp_war_may_withdraw(0, 1),
        "only the declarer may withdraw it");
    int announcements = mp_war_announcements();
    uint64_t before_withdrawal = mp_checksum_state();
    city_action(0, MP_ACTION_WITHDRAW_WAR, 1, 0, 0);
    CHECK(mp_war_status(0, 1) == MP_WAR_PEACE && mp_war_declarer(0, 1) == -1 && !mp_war_peace_proposed(0, 1) &&
        !mp_war_peace_proposed(1, 0) && mp_checksum_state() != before_withdrawal,
        "the declarer withdraws it during the notice: peace at once, in the checksum");
    CHECK(mp_war_announcements() == announcements + 1, "the withdrawal is announced to every player");
    RUN_UNTIL(marble_of(0) >= 16, 10, 4000);
    printf("the trade resumes: player 1 has %d marble, player 2 %d, %d ticks after the withdrawal\n", marble_of(0),
        marble_of(1), war_ticks);
    CHECK(marble_of(0) == 16 && marble_of(1) == 0 && mp_war_status(0, 1) == MP_WAR_PEACE,
        "the trade between them resumes: the 8 loads brought home are sold again");
    RUN_UNTIL(formation_get(first.legion[0])->is_at_fort, 10, 4000);
    CHECK(formation_get(first.legion[0])->is_at_fort, "the legion sent to the border goes back to its fort");
    // once the fighting began, the declaration cannot be withdrawn any more
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_HONOURABLE, 0);
    RUN_UNTIL(mp_war_is_fighting(0, 1), 10, 3000);
    city_action(0, MP_ACTION_WITHDRAW_WAR, 1, 0, 0);
    CHECK(mp_war_is_fighting(0, 1) && !mp_war_may_withdraw(0, 1),
        "once the fighting began, it is not withdrawn any more");
    // the defender who struck first in the notice made the war his: the first declarer cannot withdraw it
    if (!mp_savegame_read(start)) {
        return 2;
    }
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_HONOURABLE, 0);
    city_action(1, MP_ACTION_DECLARE_WAR, 0, MP_WAR_BRUTAL, 0);
    city_action(0, MP_ACTION_WITHDRAW_WAR, 1, 0, 0);
    city_action(1, MP_ACTION_WITHDRAW_WAR, 0, 0, 0);
    CHECK(mp_war_is_fighting(0, 1), "the defender struck first: nobody withdraws the war any more");

    // the war of the legions, played twice from the same start: the same checksum at the end
    if (!mp_savegame_read(start)) {
        return 2;
    }
    failures += play_war(&first, 1);
    second.legion[0] = first.legion[0];
    second.legion[1] = first.legion[1];
    second.target_x = first.target_x;
    second.target_y = first.target_y;
    if (!mp_savegame_read(start)) {
        return 2;
    }
    play_war(&second, 0);
    CHECK(second.checksum == first.checksum && second.destroyed == first.destroyed &&
        second.dead_attackers == first.dead_attackers,
        "played again from the same start: the same war, the same checksum");

    // peace signed in the middle of a melee: the fights between the two players stop at once, nobody dies after
    if (!mp_savegame_read(start)) {
        return 2;
    }
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_BRUTAL, 0);
    city_action(0, MP_ACTION_LEGION_MOVE, first.legion[0], first.target_x, first.target_y);
    city_action(1, MP_ACTION_LEGION_MOVE, first.legion[1], first.target_x + 1, first.target_y);
    RUN_UNTIL(fighting_against(0, 1) > 0 && fighting_against(1, 0) > 0, 1, 8000);
    printf("melee after %d ticks: %d figures of player 1 and %d of player 2 fight each other\n", war_ticks,
        fighting_against(0, 1), fighting_against(1, 0));
    CHECK(fighting_against(0, 1) > 0 && fighting_against(1, 0) > 0, "at war, the two legions meet in melee");
    city_action(0, MP_ACTION_PROPOSE_PEACE, 1, 1, 0);
    city_action(1, MP_ACTION_PROPOSE_PEACE, 0, 1, 0);
    CHECK(mp_war_status(0, 1) == MP_WAR_PEACE && fighting_against(0, 1) == 0 && fighting_against(1, 0) == 0,
        "peace signed during the melee: every fight between them stops at once");
    int alive1 = alive_soldiers(first.legion[0]), alive2 = alive_soldiers(first.legion[1]);
    war_run(300);
    CHECK(alive_soldiers(first.legion[0]) == alive1 && alive_soldiers(first.legion[1]) == alive2 &&
        fighting_against(0, 1) == 0 && fighting_against(1, 0) == 0, "nobody dies after it, nobody fights again");

    // caravans: player 2 sells marble to player 1, whose legion stands by the road of the caravans
    if (!mp_savegame_read(start)) {
        return 2;
    }
    setup_trade(16, 0, 1);
    RUN_UNTIL(caravan_of(1) && caravan_of(1)->routing_path_id > 0, 10, 2000);
    figure *caravan = caravan_of(1);
    int px = 0, py = 0;
    CHECK(caravan && path_tile_before_end(caravan, 15, &px, &py), "a caravan of player 2 is on its way to player 1");
    if (!caravan) {
        return 1;
    }
    printf("the legion of player 1 stands on the road at (%d, %d), 15 tiles before the warehouse\n", px, py);
    city_action(0, MP_ACTION_LEGION_MOVE, first.legion[0], px, py);
    RUN_UNTIL(marble_of(0) >= 8, 10, 3000);
    printf("in peace: player 1 has %d marble, player 2 %d\n", marble_of(0), marble_of(1));
    CHECK(marble_of(0) == 8 && soldiers_at_standard(first.legion[0]) > 0,
        "in peace, the caravan passes the legion and delivers its 8 loads");
    // the next caravan left a month after the first one (T5.7): war is declared once it passed the legion, 3 tiles
    // before the warehouse; it turns back and meets the legion on its way home
    caravan = caravan_of(1);
    CHECK(caravan && caravan->loads_sold_or_carrying == 8, "the next caravan of 8 loads is on its way");
    RUN_UNTIL(!caravan_of(1) || caravan_of(1)->routing_path_current_tile >= caravan_of(1)->routing_path_length - 3,
        1, 3000);
    int seller_money = treasury_of(1);
    int taken = mp_war_loads_taken(), lost = mp_war_loads_lost();
    CHECK(caravan_of(1) && caravan_of(1)->loads_sold_or_carrying == 8, "it passed the legion in peace too");
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_BRUTAL, 0);
    war_run(1);
    CHECK(caravan_of(1) && BUILDING_OWNER(caravan_of(1)->destination_building_id) == 1,
        "at war, the caravan on its way to player 1 turns back home");
    RUN_UNTIL(!caravan_of(1), 10, 3000);
    printf("at war: player 1 has %d marble, player 2 %d; %d loads taken, %d lost\n", marble_of(0), marble_of(1),
        mp_war_loads_taken() - taken, mp_war_loads_lost() - lost);
    CHECK(marble_of(0) == 16 && marble_of(1) == 0 && mp_war_loads_taken() - taken == 8 && mp_war_loads_lost() == lost,
        "on its way back, the soldiers of player 1 take the 8 loads of the caravan");
    CHECK(treasury_of(1) <= seller_money, "player 2 is not paid for them");
    // more marble for player 2: nothing goes to player 1 while they are at war
    int x1, y1;
    mp_mapgen_city_center(1, &x1, &y1);
    player_context_switch(1);
    building *store = building_get(map_building_at(map_grid_offset(x1 - 6, y1 + 1)));
    for (int i = 0; i < 8; i++) {
        building_warehouse_add_resource(store, RESOURCE_MARBLE);
    }
    player_context_switch(0);
    war_run(40 * 50);
    CHECK(!caravan_of(1) && marble_of(1) == 8, "no caravan between players at war");
    CHECK(mp_trade_purchase_state(1, 0, RESOURCE_MARBLE) == MP_TRADE_PURCHASE_AT_WAR,
        "the trade page says the war is why nothing comes");

    // a caravan on its way when an honourable war is declared: nobody fights during the notice, yet it turns back and
    // brings its goods home; nothing is delivered nor paid
    if (!mp_savegame_read(start)) {
        return 2;
    }
    setup_trade(8, 0, 1);
    RUN_UNTIL(caravan_of(1) && caravan_of(1)->routing_path_id > 0, 10, 2000);
    war_run(500);
    caravan = caravan_of(1);
    int tiles_left = caravan ? caravan->routing_path_length - caravan->routing_path_current_tile : 0;
    seller_money = treasury_of(1);
    int buyer_money = treasury_of(0);
    city_action(1, MP_ACTION_DECLARE_WAR, 0, MP_WAR_HONOURABLE, 0);
    war_run(1);
    CHECK(caravan && tiles_left > 20 && caravan_of(1) && marble_of(1) == 0 &&
        BUILDING_OWNER(caravan_of(1)->destination_building_id) == 1,
        "an honourable war declared while a caravan travels: it turns back");
    RUN_UNTIL(!caravan_of(1), 10, 4000);
    printf("back home after %d ticks: player 1 has %d marble, player 2 %d; treasuries %+d and %+d\n", war_ticks,
        marble_of(0), marble_of(1), treasury_of(0) - buyer_money, treasury_of(1) - seller_money);
    CHECK(!caravan_of(1) && marble_of(1) == 8 && marble_of(0) == 0 && mp_war_status(0, 1) == MP_WAR_NOTICE,
        "its 8 loads are back in the warehouse of player 2, before the fighting");
    CHECK(treasury_of(1) < seller_money + 8 * 150 && treasury_of(0) > buyer_money - 8 * 150,
        "nothing delivered, nothing paid");

    remove(saved);
    remove(start);
    // a classic game: no war
    player_context_switch(0);
    player_context_set_num_players(1);
    if (!load(file)) {
        return 2;
    }
    uint64_t classic = mp_checksum_state();
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_BRUTAL, 0);
    city_action(0, MP_ACTION_WITHDRAW_WAR, 1, 0, 0);
    CHECK(mp_war_status(0, 1) == MP_WAR_PEACE && mp_checksum_state() == classic,
        "a classic game: no war, nothing changes");
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: the war between players is not as designed" :
        "Identical: the war between players is as designed");
    return failures ? 1 : 0;
}

// ---------- Caesar judges the wars (M10.4, M10.5, M10.7, D-084) ----------

static int war_laurels(int player_id)
{
    return mp_caesar_laurels_from(player_id, MP_LAURELS_WARS);
}

static const char *motive_name(int motive)
{
    return motive == MP_WAR_MOTIVE_RETALIATION ? "retaliation" : motive == MP_WAR_MOTIVE_MANDATE ? "mandate" :
        "without motive";
}

// what the wars of player 1 added to the wrath over the months run; the gauge decreases after
static int wrath_months(int months, int *added)
{
    int total = 0;
    for (int i = 0; i < months; i++) {
        run_to_next_month_fast();
        if (added) {
            added[i] = mp_wrath_added_last_month(0);
        }
        total += mp_wrath_added_last_month(0);
    }
    return total;
}

// runs until the given day of the month starts
static void run_to_day(int day)
{
    setting_reset_speeds(500, setting_scroll_speed());
    for (int i = 0; i < 20 * 50 && !(game_time_day() == day && game_time_tick() == 0); i++) {
        run_one_tick();
    }
}

static int last_letter_type(void)
{
    int count = mp_caesar_num_letters();
    return count ? (int) mp_caesar_get_letter(count - 1)->type : -1;
}

static int last_letter_param(void)
{
    int count = mp_caesar_num_letters();
    return count ? mp_caesar_get_letter(count - 1)->param : -1;
}

// two legions of the sizes asked, at their forts, in a game of two players on the prepared map; *tx, *ty: by the
// houses of player 2, where the legion of player 1 goes in the tests of the war
static int start_judged_war(const char *file, int soldiers1, int soldiers2, int *legion1, int *legion2, int *tx,
    int *ty)
{
    player_context_switch(0);
    player_context_set_num_players(1); // a new map in the same process
    if (!start_trade_game(file)) {
        return 0;
    }
    int x0, y0, x1, y1;
    mp_mapgen_city_center(0, &x0, &y0);
    mp_mapgen_city_center(1, &x1, &y1);
    *legion1 = make_legion(0, BUILDING_FORT_LEGIONARIES, x0 - 10, y0 - 8, soldiers1);
    *legion2 = make_legion(1, BUILDING_FORT_LEGIONARIES, x1 + 6, y1 + 4, soldiers2);
    *tx = x1 - 4;
    *ty = y1 - 4;
    war_run(400); // the soldiers reach their forts
    return *legion1 && *legion2;
}

// a tile of the land of `owner` on the line from (x0, y0) to (x1, y1), clear, with no building within 3 tiles
static int land_tile_of(int owner, int x0, int y0, int x1, int y1, int *x, int *y)
{
    int steps = abs(x1 - x0) > abs(y1 - y0) ? abs(x1 - x0) : abs(y1 - y0);
    for (int i = 0; i <= steps; i++) {
        int px = x0 + (x1 - x0) * i / (steps ? steps : 1);
        int py = y0 + (y1 - y0) * i / (steps ? steps : 1);
        int offset = map_grid_offset(px, py);
        if (mp_territory_owner(offset) != owner || map_terrain_is(offset, TERRAIN_NOT_CLEAR)) {
            continue;
        }
        int free = 1;
        for (int dy = -3; dy <= 3 && free; dy++) {
            for (int dx = -3; dx <= 3 && free; dx++) {
                int near = map_grid_offset(px + dx, py + dy);
                free = !map_grid_is_valid_offset(near) || map_building_at(near) <= 0;
            }
        }
        if (free) {
            *x = px;
            *y = py;
            return 1;
        }
    }
    return 0;
}

// a legion of player 1 goes to the tile in peace and stands there with all its soldiers
static int legion_stands_at(int legion, int x, int y)
{
    int war_ticks = 0;
    int soldiers = alive_soldiers(legion);
    city_action(FORMATION_OWNER(legion), MP_ACTION_LEGION_MOVE, legion, x, y);
    RUN_UNTIL(soldiers_at_standard(legion) >= soldiers, 50, 8000);
    return soldiers_at_standard(legion) >= soldiers;
}

// the legion of the defender comes out against the legion standing at (x, y); runs until one of them has no soldier
// left (or `max_ticks`)
static int battle(int legion_out, int legion_standing, int x, int y, int max_ticks)
{
    int war_ticks = 0;
    city_action(FORMATION_OWNER(legion_out), MP_ACTION_LEGION_MOVE, legion_out, x + 1, y);
    RUN_UNTIL(!alive_soldiers(legion_out) || !alive_soldiers(legion_standing), 10, max_ticks);
    return war_ticks;
}

// the motive of a war, its price paid when the fighting starts, the 12 months of a retaliation and after a peace,
// the mandate against an enemy of Rome, the triumphs (CESAR §7.1, D-053, D-080, D-084)
static int command_warmotives(const char *file)
{
    int failures = 0;
    int war_ticks = 0;
    int redeclared = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-100s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    int legion1, legion2, tx, ty;
    if (!start_war_game(file, &legion1, &legion2, &tx, &ty)) {
        printf("Unable to prepare the game\n");
        return 2;
    }
    const char *start = "warmotives-start.mpsav";
    const char *threat = "warmotives-threat.mpsav";
    const char *legions = "warmotives-legions.mpsav";
    if (!mp_savegame_write(start)) {
        printf("Unable to save\n");
        return 2;
    }
    CHECK(mp_wrath_motive_if_declared(0, 1, &redeclared) == MP_WAR_MOTIVE_UNJUST && !redeclared,
        "a new game: a war would be without motive");
    CHECK(mp_wrath_war_price(MP_WAR_MOTIVE_UNJUST, MP_WAR_HONOURABLE) == -150 &&
        mp_wrath_war_price(MP_WAR_MOTIVE_UNJUST, MP_WAR_BRUTAL) == -300 &&
        mp_wrath_war_price(MP_WAR_MOTIVE_RETALIATION, MP_WAR_HONOURABLE) == 0 &&
        mp_wrath_war_price(MP_WAR_MOTIVE_RETALIATION, MP_WAR_BRUTAL) == -150 &&
        mp_wrath_war_price(MP_WAR_MOTIVE_MANDATE, MP_WAR_HONOURABLE) == 0 &&
        mp_wrath_war_price(MP_WAR_MOTIVE_MANDATE, MP_WAR_BRUTAL) == -150,
        "prices: 15 laurels without motive, 15 more for a brutal war, nothing for a just honourable war");

    // an honourable war without motive is paid when its fighting starts
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_HONOURABLE, 0);
    CHECK(mp_wrath_motive(0, 1) == MP_WAR_MOTIVE_UNJUST && war_laurels(0) == 0,
        "an honourable war without motive: nothing paid at the declaration");
    CHECK(mp_wrath_motive_if_declared(1, 0, 0) == MP_WAR_MOTIVE_RETALIATION,
        "its target has a motive of retaliation");
    war_run(MP_WAR_NOTICE_TICKS - 1);
    CHECK(mp_war_status(0, 1) == MP_WAR_NOTICE && war_laurels(0) == 0 && !mp_wrath_is_enemy_of_rome(0),
        "the last tick of the notice: still nothing paid");
    war_run(1);
    printf("the fighting starts: %d tenths of laurels of war for player 1, %d for player 2\n", war_laurels(0),
        war_laurels(1));
    CHECK(mp_war_is_fighting(0, 1) && war_laurels(0) == MP_CAESAR_UNJUST_WAR_LAURELS && war_laurels(1) == 0,
        "the fighting starts: its declarer pays 15 laurels, at that tick");
    CHECK(mp_wrath_is_enemy_of_rome(0) && mp_wrath_enemy_of_rome_months(0) == 12 && !mp_wrath_is_enemy_of_rome(1),
        "a war without motive makes its declarer an enemy of Rome for 12 months");
    run_to_next_month_fast();
    CHECK(war_laurels(0) == MP_CAESAR_UNJUST_WAR_LAURELS, "paid once");

    // a threat withdrawn during its notice costs nothing (D-080)
    if (!mp_savegame_read(start)) {
        return 2;
    }
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_HONOURABLE, 0);
    war_run(600);
    city_action(0, MP_ACTION_WITHDRAW_WAR, 1, 0, 0);
    war_run(MP_WAR_NOTICE_TICKS + 2 * 800);
    CHECK(mp_war_status(0, 1) == MP_WAR_PEACE && war_laurels(0) == 0 && mp_caesar_wrath() == 0 &&
        mp_caesar_belligerence(0) == 0 && !mp_wrath_is_enemy_of_rome(0),
        "a threat withdrawn during its notice costs neither laurels nor wrath");
    CHECK(mp_wrath_motive_if_declared(1, 0, &redeclared) == MP_WAR_MOTIVE_RETALIATION && !redeclared,
        "it gives its target a motive of retaliation");
    CHECK(mp_wrath_motive_if_declared(0, 1, &redeclared) == MP_WAR_MOTIVE_UNJUST && redeclared,
        "declaring it again within 12 months would be without motive, its wrath doubled");
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_HONOURABLE, 0);
    CHECK(mp_wrath_motive(0, 1) == MP_WAR_MOTIVE_UNJUST && mp_wrath_is_redeclared(0, 1) && war_laurels(0) == 0,
        "declared again: without motive, wrath doubled, nothing paid yet");
    RUN_UNTIL(mp_war_is_fighting(0, 1), 10, 3000);
    CHECK(war_laurels(0) == MP_CAESAR_UNJUST_WAR_LAURELS, "paid when its fighting starts");

    // a brutal war without motive: 30 laurels at once
    if (!mp_savegame_read(start)) {
        return 2;
    }
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_BRUTAL, 0);
    CHECK(mp_war_is_fighting(0, 1) && war_laurels(0) == -300 && mp_wrath_is_enemy_of_rome(0),
        "a brutal war without motive: 30 laurels at once, its declarer an enemy of Rome");

    // the defender strikes first during the notice: the war is his, a retaliation, and he pays its brutality
    if (!mp_savegame_read(start)) {
        return 2;
    }
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_HONOURABLE, 0);
    war_run(100);
    city_action(1, MP_ACTION_DECLARE_WAR, 0, MP_WAR_BRUTAL, 0);
    CHECK(mp_war_declarer(0, 1) == 1 && mp_wrath_motive(0, 1) == MP_WAR_MOTIVE_RETALIATION &&
        war_laurels(1) == MP_CAESAR_BRUTAL_WAR_LAURELS && war_laurels(0) == 0,
        "the defender strikes first: his retaliation, 15 laurels for its brutality");
    war_run(MP_WAR_NOTICE_TICKS);
    CHECK(war_laurels(0) == 0 && war_laurels(1) == MP_CAESAR_BRUTAL_WAR_LAURELS && !mp_wrath_is_enemy_of_rome(0) &&
        !mp_wrath_is_enemy_of_rome(1), "the first declarer pays nothing: his war never started; no enemy of Rome");

    // a motive of retaliation lasts 12 months
    if (!mp_savegame_read(start)) {
        return 2;
    }
    city_action(1, MP_ACTION_DECLARE_WAR, 0, MP_WAR_HONOURABLE, 0);
    city_action(1, MP_ACTION_WITHDRAW_WAR, 0, 0, 0);
    if (!mp_savegame_write(threat)) {
        return 2;
    }
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_BRUTAL, 0);
    CHECK(mp_wrath_motive(0, 1) == MP_WAR_MOTIVE_RETALIATION && war_laurels(0) == MP_CAESAR_BRUTAL_WAR_LAURELS &&
        !mp_wrath_is_enemy_of_rome(0),
        "player 1 answers the threat of player 2 by a brutal war: a retaliation, 15 laurels, not an enemy of Rome");
    if (!mp_savegame_read(threat)) {
        return 2;
    }
    war_run(MP_CAESAR_RETALIATION_MONTHS * 800 - 1);
    CHECK(mp_wrath_motive_if_declared(0, 1, 0) == MP_WAR_MOTIVE_RETALIATION &&
        mp_wrath_motive_if_declared(1, 0, &redeclared) == MP_WAR_MOTIVE_UNJUST && redeclared,
        "a tick before 12 months: still a retaliation, and player 2 still bound by his withdrawn threat");
    war_run(1);
    int redeclared_by_2 = 1;
    CHECK(mp_wrath_motive_if_declared(0, 1, &redeclared) == MP_WAR_MOTIVE_UNJUST && !redeclared &&
        mp_wrath_motive_if_declared(1, 0, &redeclared_by_2) == MP_WAR_MOTIVE_UNJUST && !redeclared_by_2,
        "12 months after the threat: no motive any more, nobody bound");

    // a peace binds the declarer of the war, not his enemy
    if (!mp_savegame_read(start)) {
        return 2;
    }
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_BRUTAL, 0);
    city_action(0, MP_ACTION_PROPOSE_PEACE, 1, 1, 0);
    city_action(1, MP_ACTION_PROPOSE_PEACE, 0, 1, 0);
    CHECK(mp_war_status(0, 1) == MP_WAR_PEACE &&
        mp_wrath_motive_if_declared(0, 1, &redeclared) == MP_WAR_MOTIVE_UNJUST && redeclared,
        "after the peace, its declarer may declare it again within 12 months only without motive");
    CHECK(mp_wrath_motive_if_declared(1, 0, &redeclared) == MP_WAR_MOTIVE_MANDATE && !redeclared,
        "his enemy is free: a mandate of Caesar, the declarer of a war without motive being an enemy of Rome");
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_BRUTAL, 0);
    CHECK(mp_wrath_motive(0, 1) == MP_WAR_MOTIVE_UNJUST && mp_wrath_is_redeclared(0, 1) && war_laurels(0) == -600,
        "declared again: without motive, its wrath doubled, 30 laurels again");

    // the acts of war are hostile acts: the declaration, the buildings pulled down, the fights
    if (!mp_savegame_read(start)) {
        return 2;
    }
    CHECK(legion_stands_at(legion1, tx, ty), "(in peace, the legion of player 1 stands by the houses of player 2)");
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_BRUTAL, 0);
    int declared_at = mp_wrath_last_hostile_act(0, 1);
    CHECK(declared_at >= 0 && mp_wrath_last_hostile_act(1, 0) < 0, "the declaration is a hostile act of the declarer");
    int buildings_before, caravans_before, killed_before;
    mp_wrath_damage_done(0, 1, &buildings_before, &caravans_before, &killed_before);
    int buildings2 = buildings_of(1);
    RUN_UNTIL(buildings_of(1) < buildings2, 10, 3000);
    int pulled_down, caravans, killed;
    mp_wrath_damage_done(0, 1, &pulled_down, &caravans, &killed);
    printf("the first building of player 2 down after %d ticks: %d counted\n", war_ticks,
        pulled_down - buildings_before);
    CHECK(pulled_down > buildings_before && mp_wrath_last_hostile_act(0, 1) > declared_at,
        "his soldiers pull down a building of player 2: counted for the wrath, a hostile act");
    int soldiers1 = alive_soldiers(legion1);
    city_action(1, MP_ACTION_LEGION_MOVE, legion2, tx + 1, ty);
    RUN_UNTIL(mp_wrath_last_hostile_act(1, 0) >= 0, 1, 4000);
    printf("the legion of player 2 comes out: its first hostile act after %d ticks, %d of %d soldiers of player 1 "
        "alive\n", war_ticks, alive_soldiers(legion1), soldiers1);
    CHECK(mp_wrath_last_hostile_act(1, 0) >= 0 && alive_soldiers(legion1) == soldiers1,
        "it engages the soldiers of player 1: a hostile act of player 2, before anyone dies");

    // triumphs: a legion destroyed by the defender of a war without motive
    if (!start_judged_war(file, 8, 16, &legion1, &legion2, &tx, &ty)) {
        printf("Unable to prepare the game\n");
        return 2;
    }
    CHECK(legion_stands_at(legion1, tx, ty), "(8 soldiers of player 1 stand in the city of player 2)");
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_BRUTAL, 0);
    int ticks = battle(legion2, legion1, tx, ty, 8000);
    printf("16 soldiers of player 2 come out against them: after %d ticks, %d of player 1 and %d of player 2 alive; "
        "triumphs %d and %d; laurels of war %d and %d tenths\n", ticks, alive_soldiers(legion1),
        alive_soldiers(legion2), mp_wrath_triumphs(0), mp_wrath_triumphs(1), war_laurels(0), war_laurels(1));
    CHECK(!alive_soldiers(legion1) && mp_wrath_triumphs(1) == 1 && war_laurels(1) == MP_CAESAR_TRIUMPH_LAURELS,
        "the defender destroys the legion of the declarer of a war without motive: a triumph, 20 laurels");
    CHECK(mp_wrath_triumphs(0) == 0 && war_laurels(0) == -300, "the declarer: his 30 laurels, no triumph");

    // the declarer of a war without motive destroys a legion: no triumph; the same in a retaliation: a triumph
    if (!start_judged_war(file, 16, 4, &legion1, &legion2, &tx, &ty) || !mp_savegame_write(legions)) {
        printf("Unable to prepare the game\n");
        return 2;
    }
    CHECK(legion_stands_at(legion1, tx, ty), "(16 soldiers of player 1 stand in the city of player 2)");
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_BRUTAL, 0);
    ticks = battle(legion2, legion1, tx, ty, 6000);
    printf("4 soldiers of player 2 come out: after %d ticks, %d of player 1 and %d of player 2 alive\n", ticks,
        alive_soldiers(legion1), alive_soldiers(legion2));
    CHECK(!alive_soldiers(legion2) && mp_wrath_triumphs(0) == 0 && war_laurels(0) == -300,
        "the declarer of a war without motive destroys a legion: no triumph");
    if (!mp_savegame_read(legions)) {
        return 2;
    }
    city_action(1, MP_ACTION_DECLARE_WAR, 0, MP_WAR_HONOURABLE, 0);
    city_action(1, MP_ACTION_WITHDRAW_WAR, 0, 0, 0);
    CHECK(legion_stands_at(legion1, tx, ty), "(after a threat of player 2, the legion of player 1 stands in his city)");
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_BRUTAL, 0);
    ticks = battle(legion2, legion1, tx, ty, 6000);
    printf("a retaliation: after %d ticks, %d of player 1 and %d of player 2 alive; laurels of war %d tenths\n", ticks,
        alive_soldiers(legion1), alive_soldiers(legion2), war_laurels(0));
    CHECK(mp_wrath_motive(0, 1) == MP_WAR_MOTIVE_RETALIATION && !alive_soldiers(legion2) &&
        mp_wrath_triumphs(0) == 1 && war_laurels(0) == MP_CAESAR_BRUTAL_WAR_LAURELS + MP_CAESAR_TRIUMPH_LAURELS,
        "the same legion destroyed in a retaliation: a triumph, 20 laurels");
    remove(legions);
    remove(threat);
    remove(start);

    // three players: the declarer of a war without motive is an enemy of Rome, a war on him a mandate of Caesar
    player_context_switch(0);
    player_context_set_num_players(1);
    if (!start_tradegame(file, 3, 0, "motives")) {
        return 2;
    }
    CHECK(mp_wrath_motive_if_declared(2, 0, 0) == MP_WAR_MOTIVE_UNJUST, "three players: player 3 has no motive");
    run_to_day(8); // no fighting starts on the first day of a month
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_BRUTAL, 0);
    CHECK(mp_wrath_is_enemy_of_rome(0) && mp_wrath_motive_if_declared(2, 0, 0) == MP_WAR_MOTIVE_MANDATE,
        "player 1 wages a war without motive on player 2: a war of player 3 on him is a mandate of Caesar");
    city_action(2, MP_ACTION_DECLARE_WAR, 0, MP_WAR_HONOURABLE, 0);
    RUN_UNTIL(mp_war_is_fighting(0, 2), 10, 3000);
    CHECK(mp_wrath_motive(2, 0) == MP_WAR_MOTIVE_MANDATE && war_laurels(2) == 0,
        "player 3 declares it: a mandate, nothing to pay when its fighting starts");
    // a month of each war: the mandate counts a quarter, the war without motive in full, both brutal or not
    run_to_next_month_fast();
    int months_1 = mp_wrath_months_fighting(0, 1);
    printf("a month: player 1 (brutal, without motive, its month %d) added %d tenths, player 3 (honourable, "
        "mandate, its first month) %d\n", months_1, mp_wrath_added_last_month(0), mp_wrath_added_last_month(2));
    CHECK(mp_wrath_months_fighting(0, 2) == 1 && mp_wrath_added_last_month(2) == 3 &&
        mp_wrath_added_last_month(0) == 15 * months_1,
        "the n-th month: n points x 1.5 for the brutal war without motive, n / 4 for the mandate (rounded up)");

    // a classic game: nothing
    player_context_switch(0);
    player_context_set_num_players(1);
    if (!load(file)) {
        return 2;
    }
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_BRUTAL, 0);
    run_to_next_month_fast();
    CHECK(!mp_caesar_is_active() && war_laurels(0) == 0 && mp_caesar_wrath() == 0 && !mp_wrath_is_enemy_of_rome(0),
        "a classic game: no war, no laurels, no wrath");
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: Caesar does not judge the wars as designed" :
        "Identical: Caesar judges the wars as designed");
    return failures ? 1 : 0;
}

// a hash of the saved state of Caesar and of the wars, cheaper than the checksum of the whole game
static uint64_t judge_state_hash(void)
{
    static uint8_t bytes[8192];
    void (*save[])(buffer *buf) = { mp_caesar_save_state, mp_war_save_state, mp_wrath_save_state };
    uint64_t hash = 0xcbf29ce484222325ULL;
    for (int piece = 0; piece < 3; piece++) {
        buffer buf;
        buffer_init(&buf, bytes, sizeof(bytes));
        save[piece](&buf);
        for (int i = 0; i < buf.index; i++) {
            hash = (hash ^ bytes[i]) * 0x100000001b3ULL;
        }
    }
    return hash;
}

// the ticks of a replay whose whole checksum is compared (it costs much more than a tick on the prepared map): every
// 25th, and every tick around the start of a month, when Caesar judges
static int full_checksum_tick(int tick)
{
    int in_month = tick % (16 * 50);
    return tick % 25 == 0 || in_month <= 20 || in_month >= 16 * 50 - 20;
}

// the first example of CESAR §7.2: a targeted strike without motive, 1 legion, 3 months of war, 2 of them in the
// land of the enemy, 8 buildings and 3 caravans; returns the wrath its declarer added
static int targeted_strike(int legion, int lx, int ly, int form, int *months)
{
    int war_ticks = 0;
    run_to_day(8); // the fighting does not start on the first day of a month
    city_action(0, MP_ACTION_DECLARE_WAR, 1, form, 0);
    RUN_UNTIL(mp_war_is_fighting(0, 1), 10, 3000);
    int total = wrath_months(1, months);
    city_action(0, MP_ACTION_LEGION_MOVE, legion, lx, ly);
    total += wrath_months(1, months + 1);
    // the damage of the example, as the soldiers would do it
    for (int i = 0; i < 8; i++) {
        mp_wrath_building_pulled_down(0, 1);
    }
    for (int i = 0; i < 3; i++) {
        mp_wrath_caravan_intercepted(0, 1);
    }
    total += wrath_months(1, months + 2);
    return total;
}

// Caesar's wrath (CESAR §7.2, §7.3): what the wars add every month, its decrease, the warning, the ultimatum and the
// peace it imposes, the expedition recorded; saved and resumed in the middle of an ultimatum
static int command_caesarwrath(const char *file)
{
    int failures = 0;
    int war_ticks = 0;
    int redeclared = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-100s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    int legion1, legion2, tx, ty;
    if (!start_judged_war(file, 16, 16, &legion1, &legion2, &tx, &ty)) {
        printf("Unable to prepare the game\n");
        return 2;
    }
    const char *start = "caesarwrath-start.mpsav";
    const char *ultimatum = "caesarwrath-ultimatum.mpsav";
    if (!mp_savegame_write(start)) {
        printf("Unable to save\n");
        return 2;
    }
    int x0, y0, x1, y1, lx = 0, ly = 0;
    mp_mapgen_city_center(0, &x0, &y0);
    mp_mapgen_city_center(1, &x1, &y1);
    CHECK(land_tile_of(1, x0, y0, x1, y1, &lx, &ly), "(a tile of open land of player 2, on the way from player 1)");
    printf("the legion of player 1 will stand at (%d, %d), in the land of player 2 (his city at (%d, %d))\n", lx, ly,
        x1, y1);

    // the decrease, in general peace: 3 a month, every share in proportion
    mp_caesar_add_wrath(0, 60);
    mp_caesar_add_wrath(1, 40);
    run_to_next_month_fast();
    printf("in peace, a month later: gauge %d, shares %d and %d (tenths)\n", mp_caesar_wrath(),
        mp_caesar_belligerence(0), mp_caesar_belligerence(1));
    CHECK(mp_caesar_wrath() == 70 && mp_caesar_belligerence(0) == 42 && mp_caesar_belligerence(1) == 28,
        "in general peace the gauge loses 3 a month, each share in proportion");
    wrath_months(3, 0);
    CHECK(mp_caesar_wrath() == 0 && mp_caesar_belligerence(0) == 0 && mp_caesar_belligerence(1) == 0,
        "10 points are gone in 4 months of peace");
    CHECK(mp_caesar_num_letters() == 0, "no letter of Caesar below the warning");

    // CESAR §7.2, a targeted strike without motive: duration 6, power 4, damage 14, 24 points
    if (!mp_savegame_read(start)) {
        return 2;
    }
    int months[3];
    int added = targeted_strike(legion1, lx, ly, MP_WAR_HONOURABLE, months);
    int buildings, caravans, killed;
    mp_wrath_damage_done(0, 1, &buildings, &caravans, &killed);
    printf("a targeted strike without motive: months of war add %d, %d and %d tenths, %d in all; the gauge %d after "
        "its decrease; %d legion in the land of player 2; damage %d buildings, %d caravans, %d killed\n", months[0],
        months[1], months[2], added, mp_caesar_wrath(), mp_wrath_legions_in_land_of(0, 1), buildings, caravans, killed);
    CHECK(months[0] == 10 && months[1] == 40 && months[2] == 190 && added == 240,
        "duration 1 + 2 + 3, power 2 + 2, damage 8 + 6: 24 points, as in CESAR 7.2");
    CHECK(mp_caesar_wrath() == 240 - 3 * MP_CAESAR_WRATH_DECAY_WAR && mp_caesar_belligerence(0) == mp_caesar_wrath() &&
        mp_caesar_belligerence(1) == 0, "the gauge, less 1 a month of war: all of it the share of player 1");
    // the same in peace afterwards: gone in 7 months
    city_action(0, MP_ACTION_PROPOSE_PEACE, 1, 1, 0);
    city_action(1, MP_ACTION_PROPOSE_PEACE, 0, 1, 0);
    int peace_months = 0;
    while (mp_caesar_wrath() > 0 && peace_months < 24) {
        wrath_months(1, 0);
        peace_months++;
    }
    printf("after the peace, the gauge is back to 0 in %d months\n", peace_months);
    CHECK(peace_months == 7, "after the peace, 21 points gone in 7 months");
    // the same strike, brutal: 36 points
    if (!mp_savegame_read(start)) {
        return 2;
    }
    added = targeted_strike(legion1, lx, ly, MP_WAR_BRUTAL, months);
    printf("the same strike as a brutal war: %d, %d and %d tenths, %d in all\n", months[0], months[1], months[2],
        added);
    CHECK(months[0] == 15 && months[1] == 60 && months[2] == 285 && added == 360,
        "the same strike as a brutal war: 36 points, as in CESAR 7.2");
    // declared again within 12 months of its peace: doubled
    city_action(0, MP_ACTION_PROPOSE_PEACE, 1, 1, 0);
    city_action(1, MP_ACTION_PROPOSE_PEACE, 0, 1, 0);
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_BRUTAL, 0);
    wrath_months(1, months);
    printf("declared again within 12 months: a first month adds %d tenths\n", months[0]);
    CHECK(mp_wrath_is_redeclared(0, 1) && months[0] == 30, "declared again within 12 months: its wrath counts double");

    // the warning, the ultimatum, the peace it imposes
    if (!mp_savegame_read(start)) {
        return 2;
    }
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_HONOURABLE, 0);
    RUN_UNTIL(mp_war_is_fighting(0, 1), 10, 3000);
    wrath_months(2, 0);
    int letters = mp_caesar_num_letters();
    mp_caesar_add_wrath(0, 395 - mp_caesar_wrath());
    wrath_months(1, months);
    printf("the gauge at 39.5 points, a third month of war adds %d: %d letter(s) of Caesar\n", months[0],
        mp_caesar_num_letters() - letters);
    CHECK(mp_caesar_num_letters() == letters + 1 && last_letter_type() == MP_CAESAR_LETTER_WARNING &&
        last_letter_param() == 1, "crossing 40: a letter of warning to every player, naming player 1");
    wrath_months(1, 0);
    CHECK(mp_caesar_num_letters() == letters + 1, "no second warning while the gauge stays above");
    mp_caesar_add_wrath(0, 695 - mp_caesar_wrath());
    wrath_months(1, months);
    CHECK(mp_caesar_num_letters() == letters + 2 && last_letter_type() == MP_CAESAR_LETTER_ULTIMATUM &&
        last_letter_param() == 0 && mp_wrath_ultimatum_months() == MP_CAESAR_ULTIMATUM_DELAY &&
        mp_wrath_ultimatum_culprit() == 0,
        "crossing 70: an ultimatum, peace within 3 months; player 1 the main culprit");
    CHECK(mp_wrath_is_enemy_of_rome(0) && mp_wrath_enemy_of_rome_months(0) == 12,
        "the main culprit is an enemy of Rome for 12 months");
    // saved in the middle of the ultimatum, read back twice: the same game tick after tick
    wrath_months(1, 0);
    if (!mp_savegame_write(ultimatum)) {
        return 2;
    }
    int ultimatum_wrath = mp_caesar_wrath();
    const int replay_ticks = 2 * 800 + 100;
    static uint64_t checksums[2 * 800 + 100];
    static uint64_t judge_hashes[2 * 800 + 100];
    CHECK(mp_savegame_read(ultimatum) && mp_wrath_ultimatum_months() == 2 && mp_caesar_wrath() == ultimatum_wrath &&
        mp_wrath_ultimatum_culprit() == 0 && mp_wrath_is_enemy_of_rome(0) && mp_war_is_fighting(0, 1),
        "(saved and read back a month after the ultimatum: two months left)");
    setting_reset_speeds(500, setting_scroll_speed());
    int imposed_after = -1;
    letters = mp_caesar_num_letters();
    for (int i = 0; i < replay_ticks; i++) {
        run_one_tick();
        judge_hashes[i] = judge_state_hash();
        checksums[i] = full_checksum_tick(i + 1) ? mp_checksum_state() : 0;
        if (imposed_after < 0 && mp_war_status(0, 1) == MP_WAR_PEACE) {
            imposed_after = i + 1;
        }
    }
    printf("peace imposed %d ticks after the reading: letter %d with players %d\n", imposed_after, last_letter_type(),
        last_letter_param());
    CHECK(mp_war_status(0, 1) == MP_WAR_PEACE && mp_wrath_ultimatum_months() == 0 &&
        mp_caesar_num_letters() == letters + 1 && last_letter_type() == MP_CAESAR_LETTER_PEACE_IMPOSED &&
        last_letter_param() == 3, "at the deadline, Caesar imposes peace and tells every player");
    CHECK(mp_wrath_motive_if_declared(0, 1, &redeclared) == MP_WAR_MOTIVE_UNJUST && redeclared,
        "as a signed peace, it binds the declarer for 12 months");
    CHECK(mp_wrath_motive_if_declared(1, 0, &redeclared) == MP_WAR_MOTIVE_MANDATE && !redeclared,
        "the culprit is an enemy of Rome: a war of his victim on him is a mandate of Caesar");
    CHECK(mp_savegame_read(ultimatum), "(read back again)");
    int same = 1;
    int first_difference = -1;
    int full_checksums = 0;
    for (int i = 0; i < replay_ticks; i++) {
        run_one_tick();
        int different = judge_state_hash() != judge_hashes[i];
        if (checksums[i]) {
            different |= mp_checksum_state() != checksums[i];
            full_checksums++;
        }
        if (different && same) {
            same = 0;
            first_difference = i + 1;
        }
    }
    printf("replayed %d ticks: the state of Caesar and of the wars compared at every tick, the whole game at %d of "
        "them\n", replay_ticks, full_checksums);
    if (!same) {
        printf("first difference %d ticks after the reading\n", first_difference);
    }
    CHECK(same, "resumed in the middle of an ultimatum: the same game tick after tick, through the imposed peace");

    // the punitive expedition: sent at once (M10.6, its own test: mp_caesar_expedition)
    city_action(1, MP_ACTION_DECLARE_WAR, 0, MP_WAR_BRUTAL, 0);
    letters = mp_caesar_num_letters();
    mp_caesar_add_wrath(1, MP_CAESAR_WRATH_MAX - mp_caesar_wrath());
    wrath_months(1, 0);
    CHECK(!mp_wrath_expedition_due() && mp_expedition_count() == 1 && mp_caesar_num_letters() == letters + 1 &&
        last_letter_type() == MP_CAESAR_LETTER_EXPEDITION, "reaching 100: the punitive expedition is sent (M10.6)");
    if (!mp_savegame_write(ultimatum) || !mp_savegame_read(ultimatum)) {
        return 2;
    }
    CHECK(mp_expedition_count() == 1, "(and saved)");

    // a game saved before Caesar judged the wars: the wars in progress have no motive and are paid for
    if (!mp_savegame_read(start)) {
        return 2;
    }
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_HONOURABLE, 0);
    if (!mp_savegame_write(ultimatum) || !remove_saved_piece(ultimatum, "mp_wrath") || !mp_savegame_read(ultimatum)) {
        printf("Unable to write an older saved game\n");
        return 2;
    }
    RUN_UNTIL(mp_war_is_fighting(0, 1), 10, 3000);
    CHECK(mp_war_is_fighting(0, 1) && war_laurels(0) == 0 && mp_wrath_motive(0, 1) == MP_WAR_MOTIVE_UNJUST,
        "an older saved game: its war goes on without motive, and nothing is paid at its start");
    remove(ultimatum);
    remove(start);

    // a classic game: nothing
    player_context_switch(0);
    player_context_set_num_players(1);
    if (!load(file)) {
        return 2;
    }
    mp_caesar_add_wrath(0, 500);
    wrath_months(2, 0);
    CHECK(!mp_caesar_is_active() && mp_caesar_wrath() == 0 && mp_caesar_num_letters() == 0 &&
        !mp_wrath_ultimatum_months(), "a classic game: no wrath, no letter");
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: Caesar's wrath is not as designed" :
        "Identical: Caesar's wrath is as designed");
    return failures ? 1 : 0;
}

// a fort with soldiers for the player, at the first place where one can be built around (x, y)
static int legion_near(int player_id, int x, int y, int soldiers)
{
    static const int DX[] = { -10, -10, -16, -4, -16, 6, 6, 12, -10, -16, 12, 0, -22, -22 };
    static const int DY[] = { -8, -14, -8, -14, -14, -14, -8, -8, 6, 6, 6, 10, -8, 0 };
    for (int i = 0; i < (int) (sizeof(DX) / sizeof(DX[0])); i++) {
        if (map_building_at(map_grid_offset(x + DX[i], y + DY[i])) > 0) {
            continue; // a fort already stands there
        }
        int legion = make_legion(player_id, BUILDING_FORT_LEGIONARIES, x + DX[i], y + DY[i], soldiers);
        if (legion) {
            printf("(a fort of player %d at %+d, %+d from his city)\n", player_id + 1, DX[i], DY[i]);
            return legion;
        }
    }
    return 0;
}

// an army too powerful (CESAR §7.5, M10.7): Caesar tolerates max(2, population / 5000) legions; every legion more
// costs 2 laurels a month, at war or at peace, also to a player alone
static int command_armytolerance(const char *file)
{
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-90s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    CHECK(mp_caesar_tolerated_legions(0) == 2 && mp_caesar_tolerated_legions(9999) == 2 &&
        mp_caesar_tolerated_legions(14999) == 2 && mp_caesar_tolerated_legions(15000) == 3 &&
        mp_caesar_tolerated_legions(24999) == 4 && mp_caesar_tolerated_legions(25000) == 5,
        "tolerated: 2 up to 14 999 inhabitants, then one legion for each 5 000");
    CHECK(mp_caesar_army_cost(2, 0) == 0 && mp_caesar_army_cost(3, 0) == -20 && mp_caesar_army_cost(5, 9000) == -60 &&
        mp_caesar_army_cost(3, 15000) == 0 && mp_caesar_army_cost(4, 15000) == -20,
        "each legion beyond: 2 laurels a month");
    // alone on the prepared map
    if (!mp_mapgen_create_prepared(file, 1, 0)) {
        printf("Unable to create the prepared map\n");
        return 2;
    }
    game_rules_settings rules;
    game_rules_default_multiplayer_settings(&rules);
    rules.ai_invasions = 0;
    game_rules_set_multiplayer(&rules);
    int x0, y0;
    mp_mapgen_city_center(0, &x0, &y0);
    int legions[4];
    for (int i = 0; i < 4; i++) {
        legions[i] = legion_near(0, x0, y0, i < 3 ? 4 : 0);
    }
    war_run(400);
    printf("alone: %d inhabitants, %d legions with soldiers, %d tolerated\n", city_population(),
        mp_caesar_army_legions(0), mp_caesar_tolerated_legions(city_population()));
    CHECK(legions[0] && legions[1] && legions[2] && legions[3] && mp_caesar_army_legions(0) == 3,
        "alone: three legions with soldiers, and an empty fort that does not count");
    run_to_next_month_fast();
    int first = mp_caesar_laurels_from(0, MP_LAURELS_ARMY);
    run_to_next_month_fast();
    printf("alone with 3 legions: %d then %d tenths of laurels of the army\n", first,
        mp_caesar_laurels_from(0, MP_LAURELS_ARMY));
    CHECK(first == -20 && mp_caesar_laurels_from(0, MP_LAURELS_ARMY) == -40 && mp_caesar_wrath() == 0,
        "alone, a legion too many: 2 laurels a month, no wrath");
    // the fourth fort gets soldiers: two legions too many
    player_context_switch(0);
    formation *m = formation_get(legions[3]);
    figure *soldier = figure_create(m->figure_type, m->x_home, m->y_home, DIR_0_TOP);
    soldier->formation_id = legions[3];
    soldier->formation_at_rest = 1;
    soldier->action_state = FIGURE_ACTION_81_SOLDIER_GOING_TO_FORT;
    formation_calculate_figures();
    run_to_next_month_fast();
    CHECK(mp_caesar_army_legions(0) == 4 && mp_caesar_laurels_from(0, MP_LAURELS_ARMY) == -80,
        "four legions: 4 laurels a month");
    // two players: each city on its own
    player_context_switch(0);
    player_context_set_num_players(1);
    if (!start_trade_game(file)) {
        return 2;
    }
    int x1, y1;
    mp_mapgen_city_center(1, &x1, &y1);
    for (int i = 0; i < 3; i++) {
        legion_near(1, x1, y1, 2);
    }
    war_run(100);
    run_to_next_month_fast();
    CHECK(mp_caesar_army_legions(1) == 3 && mp_caesar_laurels_from(1, MP_LAURELS_ARMY) == -20 &&
        mp_caesar_laurels_from(0, MP_LAURELS_ARMY) == 0, "two players: only the city with 3 legions pays");
    // a classic game: nothing
    player_context_switch(0);
    player_context_set_num_players(1);
    if (!load(file)) {
        return 2;
    }
    run_to_next_month_fast();
    CHECK(!mp_caesar_is_active() && mp_caesar_laurels_from(0, MP_LAURELS_ARMY) == 0, "a classic game: nothing");
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: the army tolerated by Caesar is not as designed" :
        "Identical: the army tolerated by Caesar is as designed");
    return failures ? 1 : 0;
}

// ---------- Caesar's punitive expedition (M10.6, D-086) ----------

// Caesar's soldiers alive in the city of a player, counted here from the figures; *x, *y: where the first one stands
static int caesar_soldiers(int player_id, int *x, int *y)
{
    int count = 0;
    for (int i = player_id * MAX_FIGURES + 1; i < (player_id + 1) * MAX_FIGURES; i++) {
        figure *f = figure_get(i);
        if (f->state != FIGURE_STATE_ALIVE || f->type != FIGURE_ENEMY_CAESAR_LEGIONARY ||
            f->action_state == FIGURE_ACTION_149_CORPSE) {
            continue;
        }
        if (!count && x && y) {
            *x = f->x;
            *y = f->y;
        }
        count++;
    }
    return count;
}

// every soldier of Caesar in the city of the player: the legionary of the original, in a formation of the city, come
// from its point of arrival; the number of his formations
static int caesar_soldiers_as_original(int player_id, int *formations)
{
    int ok = 1;
    int seen[MAX_FORMATIONS] = { 0 };
    *formations = 0;
    player_context_switch(player_id);
    map_point entry = scenario_map_entry();
    for (int i = player_id * MAX_FIGURES + 1; i < (player_id + 1) * MAX_FIGURES; i++) {
        figure *f = figure_get(i);
        if (f->state != FIGURE_STATE_ALIVE || f->type != FIGURE_ENEMY_CAESAR_LEGIONARY) {
            continue;
        }
        const formation *m = formation_get(f->formation_id);
        ok &= f->formation_id > 0 && FORMATION_OWNER(f->formation_id) == player_id && m->in_use &&
            m->figure_type == FIGURE_ENEMY_CAESAR_LEGIONARY && !m->is_legion &&
            m->attack_type == FORMATION_ATTACK_BEST_BUILDINGS && f->source_x == entry.x && f->source_y == entry.y;
        int local = FORMATION_LOCAL_ID(f->formation_id);
        if (local >= 0 && local < MAX_FORMATIONS && !seen[local]) {
            seen[local] = 1;
            (*formations)++;
        }
    }
    player_context_switch(0);
    return ok;
}

static int punishment_laurels(int player_id)
{
    return mp_caesar_laurels_from(player_id, MP_LAURELS_PUNISHMENT);
}

static void set_second_expedition_ends_game(int on)
{
    game_rules_settings rules = *game_rules_multiplayer_settings();
    rules.rome_takes_province = on;
    game_rules_set_multiplayer(&rules);
}

// the legions of a player march on Caesar's soldiers in his city, again every 100 ticks, until none is left (or
// `max_ticks`); returns the ticks it took
static int fight_expedition(int player_id, const int *legions, int num_legions, int max_ticks)
{
    int ticks = 0;
    int x = 0, y = 0;
    while (caesar_soldiers(player_id, &x, &y) > 0 && ticks < max_ticks) {
        for (int i = 0; i < num_legions; i++) {
            if (alive_soldiers(legions[i])) {
                city_action(player_id, MP_ACTION_LEGION_MOVE, legions[i], x, y);
            }
        }
        war_run(100);
        ticks += 100;
    }
    return ticks;
}

static int legions_soldiers(const int *legions, int num_legions)
{
    int soldiers = 0;
    for (int i = 0; i < num_legions; i++) {
        soldiers += alive_soldiers(legions[i]);
    }
    return soldiers;
}

// a hash of the saved state of the expedition, with that of Caesar and of the wars
static uint64_t expedition_state_hash(void)
{
    static uint8_t bytes[1024];
    buffer buf;
    buffer_init(&buf, bytes, sizeof(bytes));
    mp_expedition_save_state(&buf);
    uint64_t hash = judge_state_hash();
    for (int i = 0; i < buf.index; i++) {
        hash = (hash ^ bytes[i]) * 0x100000001b3ULL;
    }
    return hash;
}

// laurels of the notes over months, while the province is in disgrace or not: for each note, what it brought against
// what its monthly notes give, halved or not (the remainders carried from month to month make it exact within a
// tenth); returns whether every note brought what it should
static int notes_laurels_months(int player_id, int months, int halved, int *gained, int *expected)
{
    static const int weights[MP_NOTE_MAX] = {
        MP_CAESAR_WEIGHT_PROSPERITY, MP_CAESAR_WEIGHT_TRADE, MP_CAESAR_WEIGHT_HOUSING, MP_CAESAR_WEIGHT_CULTURE,
        MP_CAESAR_WEIGHT_GREATNESS
    };
    long long hundredths[MP_NOTE_MAX] = { 0 };
    int start[MP_NOTE_MAX];
    for (int note = 0; note < MP_NOTE_MAX; note++) {
        start[note] = mp_caesar_laurels_from(player_id, note);
    }
    for (int m = 0; m < months; m++) {
        run_to_next_month_fast();
        for (int note = 0; note < MP_NOTE_MAX; note++) {
            int earned = weights[note] * mp_caesar_note(player_id, note);
            hundredths[note] += halved ? earned / 2 : earned;
        }
    }
    int ok = 1;
    *gained = *expected = 0;
    for (int note = 0; note < MP_NOTE_MAX; note++) {
        long long gain = mp_caesar_laurels_from(player_id, note) - start[note];
        ok &= 100 * gain > hundredths[note] - 100 && 100 * gain < hundredths[note] + 100;
        *gained += (int) gain;
        *expected += (int) (hundredths[note] / 100);
    }
    return ok;
}

// Caesar's punitive expedition (CESAR §7.4, D-053, D-086): his legions in every city, more for the culprits, the
// laurels the culprits lose, the peace, the gauge at 30, the disgrace, the legions destroyed or gone after 12 months,
// the second expedition with the option of the lobby off and on; saved and replayed; three players; alone; classic
static int command_caesarexpedition(const char *file)
{
    int failures = 0;
    int war_ticks = 0;
    int redeclared = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-100s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    const figure_properties *props = figure_properties_for_type(FIGURE_ENEMY_CAESAR_LEGIONARY);
    CHECK(props->attack_value == 13 && props->defense_value == 2,
        "Caesar's legionaries of the original game: attack 13, defence 2");

    // two players on the prepared map: player 2 wages war on player 1, who has four legions
    int legion1, legion2, tx, ty;
    if (!start_judged_war(file, 16, 16, &legion1, &legion2, &tx, &ty)) {
        printf("Unable to prepare the game\n");
        return 2;
    }
    int x0, y0;
    mp_mapgen_city_center(0, &x0, &y0);
    int legions1[4] = { legion1 };
    for (int i = 1; i < 4; i++) {
        legions1[i] = legion_near(0, x0, y0, 16);
    }
    war_run(400);
    CHECK(legions1[1] && legions1[2] && legions1[3] && legions_soldiers(legions1, 4) == 64,
        "(player 1 has four legions of 16 soldiers)");
    for (int p = 0; p < 2; p++) {
        player_context_switch(p);
        map_point entry = scenario_map_entry();
        printf("player %d arrives at (%d, %d)\n", p + 1, entry.x, entry.y);
    }
    player_context_switch(0);
    // the option of the lobby: the first expedition strikes all the same
    set_second_expedition_ends_game(1);
    CHECK(!game_rules_ai_invasions() && game_rules_rome_takes_province(),
        "(AI invasions off, a second expedition would end the game)");
    mp_caesar_add_laurels(0, MP_LAURELS_GIFTS, 1000);
    mp_caesar_add_laurels(1, MP_LAURELS_GIFTS, 800);
    run_to_day(8);
    city_action(1, MP_ACTION_DECLARE_WAR, 0, MP_WAR_BRUTAL, 0);
    CHECK(mp_war_is_fighting(0, 1) && mp_wrath_motive(1, 0) == MP_WAR_MOTIVE_UNJUST,
        "(player 2 wages a brutal war without motive on player 1)");
    // the war adds 1.5 points at the next month: the gauge reaches 100, all of it the share of player 2
    mp_caesar_add_wrath(1, MP_CAESAR_WRATH_MAX - 15 - mp_caesar_wrath());
    int letters = mp_caesar_num_letters();
    int announcements = mp_war_announcements();
    run_to_next_month_fast();
    int soldiers[2], formations[2];
    for (int p = 0; p < 2; p++) {
        soldiers[p] = caesar_soldiers(p, 0, 0);
    }
    int as_original[2] = { caesar_soldiers_as_original(0, &formations[0]), caesar_soldiers_as_original(1,
        &formations[1]) };
    int lost2 = -punishment_laurels(1);
    int before2 = mp_caesar_laurels(1) + lost2;
    printf("the expedition: %d soldiers in %d formations in the city of player 1, %d in %d in that of player 2; "
        "player 2 loses %d tenths of %d; gauge %d, shares %d and %d; %d letters\n", soldiers[0], formations[0],
        soldiers[1], formations[1], lost2, before2, mp_caesar_wrath(), mp_caesar_belligerence(0),
        mp_caesar_belligerence(1), mp_caesar_num_letters() - letters);
    CHECK(mp_expedition_count() == 1 && !mp_wrath_expedition_due() && !mp_endgame_is_over(),
        "reaching 100: the punitive expedition is sent; the first one, also with the option of the lobby");
    CHECK(soldiers[0] == MP_CAESAR_EXPEDITION_SOLDIERS_INNOCENT && mp_expedition_soldiers_landed(0) == soldiers[0],
        "an innocent city: 32 soldiers of Caesar");
    CHECK(soldiers[1] == MP_CAESAR_EXPEDITION_SOLDIERS_CULPRIT_MAX && mp_expedition_soldiers_landed(1) == soldiers[1],
        "the main culprit with all the wrath: 144 soldiers");
    CHECK(as_original[0] && as_original[1] && formations[0] == 2 && formations[1] == 3,
        "the invasion of Caesar of the original: his legionaries, at the arrival of each city, in its formations");
    CHECK(!game_rules_ai_invasions() && soldiers[0] > 0 && soldiers[1] > 0, "\"AI invasions: no\" does not stop it");
    CHECK(lost2 == before2 * MP_CAESAR_CULPRIT_LOSS_PERCENT / 100 && lost2 > 0 && punishment_laurels(0) == 0 &&
        mp_expedition_laurels_lost(1) == lost2, "the main culprit loses 25 % of his laurels, the innocent nothing");
    CHECK(mp_war_status(0, 1) == MP_WAR_PEACE && !mp_wrath_ultimatum_months() &&
        mp_wrath_motive_if_declared(1, 0, &redeclared) == MP_WAR_MOTIVE_UNJUST && redeclared &&
        mp_war_announcements() > announcements, "peace on every war, as a signed peace (it binds the declarer)");
    CHECK(mp_caesar_wrath() == MP_CAESAR_WRATH_AFTER_EXPEDITION && mp_caesar_belligerence(1) == mp_caesar_wrath() &&
        mp_caesar_belligerence(0) == 0, "the gauge falls to 30, the shares with it");
    CHECK(mp_expedition_disgrace_months() == MP_CAESAR_DISGRACE_MONTHS, "the province in disgrace for 12 months");
    CHECK(mp_caesar_num_letters() == letters + 3 && last_letter_type() == MP_CAESAR_LETTER_EXPEDITION &&
        last_letter_param() == 1, "a letter of Caesar to every player, naming the main culprit (after the warning and "
        "the ultimatum of the same month)");
    CHECK(mp_expedition_state_of(0) == MP_EXPEDITION_IN_CITY && mp_expedition_months_left(0) == 12 &&
        mp_expedition_state_of(1) == MP_EXPEDITION_IN_CITY, "the legions stay 12 months at most");
    const char *landed = "caesarexpedition-landed.mpsav";
    if (!mp_savegame_write(landed)) {
        return 2;
    }

    // saved during the expedition, read back twice: the same game tick after tick
    const int replay_ticks = 2 * 800 + 100;
    static uint64_t checksums[2 * 800 + 100];
    static uint64_t hashes[2 * 800 + 100];
    CHECK(mp_savegame_read(landed) && mp_expedition_count() == 1 && caesar_soldiers(1, 0, 0) == 144 &&
        mp_expedition_disgrace_months() == 12 && game_rules_rome_takes_province(),
        "(saved and read back: the expedition, the disgrace and the option of the lobby are in the saved game)");
    setting_reset_speeds(500, setting_scroll_speed());
    for (int i = 0; i < replay_ticks; i++) {
        run_one_tick();
        hashes[i] = expedition_state_hash();
        checksums[i] = full_checksum_tick(i + 1) ? mp_checksum_state() : 0;
    }
    int alive_after[2] = { caesar_soldiers(0, 0, 0), caesar_soldiers(1, 0, 0) };
    CHECK(mp_savegame_read(landed), "(read back again)");
    int same = 1;
    int first_difference = -1;
    for (int i = 0; i < replay_ticks; i++) {
        run_one_tick();
        int different = expedition_state_hash() != hashes[i] || (checksums[i] && mp_checksum_state() != checksums[i]);
        if (different && same) {
            same = 0;
            first_difference = i + 1;
        }
    }
    printf("replayed %d ticks from the saved game: %d and %d soldiers of Caesar left%s\n", replay_ticks,
        alive_after[0], alive_after[1], same ? "" : ", DIFFERENT");
    if (!same) {
        printf("first difference %d ticks after the reading\n", first_difference);
    }
    CHECK(same && alive_after[0] == caesar_soldiers(0, 0, 0) && alive_after[1] == caesar_soldiers(1, 0, 0),
        "resumed during the expedition: the same game tick after tick");

    // the legions of player 1 destroy the expedition in his city: 20 laurels
    if (!mp_savegame_read(landed)) {
        return 2;
    }
    int landing_month = 12 * game_time_year() + game_time_month();
    letters = mp_caesar_num_letters();
    int ticks = fight_expedition(0, legions1, 4, 12 * 800);
    war_run(50); // a day for Caesar to see it
    printf("the four legions of player 1 destroy the 32 soldiers of Caesar in %d ticks (%d months), %d of their 64 "
        "soldiers alive; laurels of the expedition %d tenths\n", ticks, ticks / 800, legions_soldiers(legions1, 4),
        punishment_laurels(0));
    CHECK(!caesar_soldiers(0, 0, 0) && mp_expedition_state_of(0) == MP_EXPEDITION_DEFEATED &&
        punishment_laurels(0) == MP_CAESAR_EXPEDITION_DEFEATED_LAURELS,
        "destroying the expedition gives the city 20 laurels");
    CHECK(mp_caesar_num_letters() == letters + 1 && last_letter_type() == MP_CAESAR_LETTER_EXPEDITION_DEFEATED &&
        last_letter_param() == MP_CAESAR_EXPEDITION_DEFEATED_LAURELS, "and a letter of Caesar to its player");
    // the 144 soldiers in the city of player 2 stay 12 months, then go back where they landed
    int months = 12 * game_time_year() + game_time_month() - landing_month;
    while (mp_expedition_state_of(1) == MP_EXPEDITION_IN_CITY && months < 14) {
        run_to_next_month_fast();
        months = 12 * game_time_year() + game_time_month() - landing_month;
    }
    int leaving = caesar_soldiers(1, 0, 0);
    int punishment2 = punishment_laurels(1);
    announcements = mp_war_announcements();
    RUN_UNTIL(mp_expedition_state_of(1) != MP_EXPEDITION_LEAVING, 50, 6 * 800);
    printf("the expedition of player 2 leaves after %d months with %d soldiers; gone %d ticks later\n", months,
        leaving, war_ticks);
    CHECK(months == MP_CAESAR_EXPEDITION_MONTHS && leaving > 0, "the expedition leaves after 12 months");
    CHECK(mp_expedition_state_of(1) == MP_EXPEDITION_LEFT && !caesar_soldiers(1, 0, 0) &&
        punishment_laurels(1) == punishment2 && mp_war_announcements() > announcements,
        "its soldiers go back where they landed: no laurels, an announcement to every player");
    CHECK(mp_expedition_disgrace_months() == 0, "(the disgrace is over too)");

    // a second expedition, the option of the lobby off: it strikes like the first one
    if (!mp_savegame_read(landed)) {
        return 2;
    }
    set_second_expedition_ends_game(0);
    int punishment_first = punishment_laurels(1);
    mp_caesar_add_wrath(1, MP_CAESAR_WRATH_MAX - mp_caesar_wrath());
    run_to_next_month_fast();
    lost2 = -(punishment_laurels(1) - punishment_first);
    before2 = mp_caesar_laurels(1) + lost2;
    printf("a second expedition: %d and %d soldiers more, player 2 loses %d tenths of %d\n",
        mp_expedition_soldiers_landed(0), mp_expedition_soldiers_landed(1), lost2, before2);
    CHECK(mp_expedition_count() == 2 && !mp_endgame_is_over() && mp_expedition_soldiers_landed(0) == 32 &&
        mp_expedition_soldiers_landed(1) == 144 && lost2 == before2 * MP_CAESAR_CULPRIT_LOSS_PERCENT / 100 &&
        mp_caesar_wrath() == MP_CAESAR_WRATH_AFTER_EXPEDITION && mp_expedition_disgrace_months() == 12,
        "the option of the lobby off: a second expedition strikes like the first");
    // the option on: Rome takes the province back, no winner
    if (!mp_savegame_read(landed)) {
        return 2;
    }
    int punishment_before[2] = { punishment_laurels(0), punishment_laurels(1) };
    mp_caesar_add_wrath(1, MP_CAESAR_WRATH_MAX - mp_caesar_wrath());
    run_to_next_month_fast();
    printf("a second expedition, the option on: over %d, winner %d, Rome %d, scores %d and %d\n", mp_endgame_is_over(),
        mp_endgame_winner(), mp_endgame_rome_took_province(), mp_endgame_score(0), mp_endgame_score(1));
    CHECK(mp_endgame_is_over() && mp_endgame_winner() == -1 && mp_endgame_rome_took_province() &&
        mp_expedition_count() == 1 && mp_expedition_soldiers_landed(0) == 32 &&
        punishment_laurels(0) == punishment_before[0] && punishment_laurels(1) == punishment_before[1] &&
        mp_endgame_score(0) == mp_caesar_laurels(0) / 10 && mp_endgame_score(1) == mp_caesar_laurels(1) / 10,
        "the option of the lobby on: a second expedition ends the game, without winner");
    if (!mp_savegame_write(landed) || !mp_savegame_read(landed)) {
        return 2;
    }
    CHECK(mp_endgame_is_over() && mp_endgame_winner() == -1 && mp_endgame_rome_took_province(), "(and saved)");

    // a game saved before the expedition: none sent, no disgrace
    if (!mp_savegame_read(landed) || !remove_saved_piece(landed, "mp_expedition") || !mp_savegame_read(landed)) {
        printf("Unable to write an older saved game\n");
        return 2;
    }
    CHECK(mp_expedition_count() == 0 && !mp_expedition_disgrace_months() &&
        mp_expedition_state_of(0) == MP_EXPEDITION_NONE, "an older saved game: no expedition was sent");
    remove(landed);

    // three players: player 1 innocent, players 2 and 3 culprits, 60 and 40 % of the wrath; easy: the same soldiers
    player_context_switch(0);
    player_context_set_num_players(1);
    if (!start_tradegame(file, 3, 0, "expedition")) {
        return 2;
    }
    CHECK(game_rules_difficulty() == DIFFICULTY_EASY, "(three players, easy)");
    int laurels[3] = { 500, 1000, 1000 };
    for (int p = 0; p < 3; p++) {
        mp_caesar_add_laurels(p, MP_LAURELS_GIFTS, laurels[p]);
    }
    mp_caesar_add_wrath(1, 600);
    mp_caesar_add_wrath(2, 400);
    // player 1 may create only 20 more figures
    int kept = 0;
    for (int i = 1; i < MAX_FIGURES; i++) {
        figure *f = figure_get(i);
        if (!f->state && kept++ >= 20) {
            f->state = FIGURE_STATE_ALIVE; // a figure without type, which does nothing
        }
    }
    run_to_next_month_fast();
    // no war: the gauge lost 3 points before the expedition, each share in proportion
    int share2 = 600 * 970 / 1000, share3 = 400 * 970 / 1000;
    int main_soldiers = 96 + 48 * share2 / (share2 + share3);
    int expected3 = 32 + (main_soldiers - 32) * share3 / share2;
    int lost[3];
    for (int p = 0; p < 3; p++) {
        lost[p] = -punishment_laurels(p);
    }
    printf("three players: %d, %d and %d soldiers (expected 20 at most, %d, %d); laurels lost %d, %d, %d tenths\n",
        caesar_soldiers(0, 0, 0), caesar_soldiers(1, 0, 0), caesar_soldiers(2, 0, 0), main_soldiers, expected3,
        lost[0], lost[1], lost[2]);
    CHECK(caesar_soldiers(1, 0, 0) == main_soldiers && caesar_soldiers(2, 0, 0) == expected3 && main_soldiers == 124 &&
        expected3 == 93, "the main culprit by his share, 124; the other by his share against the main one, 93");
    CHECK(lost[1] == (mp_caesar_laurels(1) + lost[1]) / 4 &&
        lost[2] == (mp_caesar_laurels(2) + lost[2]) * 25 * share3 / (100 * share2) && lost[0] == 0,
        "the main culprit loses 25 %, the other 25 % of his share against the main one, the innocent nothing");
    CHECK(caesar_soldiers(0, 0, 0) > 0 && caesar_soldiers(0, 0, 0) <= 20 && !figure_get(0)->state &&
        !figure_get(0)->formation_id && !figure_get(0)->action_state,
        "a city may not create more figures: fewer soldiers land");
    for (int i = 1; i < MAX_FIGURES; i++) {
        figure *f = figure_get(i);
        if (f->state == FIGURE_STATE_ALIVE && !f->type) {
            f->state = 0;
        }
    }
    // nobody fights them: they all leave after 12 months
    letters = mp_caesar_num_letters();
    months = 0;
    while (mp_expedition_state_of(0) == MP_EXPEDITION_IN_CITY && months < 14) {
        run_to_next_month_fast();
        months++;
    }
    RUN_UNTIL(mp_expedition_state_of(0) != MP_EXPEDITION_LEAVING && mp_expedition_state_of(1) !=
        MP_EXPEDITION_LEAVING && mp_expedition_state_of(2) != MP_EXPEDITION_LEAVING, 50, 6 * 800);
    printf("three players: the expeditions leave after %d months, gone %d ticks later; states %d %d %d\n", months,
        war_ticks, mp_expedition_state_of(0), mp_expedition_state_of(1), mp_expedition_state_of(2));
    CHECK(months == 12 && mp_expedition_state_of(0) == MP_EXPEDITION_LEFT &&
        mp_expedition_state_of(1) == MP_EXPEDITION_LEFT && mp_expedition_state_of(2) == MP_EXPEDITION_LEFT &&
        -punishment_laurels(1) == lost[1] && -punishment_laurels(0) == lost[0],
        "no city destroyed them: they all leave after 12 months, nothing gained");
    CHECK(mp_caesar_num_letters() == letters + 1 && last_letter_type() == MP_CAESAR_LETTER_EXPEDITION_LEFT,
        "a letter of Caesar to the player whose city they leave");

    // alone on the prepared map: no war, the gauge cannot rise
    player_context_switch(0);
    player_context_set_num_players(1);
    if (!mp_mapgen_create_prepared(file, 1, 0)) {
        return 2;
    }
    set_caesar_rules(GAME_END_CAESAR, 1000);
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_BRUTAL, 0);
    for (int m = 0; m < 3; m++) {
        run_to_next_month_fast();
    }
    CHECK(mp_war_status(0, 1) == MP_WAR_PEACE && mp_caesar_wrath() == 0 && mp_expedition_count() == 0 &&
        !caesar_soldiers(0, 0, 0), "alone: no war, no wrath, no expedition");
    // forced (tests): the disgrace halves the laurels of the notes for 12 months, then stops
    if (!load("valentia57.sav")) {
        return 2;
    }
    set_caesar_rules(GAME_END_CAESAR, 1000);
    run_to_next_month_fast();
    mp_caesar_add_wrath(0, MP_CAESAR_WRATH_MAX);
    run_to_next_month_fast();
    printf("alone in Valentia, the gauge forced to 100: %d soldiers of Caesar, disgrace %d months\n",
        caesar_soldiers(0, 0, 0), mp_expedition_disgrace_months());
    CHECK(mp_expedition_count() == 1 && caesar_soldiers(0, 0, 0) == 144, "(the expedition lands in Valentia)");
    int gained = 0, expected = 0, full_gained = 0, full_expected = 0;
    int halved = notes_laurels_months(0, MP_CAESAR_DISGRACE_MONTHS, 1, &gained, &expected);
    int disgrace_after = mp_expedition_disgrace_months();
    int full = notes_laurels_months(0, 3, 0, &full_gained, &full_expected);
    printf("laurels of the notes: %d tenths in the 12 months of disgrace (expected %d, halved), %d in the 3 months "
        "after (expected %d)\n", gained, expected, full_gained, full_expected);
    CHECK(halved && expected > 100 && disgrace_after == 0,
        "in disgrace, every monthly laurel of the notes is halved, for 12 months");
    CHECK(full && full_expected > 0, "then they are whole again");

    // a classic game: nothing
    options.multiplayer = 0;
    if (!load(file)) {
        return 2;
    }
    mp_caesar_add_wrath(0, MP_CAESAR_WRATH_MAX);
    for (int m = 0; m < 2; m++) {
        run_to_next_month_fast();
    }
    CHECK(!mp_caesar_is_active() && mp_expedition_count() == 0 && !mp_expedition_disgrace_months() &&
        !caesar_soldiers(0, 0, 0), "a classic game: no expedition");
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: Caesar's punitive expedition is not as designed" :
        "Identical: Caesar's punitive expedition is as designed");
    return failures ? 1 : 0;
}

// ---------- gatehouses, walls and the aqueduct of Caesar at war (M10.2, D-081) ----------

// A ring of walls of the player around the middle of his city, closed by two gatehouses on the main road of Caesar,
// west and east (2 x 2 tiles from (gate_x, gate_y), the road on their north row)
typedef struct {
    int west, east, north, south;
    int gate_x[2], gate_y;
} siege_ring;

static int is_wall(int x, int y)
{
    return map_terrain_is(map_grid_offset(x, y), TERRAIN_WALL);
}

static int gate_standing(const siege_ring *r, int i)
{
    int id = map_building_at(map_grid_offset(r->gate_x[i], r->gate_y));
    const building *b = building_get(id);
    return id > 0 && b->type == BUILDING_GATEHOUSE && b->state == BUILDING_STATE_IN_USE;
}

static int build_siege_ring(int player_id, siege_ring *r)
{
    int cx, cy;
    mp_mapgen_city_center(player_id, &cx, &cy);
    r->west = cx - 14;
    r->east = cx + 4;
    r->north = cy - 6;
    r->south = cy + 6;
    r->gate_y = cy;
    r->gate_x[0] = r->west;
    r->gate_x[1] = r->east - 1;
    build_as(player_id, BUILDING_GATEHOUSE, r->gate_x[0], r->gate_y, r->gate_x[0], r->gate_y);
    build_as(player_id, BUILDING_GATEHOUSE, r->gate_x[1], r->gate_y, r->gate_x[1], r->gate_y);
    build_as(player_id, BUILDING_WALL, r->west, r->north, r->east, r->north);
    build_as(player_id, BUILDING_WALL, r->west, r->south, r->east, r->south);
    build_as(player_id, BUILDING_WALL, r->west, r->north + 1, r->west, r->gate_y - 1);
    build_as(player_id, BUILDING_WALL, r->west, r->gate_y + 2, r->west, r->south - 1);
    build_as(player_id, BUILDING_WALL, r->east, r->north + 1, r->east, r->gate_y - 1);
    build_as(player_id, BUILDING_WALL, r->east, r->gate_y + 2, r->east, r->south - 1);
    for (int t = 0; t < 50 && !(gate_standing(r, 0) && gate_standing(r, 1)); t++) {
        war_run(1); // the gatehouses come into use (building_update_state, once a day)
    }
    int closed = gate_standing(r, 0) && gate_standing(r, 1) &&
        BUILDING_OWNER(map_building_at(map_grid_offset(r->gate_x[0], r->gate_y))) == player_id;
    for (int x = r->west; x <= r->east; x++) {
        closed &= is_wall(x, r->north) && is_wall(x, r->south);
    }
    for (int y = r->north; y <= r->south; y++) {
        if (y != r->gate_y && y != r->gate_y + 1) {
            closed &= is_wall(r->west, y) && is_wall(r->east, y);
        }
    }
    return closed;
}

static int is_soldier_of(const figure *f, int formation_id)
{
    return f->state == FIGURE_STATE_ALIVE && f->formation_id == formation_id && figure_is_legion(f) &&
        f->action_state != FIGURE_ACTION_149_CORPSE;
}

typedef struct {
    int inside;      // soldiers inside the ring
    int on_walls;    // soldiers standing on a wall
    int stopped;     // soldiers stopped by a gatehouse, breaking it
} siege_count;

static void count_siege(int formation_id, const siege_ring *r, siege_count *c)
{
    memset(c, 0, sizeof(*c));
    int owner = FORMATION_OWNER(formation_id);
    for (int i = owner * MAX_FIGURES + 1; i < (owner + 1) * MAX_FIGURES; i++) {
        figure *f = figure_get(i);
        if (!is_soldier_of(f, formation_id)) {
            continue;
        }
        c->inside += f->x > r->west && f->x < r->east && f->y > r->north && f->y < r->south;
        c->on_walls += is_wall(f->x, f->y);
        c->stopped += f->direction == DIR_FIGURE_ATTACK && f->action_state != FIGURE_ACTION_150_ATTACK;
    }
}

typedef struct {
    int arrived;             // tick at which every soldier stood at the standard, 0 if never
    int first_stop;          // tick at which a soldier was first stopped by a gatehouse, 0 if never
    int fall;                // tick at which the west gatehouse fell, 0 if never
    int inside_before_fall;  // most soldiers inside the ring while the west gatehouse stood
    int on_walls;            // soldiers on a wall, summed over the ticks
    int stopped_ticks;       // ticks with a soldier stopped by a gatehouse
    int inside;              // soldiers inside at the end
    int soldiers;            // soldiers alive at the end
} siege_march;

// the legion marches to (x, y), tick by tick, until every soldier stands at the standard (at most max_ticks)
static void march(int formation_id, const siege_ring *r, int x, int y, int max_ticks, siege_march *s)
{
    memset(s, 0, sizeof(*s));
    city_action(FORMATION_OWNER(formation_id), MP_ACTION_LEGION_MOVE, formation_id, x, y);
    for (int t = 1; t <= max_ticks; t++) {
        war_run(1);
        siege_count c;
        count_siege(formation_id, r, &c);
        if (!s->fall && !gate_standing(r, 0)) {
            s->fall = t;
        }
        if (!s->fall && c.inside > s->inside_before_fall) {
            s->inside_before_fall = c.inside;
        }
        if (!s->first_stop && c.stopped) {
            s->first_stop = t;
        }
        s->on_walls += c.on_walls;
        s->stopped_ticks += c.stopped > 0;
        s->soldiers = alive_soldiers(formation_id);
        s->inside = c.inside;
        if (s->soldiers && soldiers_at_standard(formation_id) == s->soldiers) {
            s->arrived = t;
            break;
        }
    }
}

// the war of the gatehouses, from the start with the ring: the legion of player 1 breaks the west gatehouse of
// player 2 and walks in, then goes out to the north-east of the ring, around the east gatehouse
static int play_siege(int legion, const siege_ring *r, int show, siege_march *in, siege_march *around,
    uint64_t *checksum)
{
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); if (show) printf("%-74s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_BRUTAL, 0);
    int pulled_down = 0, caravans_taken = 0, killed = 0;
    mp_wrath_damage_done(0, 1, &pulled_down, &caravans_taken, &killed);
    march(legion, r, r->west + 8, r->gate_y + 3, 8000, in);
    int pulled_down_after = 0;
    mp_wrath_damage_done(0, 1, &pulled_down_after, &caravans_taken, &killed);
    CHECK(pulled_down_after > pulled_down, "Caesar counts the gatehouse pulled down in his wrath (D-084)");
    if (show) {
        printf("at war: a soldier stopped by the west gatehouse after %d ticks, which falls after %d ticks (%d ticks of "
            "attack); %d of %d soldiers inside after %d ticks\n", in->first_stop, in->fall,
            in->fall - in->first_stop, in->inside, in->soldiers, in->arrived);
    }
    CHECK(in->first_stop > 0 && in->fall > in->first_stop, "at war, the gatehouse of player 2 stops the legion of player 1");
    CHECK(in->inside_before_fall == 0, "no soldier of it inside the walls before the gatehouse falls");
    CHECK(in->on_walls == 0, "none ever stands on a wall");
    CHECK(in->arrived && in->inside == in->soldiers, "once it fell, they walk in: every soldier at the standard");
    if (show) {
        // the gatehouse stood on the road of Caesar: his road is back under it (D-085, tested by caesargate)
        int o = map_grid_offset(r->gate_x[0], r->gate_y);
        printf("under the fallen gatehouse, on the main road: %s, owner %d\n",
            map_terrain_is(o, TERRAIN_ROAD) ? "road" : map_terrain_is(o, TERRAIN_RUBBLE) ? "rubble" : "other",
            map_owner_get_claimed(o));
    }
    CHECK(gate_standing(r, 1), "the east gatehouse stands");
    march(legion, r, r->east + 6, r->north + 2, 4000, around);
    if (show) {
        printf("out of the ring by the broken gate and around it: %d ticks, %d ticks stopped by a gatehouse\n",
            around->arrived, around->stopped_ticks);
    }
    CHECK(around->arrived && around->stopped_ticks == 0 && gate_standing(r, 1),
        "with a way around, they go around the east gatehouse, which stands");
    CHECK(around->on_walls == 0, "still never on a wall");
    *checksum = mp_checksum_state();
#undef CHECK
    return failures;
}

// gatehouses and walls at war (M10.2, D-081): the gatehouses of a player are closed to the soldiers of the players
// fighting him, who go around them or break them as invaders; open to everyone else, in peace and during the notice
// of an honourable war; nobody walks over walls
static int command_wargate(const char *file)
{
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-74s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    int legion1, legion2, tx, ty;
    if (!start_war_game(file, &legion1, &legion2, &tx, &ty)) {
        printf("Unable to prepare the game\n");
        return 2;
    }
    siege_ring ring;
    CHECK(build_siege_ring(1, &ring), "player 2 closes the middle of his city: walls and two gatehouses");
    int soldiers = alive_soldiers(legion1);
    const char *start = "wargate-start.mpsav";
    if (!mp_savegame_write(start)) {
        printf("Unable to save\n");
        return 2;
    }
    int x = ring.west + 8, y = ring.gate_y + 3;

    // in peace, then during the notice of an honourable war: the gatehouses let the legion through
    siege_march peace;
    march(legion1, &ring, x, y, 6000, &peace);
    printf("in peace: the legion of player 1 inside the walls of player 2 after %d ticks, %d of %d soldiers\n",
        peace.arrived, peace.inside, soldiers);
    CHECK(peace.arrived && peace.inside == soldiers && peace.stopped_ticks == 0 && gate_standing(&ring, 0),
        "in peace, the legion of player 1 goes through the gatehouse of player 2");
    CHECK(peace.on_walls == 0, "never over his walls");
    if (!mp_savegame_read(start)) {
        return 2;
    }
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_HONOURABLE, 0);
    siege_march notice;
    march(legion1, &ring, x, y, 6000, &notice);
    printf("during the notice: inside after %d ticks, %d of %d soldiers, %d days before the fighting\n",
        notice.arrived, notice.inside, soldiers, mp_war_days_until_fighting(0, 1));
    CHECK(mp_war_status(0, 1) == MP_WAR_NOTICE && notice.arrived && notice.inside == soldiers &&
        notice.stopped_ticks == 0 && gate_standing(&ring, 0), "during the notice of an honourable war too");

    // at war: twice from the same start, the same siege
    siege_march in[2], around[2];
    uint64_t checksum[2];
    for (int run = 0; run < 2; run++) {
        if (!mp_savegame_read(start)) {
            return 2;
        }
        failures += play_siege(legion1, &ring, !run, &in[run], &around[run], &checksum[run]);
    }
    CHECK(checksum[0] == checksum[1] && in[0].fall == in[1].fall && around[0].arrived == around[1].arrived,
        "played again from the same start: the same siege, the same checksum");
    remove(start);
    player_context_switch(0);
    player_context_set_num_players(1);
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: the gatehouses and walls of the players at war are not as designed" :
        "Identical: the gatehouses and walls of the players at war are as designed");
    return failures ? 1 : 0;
}

// a caravan of the player on a tile of the gatehouse
static int caravan_on_gate(int player_id, int gate_x, int gate_y)
{
    for (int i = player_id * MAX_FIGURES + 1; i < (player_id + 1) * MAX_FIGURES; i++) {
        figure *f = figure_get(i);
        if (f->state == FIGURE_STATE_ALIVE && mp_trade_is_caravan(f) && f->x >= gate_x && f->x <= gate_x + 1 &&
            f->y >= gate_y && f->y <= gate_y + 1) {
            return 1;
        }
    }
    return 0;
}

// The gatehouses of a player at war let the neutral players through (M10.2, D-081): three players on map 1, players
// A and G fight; the legion of the neutral player N walks through the gatehouse of the ring of G, and his caravan
// through the gatehouse on the road to the warehouse of G
static int command_wargate3(const char *file)
{
    if (!start_tradegame(file, 3, 0, "wargate3")) {
        return 2;
    }
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-74s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    // G on the north coast, N inland on the same shore, A on the other shore
    int g = -1, n = -1, a = -1;
    for (int p = 0; p < 3; p++) {
        int cx, cy;
        mp_mapgen_city_center(p, &cx, &cy);
        if (cy > 150) {
            a = p;
        } else if (cx > 130) {
            g = p;
        } else {
            n = p;
        }
    }
    if (g < 0 || n < 0 || a < 0) {
        printf("Unexpected places of the cities\n");
        return 2;
    }
    int gx, gy, nx, ny;
    mp_mapgen_city_center(g, &gx, &gy);
    mp_mapgen_city_center(n, &nx, &ny);
    printf("player %d at war with player %d, who has gatehouses; player %d neutral\n", a + 1, g + 1, n + 1);
    int legion = make_legion(n, BUILDING_FORT_LEGIONARIES, nx - 10, ny + 4, 16);
    siege_ring ring;
    int ready = legion && build_siege_ring(g, &ring);
    // the warehouse of G at the end of a road of his own, east of the ring, a gatehouse on that road
    int road_x = gx + 10;
    build_as(g, BUILDING_ROAD, road_x, gy + 1, road_x, gy + 7);
    ready &= build_as(g, BUILDING_WAREHOUSE, road_x + 1, gy + 5, road_x + 1, gy + 5);
    int store = map_building_at(map_grid_offset(road_x + 1, gy + 5));
    ready &= build_as(g, BUILDING_GATEHOUSE, road_x, gy + 2, road_x, gy + 2);
    int seller_store = build_warehouse_off_main_road(n, 1);
    ready &= seller_store != 0;
    run_trace(50, 50, 0, 0); // in use
    int gate = map_building_at(map_grid_offset(road_x, gy + 2));
    ready &= building_get(gate)->type == BUILDING_GATEHOUSE && building_get(gate)->state == BUILDING_STATE_IN_USE;
    CHECK(ready, "a legion for the neutral player; gatehouses for the other");
    if (!ready) {
        player_context_switch(0);
        player_context_set_num_players(1);
        return 1;
    }
    city_action(a, MP_ACTION_DECLARE_WAR, g, MP_WAR_BRUTAL, 0);
    CHECK(mp_war_is_fighting(a, g) && !mp_war_is_fighting(n, g) && !mp_war_is_fighting(n, a),
        "a brutal war between two players, the third at peace with both");

    // the legion of the neutral player into the ring
    int soldiers = alive_soldiers(legion);
    siege_march s;
    march(legion, &ring, ring.west + 8, ring.gate_y + 3, 8000, &s);
    printf("the legion of player %d inside the walls of player %d after %d ticks: %d of %d soldiers, %d ticks stopped\n",
        n + 1, g + 1, s.arrived, s.inside, soldiers, s.stopped_ticks);
    CHECK(s.arrived && s.inside == soldiers && s.stopped_ticks == 0 && gate_standing(&ring, 0) &&
        gate_standing(&ring, 1), "the neutral legion goes through the gatehouses of a player at war");
    CHECK(s.on_walls == 0, "never over his walls");

    // the caravan of the neutral player through the gatehouse on the road to the warehouse
    set_stock(n, seller_store, RESOURCE_MARBLE, 8);
    city_action(n, MP_ACTION_PROPOSE_ROUTE, g, 1, 0);
    city_action(g, MP_ACTION_PROPOSE_ROUTE, n, 1, 0);
    city_action(g, MP_ACTION_SET_BUYS_FROM, n, RESOURCE_MARBLE, 1);
    int on_gate_ticks = 0, ticks = 0;
    for (; ticks < 8 * 16 * 50 && !stock_in(g, store, RESOURCE_MARBLE); ticks++) {
        war_run(1);
        on_gate_ticks += caravan_on_gate(n, road_x, gy + 2);
    }
    printf("the caravan of player %d delivers %d loads to player %d after %d ticks, %d ticks on his gatehouse\n",
        n + 1, stock_in(g, store, RESOURCE_MARBLE), g + 1, ticks, on_gate_ticks);
    CHECK(stock_in(g, store, RESOURCE_MARBLE) > 0 && on_gate_ticks > 0 &&
        building_get(gate)->state == BUILDING_STATE_IN_USE,
        "the caravan of the neutral player goes through his gatehouse and delivers");
    player_context_switch(0);
    player_context_set_num_players(1);
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: the gatehouses of a player at war stop the neutral players" :
        "Identical: the gatehouses of a player at war let the neutral players through");
    return failures ? 1 : 0;
}

// the tiles of a w x h area that are a road of Caesar: road, his, no building, no rubble, walkable
static int caesar_road_tiles(int x, int y, int w, int h)
{
    int count = 0;
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            int o = map_grid_offset(x + dx, y + dy);
            count += map_terrain_is(o, TERRAIN_ROAD) && !map_terrain_is(o, TERRAIN_BUILDING | TERRAIN_RUBBLE) &&
                map_building_at(o) == 0 && map_owner_get_claimed(o) == MAP_OWNER_CAESAR && map_routing_citizen_is_road(o);
        }
    }
    return count;
}

// the tiles of the area that Caesar claims
static int caesar_claims(int x, int y, int w, int h)
{
    int count = 0;
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            count += map_owner_get_claimed(map_grid_offset(x + dx, y + dy)) == MAP_OWNER_CAESAR;
        }
    }
    return count;
}

static void delete_building_at(int player_id, int x, int y)
{
    mp_command clear = { .type = MP_COMMAND_BUILD, .player_id = player_id, .args = { BUILDING_CLEAR_LAND, 0, x, y, x, y, 0, 0 } };
    mp_command_execute(&clear);
}

// a building on the road of Caesar gives it back when it disappears (M10.2, D-085): two players on map 1, player 2
// closes his city with walls and two gatehouses over the main road (D-081); the road of Caesar under them is still his
// while they stand, and comes back at once, indestructible and walkable, when the owner deletes them, when enemy soldiers
// pull one down, after a save and a load, and under a triumphal arch; a caravan of player 2 leaves his walled city
// again; the same deletion played twice gives the same checksum
static int command_caesargate(const char *file)
{
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-78s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    int legion1, legion2, tx, ty;
    if (!start_war_game(file, &legion1, &legion2, &tx, &ty)) {
        printf("Unable to prepare the game\n");
        return 2;
    }
    int cx, cy;
    mp_mapgen_city_center(1, &cx, &cy);
    // the gatehouses of the ring (build_siege_ring) and the road of Caesar that is under them
    int gx[2] = { cx - 14, cx + 3 }, gy = cy;
    int caesar_before[2];
    for (int g = 0; g < 2; g++) {
        caesar_before[g] = caesar_road_tiles(gx[g], gy, 2, 2);
    }
    printf("road of Caesar under the future gatehouses: %d and %d tiles\n", caesar_before[0], caesar_before[1]);
    siege_ring ring;
    CHECK(caesar_before[0] > 0 && caesar_before[1] > 0 && build_siege_ring(1, &ring) && ring.gate_x[0] == gx[0] &&
        ring.gate_x[1] == gx[1] && ring.gate_y == gy, "player 2 closes his city: two gatehouses over the road of Caesar");
    setup_trade(8, 0, 0);
    int gates_claim = 1, gates_road = 1;
    for (int g = 0; g < 2; g++) {
        gates_claim &= caesar_claims(gx[g], gy, 2, 2) == caesar_before[g];
        gates_road &= map_terrain_is(map_grid_offset(gx[g], gy), TERRAIN_ROAD) &&
            map_terrain_is(map_grid_offset(gx[g], gy), TERRAIN_GATEHOUSE);
    }
    CHECK(gates_claim && gates_road, "while the gatehouses stand, Caesar still claims his tiles under them");
    // the road under a building taken away (as the triumphal arch does on its sides) does not free the claim
    int under = map_grid_offset(gx[0], gy);
    map_owner_set_builder(1);
    map_terrain_remove(under, TERRAIN_ROAD);
    int claim_kept = map_owner_get_claimed(under) == MAP_OWNER_CAESAR;
    map_terrain_add(under, TERRAIN_ROAD);
    map_owner_set_builder(MAP_OWNER_NONE);
    CHECK(claim_kept && map_owner_get_claimed(under) == MAP_OWNER_CAESAR,
        "the road taken away under a building (triumphal arch): Caesar still claims the tile");
    // a triumphal arch for the third case, over the road of Caesar too, west of the warehouse
    int ax = cx - 11, ay = cy - 1;
    int arch_before = caesar_road_tiles(ax, ay, 3, 3);
    // the command refuses an arch the city has not earned, as the build menu does
    build_as(1, BUILDING_TRIUMPHAL_ARCH, ax, ay, ax, ay);
    run_trace(50, 50, 0, 0);
    int unearned = map_building_at(map_grid_offset(ax, ay));
    CHECK(!unearned || building_get(unearned)->type != BUILDING_TRIUMPHAL_ARCH,
        "a triumphal arch the city has not earned: the command refuses it, as the menu");
    player_context_switch(1);
    city_buildings_earn_triumphal_arch();
    player_context_switch(0);
    build_as(1, BUILDING_TRIUMPHAL_ARCH, ax, ay, ax, ay);
    run_trace(50, 50, 0, 0);
    int arch_id = map_building_at(map_grid_offset(ax, ay));
    int arch_standing = arch_id > 0 && building_get(arch_id)->type == BUILDING_TRIUMPHAL_ARCH;
    printf("triumphal arch over %d tiles of the road of Caesar: %s, he claims %d tiles\n", arch_before,
        arch_standing ? "built" : "not built", caesar_claims(ax, ay, 3, 3));
    const char *start = "caesargate-start.mpsav";
    if (!mp_savegame_write(start)) {
        printf("Unable to save\n");
        return 2;
    }

    // the owner deletes everything: twice from the same start, the second time from the saved game and played again
    uint64_t checksum[2] = { 0, 0 };
    for (int run = 0; run < 2; run++) {
        if (!mp_savegame_read(start)) {
            return 2;
        }
        int still = 1;
        for (int g = 0; g < 2; g++) {
            still &= caesar_claims(gx[g], gy, 2, 2) == caesar_before[g];
        }
        if (!run) {
            CHECK(still, "reloaded with the gatehouses standing: Caesar still claims the tiles under them");
        }
        for (int g = 0; g < 2; g++) {
            delete_building_at(1, gx[g], gy);
        }
        if (arch_standing) {
            delete_building_at(1, ax + 1, ay + 1);
        }
        run_trace(100, 100, 0, 0);
        int back = 1, nothing_else = 1;
        for (int g = 0; g < 2; g++) {
            back &= caesar_road_tiles(gx[g], gy, 2, 2) == caesar_before[g];
            nothing_else &= !map_terrain_is(map_grid_offset(gx[g], gy), TERRAIN_BUILDING | TERRAIN_GATEHOUSE | TERRAIN_RUBBLE);
        }
        if (arch_standing) {
            back &= caesar_road_tiles(ax, ay, 3, 3) == arch_before;
        }
        if (!run) {
            CHECK(back && nothing_else, "deleted by player 2: the road of Caesar is back under every one, no rubble");
        }
        // indestructible, as the rest of his road
        for (int q = 0; q < 2; q++) {
            for (int g = 0; g < 2; g++) {
                delete_building_at(q, gx[g], gy);
            }
        }
        int kept = caesar_road_tiles(gx[0], gy, 2, 2) == caesar_before[0] &&
            caesar_road_tiles(gx[1], gy, 2, 2) == caesar_before[1];
        if (!run) {
            CHECK(kept, "nobody can clear it");
        }
        // a caravan of player 2 leaves his walled city by the roads that came back
        city_action(0, MP_ACTION_PROPOSE_ROUTE, 1, 1, 0);
        city_action(1, MP_ACTION_PROPOSE_ROUTE, 0, 1, 0);
        int days = 0;
        for (; days < 120 && marble_of(0) < 8; days++) {
            run_trace(50, 50, 0, 0);
        }
        if (!run) {
            printf("player 1 has %d marble of player 2 after %d days\n", marble_of(0), days);
            CHECK(marble_of(0) == 8, "the caravan of player 2 walks out through the road of Caesar and delivers");
        }
        checksum[run] = mp_checksum_state();
    }
    CHECK(checksum[0] == checksum[1], "the same deletion played again from the saved game: the same checksum");

    // enemy soldiers pull a gatehouse down
    if (!mp_savegame_read(start)) {
        return 2;
    }
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_BRUTAL, 0);
    siege_march s;
    march(legion1, &ring, ring.west + 8, ring.gate_y - 3, 8000, &s); // the warehouse is south of the road
    printf("the legion of player 1 pulls the west gatehouse down after %d ticks (first stopped after %d, at the standard "
        "after %d, %d of %d inside)\n", s.fall, s.first_stop, s.arrived, s.inside, s.soldiers);
    CHECK(s.fall > 0 && !gate_standing(&ring, 0), "the soldiers of player 1 pull the west gatehouse of player 2 down");
    CHECK(caesar_road_tiles(gx[0], gy, 2, 2) == caesar_before[0],
        "the road of Caesar is back under the fallen gatehouse, no rubble");
    CHECK(gate_standing(&ring, 1) && caesar_claims(gx[1], gy, 2, 2) == caesar_before[1],
        "the east gatehouse stands on his claim");

    // collapse and fire: the west gatehouse burns, the east one and the arch collapse
    if (!mp_savegame_read(start)) {
        return 2;
    }
    player_context_switch(1);
    building_destroy_by_fire(building_get(map_building_at(map_grid_offset(gx[0], gy))));
    building_destroy_by_collapse(building_get(map_building_at(map_grid_offset(gx[1], gy))));
    if (arch_standing) {
        building_destroy_by_collapse(building_get(map_building_at(map_grid_offset(ax, ay))));
    }
    player_context_switch(0);
    CHECK(caesar_claims(gx[0], gy, 2, 2) == caesar_before[0] && caesar_claims(gx[1], gy, 2, 2) == caesar_before[1],
        "burnt and collapsed: Caesar still claims the tiles");
    run_trace(1000, 1000, 0, 0);
    int back = caesar_road_tiles(gx[0], gy, 2, 2) == caesar_before[0] && caesar_road_tiles(gx[1], gy, 2, 2) == caesar_before[1];
    if (arch_standing) {
        back &= caesar_road_tiles(ax, ay, 3, 3) == arch_before;
    }
    printf("west (burnt) %d, east (collapsed) %d, arch (collapsed) %d tiles of road of Caesar back\n",
        caesar_road_tiles(gx[0], gy, 2, 2), caesar_road_tiles(gx[1], gy, 2, 2), caesar_road_tiles(ax, ay, 3, 3));
    CHECK(back, "burnt and collapsed gatehouses and arch: his road is back, no ruin, no rubble");
    CHECK(arch_standing, "the triumphal arch could be built on the road of Caesar");
    remove(start);
    player_context_switch(0);
    player_context_set_num_players(1);
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: the road of Caesar is not given back" :
        "Identical: the road of Caesar is given back when what stood on it disappears");
    return failures ? 1 : 0;
}

// the tiles of the aqueduct of Caesar on the row of its end, from x to x + width - 1
static int caesar_aqueduct_tiles(int x, int y, int width)
{
    int count = 0;
    for (int i = 0; i < width; i++) {
        int o = map_grid_offset(x + i, y);
        count += map_terrain_is(o, TERRAIN_AQUEDUCT) && map_owner_get_claimed(o) == MAP_OWNER_CAESAR;
    }
    return count;
}

// a free tile next to a soldier of the legion at its standard, away from other aqueducts: 0 if none
static int tile_by_soldier(int formation_id, int *x, int *y)
{
    int owner = FORMATION_OWNER(formation_id);
    for (int i = owner * MAX_FIGURES + 1; i < (owner + 1) * MAX_FIGURES; i++) {
        figure *f = figure_get(i);
        if (!is_soldier_of(f, formation_id) || f->action_state != FIGURE_ACTION_84_SOLDIER_AT_STANDARD) {
            continue;
        }
        for (int dir = 0; dir < 8; dir++) {
            int o = f->grid_offset + map_grid_direction_delta(dir);
            int tx = map_grid_offset_to_x(o), ty = map_grid_offset_to_y(o);
            if (!map_terrain_is(o, TERRAIN_NOT_CLEAR) && !map_has_figure_at(o) &&
                !map_terrain_exists_tile_in_area_with_type(tx - 1, ty - 1, 3, TERRAIN_AQUEDUCT)) {
                *x = tx;
                *y = ty;
                return 1;
            }
        }
    }
    return 0;
}

// ticks of the aqueduct game: no engineer keeps the reservoir from collapsing
static void aqueduct_run(int ticks, int reservoir_x, int reservoir_y)
{
    for (int i = 0; i < ticks; i++) {
        war_run(1);
        int id = map_building_at(map_grid_offset(reservoir_x, reservoir_y));
        if (id > 0) {
            building_get(id)->damage_risk = 0;
        }
    }
}

typedef struct {
    int inland, attacker, legion;  // the player watered by Caesar, his enemy and the legion of the enemy
    int ax, ay;                    // the end of the aqueduct of Caesar
    int standard_x, standard_y;
} aqueduct_war;

typedef struct {
    int level_at_peace, level_at_break, level_after_cut, level_mended;
    int ticks_to_break;            // after the start of the fighting
    int broken;                    // tiles of Caesar broken by the soldiers
    uint64_t checksum;
} aqueduct_result;

#define AQUEDUCT_ROW 16 // tiles of the row of the aqueduct watched, from its end

// The aqueduct of Caesar at war, from the start: the player inland has his reservoir at the end of the aqueduct of
// Caesar; the legion of his enemy stands by it in peace, then during the notice of an honourable war, then fights
// and breaks it; peace signed, the player inland mends it with an aqueduct of his own
static int play_aqueduct_war(const aqueduct_war *w, int show, aqueduct_result *r)
{
    int failures = 0;
    int war_ticks = 0;
#define CHECK(condition, text) do { int ok_ = (condition); if (show) printf("%-74s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    int rx = w->ax - 2, ry = w->ay, level = 0;
    build_as(w->inland, BUILDING_DRAGGABLE_RESERVOIR, rx, ry, rx, ry);
    city_action(w->attacker, MP_ACTION_LEGION_MOVE, w->legion, w->standard_x, w->standard_y);
    int soldiers = alive_soldiers(w->legion);
    for (war_ticks = 0; war_ticks < 6000 && soldiers_at_standard(w->legion) < soldiers; war_ticks += 50) {
        aqueduct_run(50, rx, ry);
    }
    int tiles = caesar_aqueduct_tiles(w->ax, w->ay, AQUEDUCT_ROW);
    aqueduct_run(300, rx, ry);
    reservoir_at(rx, ry, &r->level_at_peace);
    if (show) {
        printf("in peace: the legion of player %d by the aqueduct of Caesar after %d ticks; the reservoir of player %d "
            "holds %d\n", w->attacker + 1, war_ticks, w->inland + 1, r->level_at_peace);
    }
    CHECK(soldiers_at_standard(w->legion) == soldiers && r->level_at_peace > 0,
        "the aqueduct of Caesar waters player 1, the legion of player 2 stands by it");
    CHECK(caesar_aqueduct_tiles(w->ax, w->ay, AQUEDUCT_ROW) == tiles, "in peace, it leaves the aqueduct alone");
    city_action(w->attacker, MP_ACTION_DECLARE_WAR, w->inland, MP_WAR_HONOURABLE, 0);
    aqueduct_run(300, rx, ry);
    CHECK(mp_war_status(w->inland, w->attacker) == MP_WAR_NOTICE &&
        caesar_aqueduct_tiles(w->ax, w->ay, AQUEDUCT_ROW) == tiles, "during the notice of an honourable war too");
    for (war_ticks = 0; war_ticks < MP_WAR_NOTICE_TICKS && !mp_war_is_fighting(w->inland, w->attacker); war_ticks++) {
        aqueduct_run(1, rx, ry);
    }
    for (war_ticks = 0; war_ticks < 1000 && caesar_aqueduct_tiles(w->ax, w->ay, AQUEDUCT_ROW) == tiles; war_ticks++) {
        aqueduct_run(1, rx, ry);
    }
    r->ticks_to_break = war_ticks;
    aqueduct_run(200, rx, ry);
    r->broken = tiles - caesar_aqueduct_tiles(w->ax, w->ay, AQUEDUCT_ROW);
    reservoir_at(rx, ry, &r->level_at_break);
    aqueduct_run(10 * 50, rx, ry);
    reservoir_at(rx, ry, &r->level_after_cut);
    int first_broken = -1, last_broken = -1, rubble = 1;
    for (int i = 0; i < AQUEDUCT_ROW; i++) {
        int o = map_grid_offset(w->ax + i, w->ay);
        if (!map_terrain_is(o, TERRAIN_AQUEDUCT)) {
            first_broken = first_broken < 0 ? w->ax + i : first_broken;
            last_broken = w->ax + i;
            rubble &= map_terrain_is(o, TERRAIN_RUBBLE) && map_owner_get_claimed(o) == MAP_OWNER_NONE;
        }
    }
    if (show) {
        printf("at war: the first tile of the aqueduct of Caesar falls %d ticks after the start of the fighting, %d "
            "tiles 200 ticks later (x %d to %d); the reservoir holds %d, 10 days later %d\n", r->ticks_to_break,
            r->broken, first_broken, last_broken, r->level_at_break, r->level_after_cut);
    }
    CHECK(r->broken > 0 && rubble, "at war, the soldiers of player 2 break the aqueduct of Caesar that waters player 1");
    CHECK(r->level_after_cut < r->level_at_break, "the reservoir of player 1 no longer fills: it empties");
    CHECK(caesar_aqueduct_tiles(w->ax + AQUEDUCT_ROW, w->ay, 1) == 1 && mp_war_is_fighting(w->inland, w->attacker),
        "the aqueduct beyond the soldiers stands");

    // peace, then the player inland mends the aqueduct with his own
    city_action(w->attacker, MP_ACTION_PROPOSE_PEACE, w->inland, 1, 0);
    city_action(w->inland, MP_ACTION_PROPOSE_PEACE, w->attacker, 1, 0);
    aqueduct_run(300, rx, ry);
    if (first_broken >= 0) {
        // the rubble first, as anywhere in the original game
        build_as(w->inland, BUILDING_CLEAR_LAND, first_broken, w->ay, last_broken, w->ay);
        build_as(w->inland, BUILDING_AQUEDUCT, first_broken, w->ay, last_broken, w->ay);
    }
    int mended = first_broken >= 0;
    for (int x = first_broken; mended && x <= last_broken; x++) {
        int o = map_grid_offset(x, w->ay);
        mended &= map_terrain_is(o, TERRAIN_AQUEDUCT) && map_owner_get_claimed(o) == w->inland;
    }
    reservoir_at(rx, ry, &level);
    aqueduct_run(10 * 50, rx, ry);
    reservoir_at(rx, ry, &r->level_mended);
    if (show) {
        printf("mended by player 1 in peace: the reservoir holds %d, 10 days later %d\n", level, r->level_mended);
    }
    CHECK(mended && r->level_mended > level, "in peace, player 1 mends it with an aqueduct of his own: water again");
    r->checksum = mp_checksum_state();
#undef CHECK
    return failures;
}

// the aqueduct of Caesar at war (M10.2, D-081, D-034, D-055): the soldiers of a player at war break the aqueduct of
// Caesar that waters his enemy, never in peace nor during a notice; the aqueduct that waters no enemy stays; the
// player cut off mends it with his own aqueduct; the same war twice gives the same checksum
static int command_waraqueduct(const char *file)
{
    int failures = 0;
    int war_ticks = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-74s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    int legion[2], tx, ty;
    if (!start_war_game(file, &legion[0], &legion[1], &tx, &ty)) {
        printf("Unable to prepare the game\n");
        return 2;
    }
    aqueduct_war w;
    w.inland = 0;
    while (w.inland < 2 && !mp_mapgen_caesar_aqueduct_end(w.inland, &w.ax, &w.ay)) {
        w.inland++;
    }
    if (w.inland != 0) {
        printf("Player 1 is not the one inland\n");
        return 2;
    }
    w.attacker = 1;
    w.legion = legion[1];
    w.standard_x = w.ax + 7;
    w.standard_y = w.ay + 1;
    printf("the aqueduct of Caesar ends at (%d, %d), by the city of player 1; the legion of player 2 stands at (%d, %d)\n",
        w.ax, w.ay, w.standard_x, w.standard_y);
    const char *start = "waraqueduct-start.mpsav";
    if (!mp_savegame_write(start)) {
        printf("Unable to save\n");
        return 2;
    }

    // an aqueduct of Caesar that waters no enemy (player 1 has no reservoir yet): the soldiers at war leave it
    city_action(1, MP_ACTION_LEGION_MOVE, w.legion, w.standard_x, w.standard_y);
    int soldiers = alive_soldiers(w.legion);
    RUN_UNTIL(soldiers_at_standard(w.legion) >= soldiers, 50, 6000);
    int tiles = caesar_aqueduct_tiles(w.ax, w.ay, AQUEDUCT_ROW);
    // and an aqueduct of player 1 himself, apart, by a soldier
    int own_x = 0, own_y = 0;
    int own = tile_by_soldier(w.legion, &own_x, &own_y);
    if (own) {
        build_as(0, BUILDING_AQUEDUCT, own_x, own_y, own_x, own_y);
    }
    int own_offset = map_grid_offset(own_x, own_y);
    own &= map_terrain_is(own_offset, TERRAIN_AQUEDUCT) && map_owner_get_claimed(own_offset) == 0;
    city_action(1, MP_ACTION_DECLARE_WAR, 0, MP_WAR_BRUTAL, 0);
    war_run(500);
    CHECK(tiles == AQUEDUCT_ROW && caesar_aqueduct_tiles(w.ax, w.ay, AQUEDUCT_ROW) == tiles &&
        soldiers_at_standard(w.legion) == soldiers,
        "at war, the soldiers of player 2 leave the aqueduct of Caesar that waters nobody");
    printf("an aqueduct of player 1 at (%d, %d), by a soldier of player 2\n", own_x, own_y);
    CHECK(own && !map_terrain_is(own_offset, TERRAIN_AQUEDUCT) && map_terrain_is(own_offset, TERRAIN_RUBBLE),
        "but they break an aqueduct of player 1 himself");
    build_as(0, BUILDING_DRAGGABLE_RESERVOIR, w.ax - 2, w.ay, w.ax - 2, w.ay);
    RUN_UNTIL(caesar_aqueduct_tiles(w.ax, w.ay, AQUEDUCT_ROW) < tiles, 1, 1000);
    printf("player 1 joins a reservoir to it: the first tile falls %d ticks later\n", war_ticks);
    CHECK(caesar_aqueduct_tiles(w.ax, w.ay, AQUEDUCT_ROW) < tiles, "once it waters player 1, they break it");

    // in peace, during the notice, then at war; twice from the same start
    aqueduct_result result[2];
    for (int run = 0; run < 2; run++) {
        if (!mp_savegame_read(start)) {
            return 2;
        }
        failures += play_aqueduct_war(&w, !run, &result[run]);
    }
    CHECK(result[0].checksum == result[1].checksum && result[0].ticks_to_break == result[1].ticks_to_break &&
        result[0].broken == result[1].broken, "played again from the same start: the same war, the same checksum");
    remove(start);
    player_context_switch(0);
    player_context_set_num_players(1);
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: the aqueduct of Caesar at war is not as designed" :
        "Identical: the aqueduct of Caesar at war is as designed");
    return failures ? 1 : 0;
}

// ---------- the order "attack" of a legion (M10.2, D-087) ----------

// the area around (x, y): buildings by owner (1 to 4, C for Caesar), soldiers of player 1 (s), roads, walls, water
static void show_attack_area(int x, int y, int r)
{
    for (int yy = y - r; yy <= y + r; yy++) {
        printf("%4d ", yy);
        for (int xx = x - r; xx <= x + r; xx++) {
            if (!map_grid_is_inside(xx, yy, 1)) {
                putchar(' ');
                continue;
            }
            int o = map_grid_offset(xx, yy);
            char c = '.';
            int soldier = 0;
            for (int id = map_figure_at(o), guard = 0; id > 0 && guard < 50; guard++) {
                figure *f = figure_get(id);
                soldier |= figure_is_legion(f) && FIGURE_OWNER(id) == 0;
                id = f->next_figure_id_on_same_tile;
            }
            int b = map_building_at(o);
            if (soldier) {
                c = 's';
            } else if (b > 0 && building_get(b)->state == BUILDING_STATE_IN_USE) {
                c = BUILDING_IS_CAESAR(b) ? 'C' : (char) ('1' + BUILDING_OWNER(b));
            } else if (map_terrain_is(o, TERRAIN_WALL)) {
                c = '#';
            } else if (map_terrain_is(o, TERRAIN_ROAD)) {
                c = 'R';
            } else if (map_terrain_is(o, TERRAIN_RUBBLE)) {
                c = ',';
            } else if (map_terrain_is(o, TERRAIN_WATER)) {
                c = 'W';
            } else if (map_terrain_is(o, TERRAIN_TREE | TERRAIN_ROCK)) {
                c = 'T';
            } else if (map_terrain_is(o, TERRAIN_AQUEDUCT)) {
                c = 'A';
            }
            if (c == '.' && mp_territory_owner(o) >= 0) {
                c = (char) ('a' + mp_territory_owner(o));
            }
            if (xx == x && yy == y && (c == '.' || (c >= 'a' && c <= 'd'))) {
                c = '+';
            }
            putchar(c);
        }
        putchar('\n');
    }
}

static int soldiers_at_rest(int formation_id)
{
    int count = 0;
    int owner = FORMATION_OWNER(formation_id);
    for (int i = owner * MAX_FIGURES + 1; i < (owner + 1) * MAX_FIGURES; i++) {
        figure *f = figure_get(i);
        count += f->state == FIGURE_STATE_ALIVE && f->formation_id == formation_id && figure_is_legion(f) &&
            f->action_state == FIGURE_ACTION_80_SOLDIER_AT_REST;
    }
    return count;
}

// the larger of the two distances from the tile to the nearest tile of the building (as mp/assault.c)
static int distance_to_footprint(const building *b, int x, int y)
{
    int size = b->size > 0 ? b->size : 1;
    int dx = x < b->x ? b->x - x : (x >= b->x + size ? x - (b->x + size - 1) : 0);
    int dy = y < b->y ? b->y - y : (y >= b->y + size ? y - (b->y + size - 1) : 0);
    return dx > dy ? dx : dy;
}

// buildings of the player in use at most (`within`) or more than `distance` tiles from (x, y)
static int buildings_around(int player_id, int x, int y, int distance, int within)
{
    int count = 0;
    for (int i = player_id * MAX_BUILDINGS + 1; i < (player_id + 1) * MAX_BUILDINGS; i++) {
        const building *b = building_get(i);
        if (b->state == BUILDING_STATE_IN_USE) {
            int d = distance_to_footprint(b, x, y);
            count += within ? d <= distance : d > distance;
        }
    }
    return count;
}

// the buildings of player 2 the legion of player 1 attacks: a quarter around a statue (the target), and two statues
// far from it; *x, *y: the statue
static int build_attack_quarter(int *x, int *y)
{
    int x1, y1;
    mp_mapgen_city_center(1, &x1, &y1);
    *x = x1 - 13;
    *y = y1 - 8;
    int ok = build_as(1, BUILDING_SMALL_STATUE, *x, *y, *x, *y);
    ok &= build_as(1, BUILDING_SMALL_STATUE, *x - 4, *y - 3, *x - 4, *y - 3);
    ok &= build_as(1, BUILDING_WELL, *x + 4, *y - 4, *x + 4, *y - 4);
    ok &= build_as(1, BUILDING_PREFECTURE, *x - 6, *y + 3, *x - 6, *y + 3);
    ok &= build_as(1, BUILDING_SMALL_STATUE, *x + 6, *y + 4, *x + 6, *y + 4);
    ok &= build_as(1, BUILDING_MEDIUM_STATUE, *x - 3, *y - 7, *x - 3, *y - 7);
    // far from it
    ok &= build_as(1, BUILDING_SMALL_STATUE, x1 + 10, y1 - 10, x1 + 10, y1 - 10);
    ok &= build_as(1, BUILDING_SMALL_STATUE, x1 - 5, y1 + 14, x1 - 5, y1 + 14);
    war_run(50); // in use from the next update of the states
    return ok;
}

typedef struct {
    int target_x, target_y;     // the statue: the centre of the radius
    int order_x, order_y;       // the tile picked
    int ordered;
    int first_building;         // the first building the legion goes to
    int targets;                // the buildings it went to
    int targets_outside;        // of them, outside the radius
    int inside_before, far_before;
    int left_inside, down_far;  // at the end: buildings of player 2 within the radius, and far ones pulled down
    int ticks_first_down, ticks_done;
    int soldiers_at_end, at_fort_at_end;
    uint64_t checksum;
} attack_result;

// the legion of player 1, at war, attacks the tile picked; runs until its order ends
static void play_attack(int legion, attack_result *r, int show)
{
    int far = MP_ASSAULT_RADIUS + 6;
    r->inside_before = buildings_around(1, r->target_x, r->target_y, MP_ASSAULT_RADIUS, 1);
    r->far_before = buildings_around(1, r->target_x, r->target_y, far, 0);
    int all_before = buildings_of(1);
    city_action(0, MP_ACTION_LEGION_ATTACK, legion, r->order_x, r->order_y);
    r->ordered = mp_assault_is_active(legion) && mp_assault_enemy(legion) == 1;
    r->first_building = mp_assault_building(legion);
    r->targets = r->first_building ? 1 : 0;
    r->targets_outside = 0;
    r->ticks_first_down = 0;
    int last = r->first_building;
    int ticks = 0;
    for (; ticks < 20000 && mp_assault_is_active(legion); ticks++) {
        war_run(1);
        if (!r->ticks_first_down && buildings_of(1) < all_before) {
            r->ticks_first_down = ticks + 1;
        }
        int b = mp_assault_building(legion);
        if (b && b != last) {
            r->targets++;
            r->targets_outside += distance_to_footprint(building_get(b), r->target_x, r->target_y) > MP_ASSAULT_RADIUS;
            if (show) {
                printf("  tick %5d: on to building %d (type %d), %d buildings of player 2 down\n", ticks + 1, b,
                    building_get(b)->type, all_before - buildings_of(1));
            }
        }
        last = b ? b : last;
    }
    r->ticks_done = ticks;
    r->left_inside = buildings_around(1, r->target_x, r->target_y, MP_ASSAULT_RADIUS, 1);
    r->down_far = r->far_before - buildings_around(1, r->target_x, r->target_y, far, 0);
    war_run(300); // the legion stays where its order ended
    r->soldiers_at_end = soldiers_at_standard(legion);
    r->at_fort_at_end = formation_get(legion)->is_at_fort;
    r->checksum = mp_checksum_state();
}

// An attack in a game of three players: the legion of player 1 fights player 2; the city of player 3, neutral, is
// refused, that of player 2 is not
static int attack_neutral(const char *file)
{
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-76s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    player_context_switch(0);
    player_context_set_num_players(1);
    if (!start_tradegame(file, 3, 0, "warattack3")) {
        return 1;
    }
    int cx[3], cy[3];
    for (int p = 0; p < 3; p++) {
        mp_mapgen_city_center(p, &cx[p], &cy[p]);
    }
    int legion = make_legion(0, BUILDING_FORT_LEGIONARIES, cx[0] - 10, cy[0] - 8, 8);
    int ok = legion && build_as(1, BUILDING_SMALL_STATUE, cx[1] - 4, cy[1] - 4, cx[1] - 4, cy[1] - 4) &&
        build_as(2, BUILDING_SMALL_STATUE, cx[2] - 4, cy[2] - 4, cx[2] - 4, cy[2] - 4);
    war_run(400);
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_BRUTAL, 0);
    CHECK(ok && mp_war_is_fighting(0, 1) && !mp_war_is_fighting(0, 2), "three players: 1 fights 2, 3 is neutral");
    uint64_t before = mp_checksum_state();
    city_action(0, MP_ACTION_LEGION_ATTACK, legion, cx[2] - 4, cy[2] - 4);
    CHECK(!mp_assault_is_active(legion) && mp_checksum_state() == before &&
        mp_assault_enemy_at(0, cx[2] - 4, cy[2] - 4) < 0, "a target in the city of the neutral player: refused");
    city_action(0, MP_ACTION_LEGION_ATTACK, legion, cx[1] - 4, cy[1] - 4);
    CHECK(mp_assault_is_active(legion) && mp_assault_enemy(legion) == 1,
        "a target in the city of the enemy: the legion goes");
    // player 1 fights player 3 too: at peace with player 2, his legion stays out, but its attack on player 2 ends
    city_action(0, MP_ACTION_DECLARE_WAR, 2, MP_WAR_BRUTAL, 0);
    war_run(100);
    city_action(0, MP_ACTION_PROPOSE_PEACE, 1, 1, 0);
    city_action(1, MP_ACTION_PROPOSE_PEACE, 0, 1, 0);
    war_run(1);
    CHECK(mp_war_status(0, 1) == MP_WAR_PEACE && mp_war_is_fighting(0, 2) && !formation_get(legion)->is_at_fort &&
        !mp_assault_is_active(legion), "peace with the player attacked, war with another: the attack ends");
    player_context_switch(0);
    player_context_set_num_players(1);
#undef CHECK
    return failures;
}

// The order "attack" of a legion (M10.2, D-087): two players on map 1; refused at peace, during a notice, for the
// legion of another player, on one's own city or where no building of the enemy stands within the radius. At war, the
// legion of player 1 marches to the statue picked, pulls it down, moves on from building to building of player 2
// within the radius until none is left; peace or another order end it; saved and read back in the middle of the
// attack, the game goes on the same; played twice, the same checksum; refused for a neutral player; nothing in a
// classic game
static int command_warattack(const char *file)
{
    int failures = 0;
    int war_ticks = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-76s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    int legion1, legion2, tx, ty;
    if (!start_war_game(file, &legion1, &legion2, &tx, &ty)) {
        printf("Unable to prepare the game\n");
        return 2;
    }
    attack_result base;
    memset(&base, 0, sizeof(base));
    CHECK(build_attack_quarter(&base.target_x, &base.target_y),
        "player 2 builds a quarter of statues, a well and a prefecture");
    base.order_x = base.target_x;
    base.order_y = base.target_y;
    int x0, y0, x1, y1;
    mp_mapgen_city_center(0, &x0, &y0);
    mp_mapgen_city_center(1, &x1, &y1);
    int statue = map_building_at(map_grid_offset(base.target_x, base.target_y));
    int inside = buildings_around(1, base.target_x, base.target_y, MP_ASSAULT_RADIUS, 1);
    int far = buildings_around(1, base.target_x, base.target_y, MP_ASSAULT_RADIUS + 6, 0);
    printf("the statue picked at (%d, %d): %d buildings of player 2 within %d tiles, %d farther than %d, %d in all\n",
        base.target_x, base.target_y, inside, MP_ASSAULT_RADIUS, far, MP_ASSAULT_RADIUS + 6, buildings_of(1));
    if (getenv("SHOW_ATTACK")) {
        show_attack_area(base.target_x, base.target_y, 22);
    }
    const char *start = "warattack-start.mpsav";
    if (!mp_savegame_write(start)) {
        printf("Unable to save\n");
        return 2;
    }

    // ---- refused: at peace, during a notice, for another player's legion, on one's own city, on empty land ----
    city_action(0, MP_ACTION_LEGION_ATTACK, legion1, base.target_x, base.target_y);
    war_run(1);
    CHECK(!mp_assault_is_active(legion1) && formation_get(legion1)->is_at_fort &&
        mp_assault_enemy_at(0, base.target_x, base.target_y) < 0,
        "at peace: the order is refused, the legion stays in its fort");
    CHECK(!mp_assault_is_offered(legion1), "(and not offered in the legion window)");
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_HONOURABLE, 0);
    city_action(0, MP_ACTION_LEGION_ATTACK, legion1, base.target_x, base.target_y);
    war_run(1);
    CHECK(mp_war_status(0, 1) == MP_WAR_NOTICE && !mp_assault_is_active(legion1) && formation_get(legion1)->is_at_fort,
        "during the notice of an honourable war: refused");
    if (!mp_savegame_read(start)) {
        return 2;
    }
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_BRUTAL, 0);
    player_context_switch(0);
    CHECK(mp_assault_is_offered(legion1), "at war: offered in the legion window");
    uint64_t before = mp_checksum_state();
    city_action(0, MP_ACTION_LEGION_ATTACK, legion2, base.target_x, base.target_y);
    city_action(1, MP_ACTION_LEGION_ATTACK, legion1, base.target_x, base.target_y);
    CHECK(!mp_assault_is_active(legion2) && !mp_assault_is_active(legion1) && mp_checksum_state() == before,
        "the legion of another player: refused, nothing changes");
    city_action(1, MP_ACTION_LEGION_ATTACK, legion2, base.target_x, base.target_y);
    city_action(0, MP_ACTION_LEGION_ATTACK, legion1, x0 - 10, y0 - 4);
    CHECK(!mp_assault_is_active(legion2) && !mp_assault_is_active(legion1) && mp_checksum_state() == before,
        "a target in one's own city: refused");
    // a tile of his land, south of his city, with none of his buildings within the radius
    int empty_x = 0, empty_y = 0;
    for (int y = y1 + 10; y <= y1 + 25 && !empty_x; y++) {
        for (int x = x1 - 20; x <= x1 + 20 && !empty_x; x++) {
            if (mp_territory_owner(map_grid_offset(x, y)) == 1 && !map_terrain_is(map_grid_offset(x, y), TERRAIN_NOT_CLEAR) &&
                buildings_around(1, x, y, MP_ASSAULT_RADIUS, 1) == 0) {
                empty_x = x;
                empty_y = y;
            }
        }
    }
    int empty_owner = mp_territory_owner(map_grid_offset(empty_x, empty_y));
    city_action(0, MP_ACTION_LEGION_ATTACK, legion1, empty_x, empty_y);
    printf("land of player %d at (%d, %d), no building of his within %d tiles\n", empty_owner + 1, empty_x, empty_y,
        MP_ASSAULT_RADIUS);
    CHECK(empty_owner == 1 && !mp_assault_is_active(legion1) && mp_checksum_state() == before,
        "land of the enemy with none of his buildings within the radius: refused");
    CHECK(mp_assault_enemy_at(0, empty_x, empty_y) < 0 && mp_assault_enemy_at(0, base.target_x, base.target_y) == 1,
        "(the pick of the target says so too)");

    // ---- at war: the attack, played twice from the same start ----
    attack_result run[2];
    for (int i = 0; i < 2; i++) {
        if (!mp_savegame_read(start)) {
            return 2;
        }
        run[i] = base;
        city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_BRUTAL, 0);
        play_attack(legion1, &run[i], !i);
    }
    attack_result *r = &run[0];
    printf("the order: first building %d; %d buildings gone to; first building of player 2 down after %d ticks, "
        "order done after %d ticks\n", r->first_building, r->targets, r->ticks_first_down, r->ticks_done);
    printf("buildings of player 2 within %d tiles: %d before, %d after; farther than %d: %d of %d down\n",
        MP_ASSAULT_RADIUS, r->inside_before, r->left_inside, MP_ASSAULT_RADIUS + 6, r->down_far, r->far_before);
    CHECK(r->ordered && r->first_building == statue,
        "at war, the order is given: the legion goes first to the statue picked");
    CHECK(r->targets >= 4 && r->targets_outside == 0, "it moves on from building to building, all within the radius");
    CHECK(r->inside_before == inside && r->left_inside == 0 && r->ticks_done < 20000,
        "until no building of player 2 is left within the radius: the order ends");
    CHECK(r->far_before == far && r->down_far == 0, "the buildings far from the target stand");
    CHECK(!r->at_fort_at_end && r->soldiers_at_end == alive_soldiers(legion1) && r->soldiers_at_end > 0,
        "the legion stays where it is, its soldiers with the standard");
    CHECK(run[1].checksum == run[0].checksum && run[1].ticks_done == run[0].ticks_done &&
        run[1].targets == run[0].targets, "played again from the same start: the same attack, the same checksum");

    // ---- an order on a tile of his land, saved and read back in the middle of the attack ----
    if (!mp_savegame_read(start)) {
        return 2;
    }
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_BRUTAL, 0);
    int tile_x = base.target_x + 1, tile_y = base.target_y + 2;
    city_action(0, MP_ACTION_LEGION_ATTACK, legion1, tile_x, tile_y);
    CHECK(map_building_at(map_grid_offset(tile_x, tile_y)) == 0 && mp_assault_is_active(legion1) &&
        mp_assault_building(legion1) == statue, "on a tile of his land: the legion goes to the nearest building");
    int all = buildings_of(1);
    RUN_UNTIL(buildings_of(1) <= all - 2, 10, 8000);
    const char *middle = "warattack-middle.mpsav";
    int building_at_save = mp_assault_building(legion1);
    CHECK(mp_assault_is_active(legion1) && mp_savegame_write(middle), "(saved in the middle of the attack)");
    static uint64_t continued[400];
    for (int i = 0; i < 400; i++) {
        war_run(5);
        continued[i] = mp_checksum_state();
    }
    int continued_left = buildings_of(1);
    int continued_active = mp_assault_is_active(legion1);
    CHECK(mp_savegame_read(middle), "(read back)");
    CHECK(mp_assault_is_active(legion1) && mp_assault_enemy(legion1) == 1 &&
        mp_assault_building(legion1) == building_at_save, "the saved game keeps the order and its target");
    int same = 1;
    for (int i = 0; i < 400; i++) {
        war_run(5);
        same &= mp_checksum_state() == continued[i];
    }
    printf("2000 ticks later: %d buildings of player 2 left, order active %d (continued) / %d, %d (read back)\n",
        continued_left, continued_active, buildings_of(1), mp_assault_is_active(legion1));
    CHECK(same && buildings_of(1) == continued_left,
        "the game read back goes on the same: the same checksum every 5 ticks");
    CHECK(mp_savegame_read(middle) && mp_assault_is_active(legion1) && remove_saved_piece(middle, "mp_assault") &&
        mp_savegame_read(middle) && !mp_assault_is_active(legion1),
        "an older saved game, without the piece, loads without order");
    remove(middle);

    // ---- peace ends the order; the legion goes home ----
    if (!mp_savegame_read(start)) {
        return 2;
    }
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_BRUTAL, 0);
    city_action(0, MP_ACTION_LEGION_ATTACK, legion1, base.target_x, base.target_y);
    all = buildings_of(1);
    RUN_UNTIL(buildings_of(1) < all, 10, 8000);
    city_action(0, MP_ACTION_PROPOSE_PEACE, 1, 1, 0);
    city_action(1, MP_ACTION_PROPOSE_PEACE, 0, 1, 0);
    war_run(1);
    int left = buildings_of(1);
    CHECK(mp_war_status(0, 1) == MP_WAR_PEACE && !mp_assault_is_active(legion1), "peace signed: the order ends");
    RUN_UNTIL(soldiers_at_rest(legion1) == alive_soldiers(legion1), 50, 6000);
    printf("the legion is back at its fort %d ticks after the peace\n", war_ticks);
    CHECK(formation_get(legion1)->is_at_fort && soldiers_at_rest(legion1) == alive_soldiers(legion1) &&
        buildings_of(1) == left, "the legion goes home, nothing more falls");

    // ---- another order ends it ----
    if (!mp_savegame_read(start)) {
        return 2;
    }
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_BRUTAL, 0);
    city_action(0, MP_ACTION_LEGION_ATTACK, legion1, base.target_x, base.target_y);
    war_run(200);
    uint64_t with_order = mp_checksum_state();
    mp_assault_cancel(legion1);
    CHECK(mp_checksum_state() != with_order, "the order is in the checksum");
    city_action(0, MP_ACTION_LEGION_ATTACK, legion1, base.target_x, base.target_y);
    war_run(200);
    city_action(0, MP_ACTION_LEGION_MOVE, legion1, x0 + 2, y0 - 8);
    CHECK(!mp_assault_is_active(legion1) && formation_get(legion1)->standard_x == x0 + 2,
        "another order (move): the attack ends, the legion goes where it is sent");
    city_action(0, MP_ACTION_LEGION_ATTACK, legion1, base.target_x, base.target_y);
    war_run(100);
    city_action(0, MP_ACTION_LEGION_RETURN_HOME, legion1, 0, 0);
    CHECK(!mp_assault_is_active(legion1) && formation_get(legion1)->is_at_fort, "back to the fort: the attack ends");
    // no soldier left
    war_run(600);
    city_action(0, MP_ACTION_LEGION_ATTACK, legion1, base.target_x, base.target_y);
    war_run(100);
    int was_active = mp_assault_is_active(legion1);
    for (int i = 1; i < MAX_FIGURES; i++) {
        figure *f = figure_get(i);
        if (f->state == FIGURE_STATE_ALIVE && f->formation_id == legion1 && figure_is_legion(f)) {
            f->state = FIGURE_STATE_DEAD;
        }
    }
    war_run(1);
    CHECK(was_active && !mp_assault_is_active(legion1), "the legion has no soldier left: the attack ends");
    remove(start);

    // ---- a neutral player ----
    failures += attack_neutral(file);

    // ---- a classic game: nothing ----
    player_context_switch(0);
    player_context_set_num_players(1);
    if (!load(file)) {
        return 2;
    }
    uint64_t classic = mp_checksum_state();
    city_action(0, MP_ACTION_LEGION_ATTACK, FORMATION_FIRST, 50, 50);
    CHECK(!mp_assault_is_active(FORMATION_FIRST) && mp_checksum_state() == classic,
        "a classic game: no attack, nothing changes");
#undef CHECK
    printf("%s\n", failures ? "DIFFERENT: the order to attack is not as designed" :
        "Identical: the order to attack is as designed");
    return failures ? 1 : 0;
}

// The game of tools/mp-attack-test.sh (D-087): two players on map 1 at war (brutal, declared by player 1); the legion
// of player 1 (16 legionaries) rests in its fort; player 2 has the quarter of command_warattack
static int command_attacksave(const char *file, const char *out)
{
    int failures = 0;
#define CHECK(condition, text) do { int ok_ = (condition); printf("%-70s %s\n", text, ok_ ? "yes" : "NO"); failures += !ok_; } while (0)
    int legion1, legion2, tx, ty, x, y;
    if (!start_war_game(file, &legion1, &legion2, &tx, &ty)) {
        printf("Unable to prepare the game\n");
        return 2;
    }
    CHECK(build_attack_quarter(&x, &y), "player 2 builds a quarter of statues, a well and a prefecture");
    // without the fog of war: the screenshots show the quarter of player 2 that player 1 has not explored
    game_rules_settings rules;
    game_rules_default_multiplayer_settings(&rules);
    rules.ai_invasions = 0;
    rules.fog_of_war = 0;
    game_rules_set_multiplayer(&rules);
    city_action(0, MP_ACTION_DECLARE_WAR, 1, MP_WAR_BRUTAL, 0);
    war_run(100);
    const formation *m = formation_get(legion1);
    printf("the legion of player 1 rests at (%d, %d); the quarter of player 2 around (%d, %d)\n", m->x_home, m->y_home,
        x, y);
    CHECK(mp_war_is_fighting(0, 1) && m->is_at_fort && alive_soldiers(legion1) == 16,
        "at war, the legion of player 1 in its fort with 16 soldiers");
    CHECK(mp_savegame_write(out), "the game is written");
    uint64_t written = mp_checksum_state();
    CHECK(mp_savegame_read(out) && mp_checksum_state() == written, "and reads back the same");
    player_context_switch(0);
    player_context_set_num_players(1);
#undef CHECK
    printf("%s\n", failures ? "FAILED: the game of mp-attack-test.sh was not written" :
        "Written: the game of mp-attack-test.sh");
    return failures ? 1 : 0;
}

// the camera of a player reaches every corner of a generated map, after it went through a .mpmap file as in a game
static int command_viewcorners(const char *file, int num_players)
{
    int size = mp_mapgen_default_size(num_players);
    char map_file[64]; // one file per test: ctest runs them in parallel
    snprintf(map_file, sizeof(map_file), "viewcorners-%d-%d.mpmap", options.prepared_map + 1, num_players);
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

// ---------------------------------------------------------------------------------------------------------------
// T4.16, D-072: is the game more restrictive than the original? The same small city is built by commands on the
// same land, played two years, and measured, under classic rules and under the multiplayer rules, at two
// difficulties. A difference that neither the difficulty, nor the money, nor the zone rule explains is a bug.
//
// The city has its water as a player gets it: a reservoir at the source (beside the aqueduct of Caesar, inland; beside
// the sea, on the coast), an aqueduct to a second reservoir near the plan, three fountains that serve the houses
// (restrict_build_water); the services of the plan are wells, a market with a granary kept full, a temple,
// a prefecture and an engineer; the jobs (workshops, hospitals) outnumber the people who can work, so that a lack of
// workers is there to be compared (restrict_build_extra_workshops).
//
// Limit: only the "alone" pair compares a really classic game with a multiplayer one. With four cities on the map,
// game_rules_multiplayer_map() is true even under classic rules (several cities: separate water ranges, build
// permissions), so the "classic" four-player series only differs from the multiplayer one by the zone rule and
// is_multiplayer. In the classic alone game the aqueduct of Caesar carries no water (it is a feature of the
// multiplayer map): the source is a pond beside the same reservoir, the water of the original rules.
// ---------------------------------------------------------------------------------------------------------------

#define RESTRICT_MONTHS 24
#define RESTRICT_TICKS_PER_MONTH 800 // 16 days of 50 ticks
#define RESTRICT_EXTRA_MONTH 2       // the month the extra workshops are built: the zone of a multiplayer city has grown
#define RESTRICT_FUNDS 100000        // the same treasury for every variant: money explains nothing here
#define RESTRICT_MAX_CLASSIC 8       // real classic maps given on the command line
#define RESTRICT_WANTED 63           // 44 houses, 5 wells, granary, market, temple, prefecture, engineers, 4 workshops,
                                     // 2 reservoirs, 3 fountains (the extra workshops come on top, see restrict_result)

typedef struct {
    int built;             // buildings of the plan the commands put on the land (farms and mission not included)
    int farms;
    int population, peak, population_month6, population_year1;
    int houses, inhabited; // merged houses count once; vacant lots are not inhabited
    int level_sum;         // sum of the house levels of the inhabited houses
    int water, food, religion, entertainment, education, health; // inhabited houses served
    int cover_market, cover_temple, cover_prefecture, cover_engineers; // houses reached by each service building
    int desirability;      // average of the inhabited houses, times 10
    int staffed, mission_workers, workers_available, shortage, unemployed, unemployment;
    int sentiment, migration_percentage;
    int immigrated, emigrated, refused;
    int culture, prosperity, peace, favor;
    int spent, treasury;
    int zone;              // clear tiles of the zone of the city (multiplayer rules only)
    int fountain, well;    // inhabited houses served by a fountain (reservoir range), by a well
    int extra;             // workshops beyond the plan that fit on the free land around it, so that jobs outnumber workers
    int reservoirs, reservoirs_watered; // reservoirs built (the source and the one beside the plan), with water at the end
    int reservoir_id[2];
    int fountain_count;    // fountains of the plan that were built
    int origin_x, origin_y; // the block of the plan
    int pond;              // the reservoir of the source had no water near: a pond was dug for it (classic maps)
} restrict_result;

typedef struct {
    int multiplayer;   // multiplayer rules (zone, reservoirs that hold water) instead of the classic ones
    int num_players;   // cities on the map
    int difficulty;    // DIFFICULTY_*
    int only;          // the only city that builds (the others stay as the map made them), or -1 for all
    const char *classic; // a real classic map (.map of the free game) or mission save, played alone instead of the
                         // prepared map; 0 for the prepared map
} restrict_setup;

static void set_local_difficulty(int difficulty)
{
    for (int i = 0; i < 5; i++) {
        setting_decrease_difficulty();
    }
    for (int i = 0; i < difficulty; i++) {
        setting_increase_difficulty();
    }
}

static int restrict_area_clear(int x, int y, int width, int height)
{
    if (x < 0 || y < 0 || x + width > map_grid_width() || y + height > map_grid_height()) {
        return 0;
    }
    for (int yy = y; yy < y + height; yy++) {
        for (int xx = x; xx < x + width; xx++) {
            if (map_terrain_is(map_grid_offset(xx, yy), TERRAIN_NOT_CLEAR)) {
                return 0;
            }
        }
    }
    return 1;
}

// the plan occupies a block of RESTRICT_SIZE x RESTRICT_SIZE tiles: the clear block nearest to the city center
#define RESTRICT_SIZE 13
// clear tiles around the block: the room of the extra workshops (3 tiles) against its roads
#define RESTRICT_MARGIN 1

static int restrict_find_origin(int cx, int cy, int max_distance, int edge, int *ox, int *oy)
{
    int x0 = cx - RESTRICT_SIZE / 2, y0 = cy - RESTRICT_SIZE / 2;
    for (int d = 0; d <= max_distance; d++) {
        for (int y = y0 - d; y <= y0 + d; y++) {
            for (int x = x0 - d; x <= x0 + d; x++) {
                int dx = x > x0 ? x - x0 : x0 - x, dy = y > y0 ? y - y0 : y0 - y;
                int margin = RESTRICT_MARGIN + edge; // a real map: the plan keeps off the edge of the map
                if ((dx > dy ? dx : dy) == d && restrict_area_clear(x - margin, y - margin, RESTRICT_SIZE + 2 * margin,
                        RESTRICT_SIZE + 2 * margin)) {
                    *ox = x;
                    *oy = y;
                    return 1;
                }
            }
        }
    }
    return 0;
}

// a 3x3 place with some meadow for a farm, beside the plan, clear of it and of the farm already placed (px, py)
static int restrict_find_farm(int ox, int oy, int px, int py, int *fx, int *fy)
{
    for (int d = 1; d <= 14; d++) {
        for (int y = oy - 3 - d; y <= oy + RESTRICT_SIZE + d; y++) {
            for (int x = ox - 3 - d; x <= ox + RESTRICT_SIZE + d; x++) {
                int outside = x + 3 < ox - 1 || x > ox + RESTRICT_SIZE || y + 3 < oy - 1 || y > oy + RESTRICT_SIZE;
                int near_first = px >= 0 && x < px + 5 && x + 5 > px && y < py + 5 && y + 5 > py;
                if (!outside || near_first || !restrict_area_clear(x - 1, y - 1, 5, 5)) {
                    continue;
                }
                int meadow = 0;
                for (int yy = y; yy < y + 3; yy++) {
                    for (int xx = x; xx < x + 3; xx++) {
                        meadow += (map_terrain_get(map_grid_offset(xx, yy)) & TERRAIN_MEADOW) != 0;
                    }
                }
                int dx = x + 1 > ox + RESTRICT_SIZE / 2 ? x + 1 - ox - RESTRICT_SIZE / 2 : ox + RESTRICT_SIZE / 2 - x - 1;
                int dy = y + 1 > oy + RESTRICT_SIZE / 2 ? y + 1 - oy - RESTRICT_SIZE / 2 : oy + RESTRICT_SIZE / 2 - y - 1;
                if (meadow && (dx > dy ? dx : dy) <= RESTRICT_SIZE / 2 + d + 2) {
                    *fx = x;
                    *fy = y;
                    return 1;
                }
            }
        }
    }
    return 0;
}

static void restrict_road(int player_id, int x1, int y1, int x2, int y2)
{
    mp_command road = { .type = MP_COMMAND_BUILD, .player_id = player_id, .args = { BUILDING_ROAD, 0, x1, y1, x2, y2, 0, 0 } };
    mp_command_execute(&road);
}

static int restrict_place(int player_id, int type, int x, int y)
{
    int ok = build_as(player_id, type, x, y, x, y);
    if (!ok) {
        // in a multiplayer game, a tile out of the zone of the player or partly in the zone of another one
        printf("    player %d: type %d at (%d, %d) was not built (zone owner: player %d, 0 nobody)\n", player_id + 1, type, x, y,
            mp_territory_is_active() ? mp_territory_owner(map_grid_offset(x, y)) + 1 : 0);
    }
    return ok;
}

// the nearest clear 3x3 place for a reservoir (given by its center) that satisfies `fits`, tried in order of distance
// from (from_x, from_y), skipping the places already tried
#define RESTRICT_MAX_TRIES 24
typedef int (*restrict_site_test)(int cx, int cy, const void *context);

static int restrict_find_site(int from_x, int from_y, int range, restrict_site_test fits, const void *context,
    int tried[][2], int *num_tried, int *rx, int *ry)
{
    int best = 1 << 30;
    for (int y = from_y - range; y <= from_y + range; y++) {
        for (int x = from_x - range; x <= from_x + range; x++) {
            int distance = (x > from_x ? x - from_x : from_x - x) + (y > from_y ? y - from_y : from_y - y);
            if (distance >= best || !restrict_area_clear(x - 1, y - 1, 3, 3) || !fits(x, y, context)) {
                continue;
            }
            int again = 0;
            for (int i = 0; i < *num_tried; i++) {
                again |= tried[i][0] == x && tried[i][1] == y;
            }
            if (!again) {
                best = distance;
                *rx = x;
                *ry = y;
            }
        }
    }
    if (best == 1 << 30 || *num_tried >= RESTRICT_MAX_TRIES) {
        return 0;
    }
    tried[*num_tried][0] = *rx;
    tried[*num_tried][1] = *ry;
    (*num_tried)++;
    return 1;
}

// the source: a reservoir next to the end of the aqueduct of Caesar (inland player), or beside the sea (coastal player)
static int restrict_fits_sea(int cx, int cy, const void *context)
{
    (void) context;
    return map_terrain_exists_tile_in_area_with_type(cx - 2, cy - 2, 5, TERRAIN_WATER);
}

static int restrict_fits_aqueduct(int cx, int cy, const void *context)
{
    const int *end = context;
    int dx = cx > end[0] ? cx - end[0] : end[0] - cx, dy = cy > end[1] ? cy - end[1] : end[1] - cy;
    return (dx == 2 && dy == 0) || (dx == 0 && dy == 2);
}

// the second reservoir: beside the plan, outside it, with every fountain inside its range (10 tiles around it)
typedef struct {
    int ox, oy;
} restrict_plan_site;

static int restrict_fits_plan(int cx, int cy, const void *context)
{
    const restrict_plan_site *plan = context;
    int apart_x = cx + 1 < plan->ox - 1 || cx - 1 > plan->ox + RESTRICT_SIZE;
    int apart_y = cy + 1 < plan->oy - 1 || cy - 1 > plan->oy + RESTRICT_SIZE;
    if (!apart_x && !apart_y) {
        return 0;
    }
    const int fountain_x[3] = { 1, 6, 10 };
    for (int i = 0; i < 3; i++) {
        int dx = plan->ox + fountain_x[i] > cx ? plan->ox + fountain_x[i] - cx : cx - plan->ox - fountain_x[i];
        int dy = plan->oy + 6 > cy ? plan->oy + 6 - cy : cy - plan->oy - 6;
        if (dx > 11 || dy > 11) {
            return 0;
        }
    }
    return 1;
}

static int restrict_reservoir_at(int x, int y)
{
    return building_get(map_building_at(map_grid_offset(x, y)))->type == BUILDING_RESERVOIR;
}

// water for the plan, as a player does it: a reservoir at the source, an aqueduct to a second reservoir beside the
// plan, three fountains on the service row. The reservoirs of Caesar (no, the aqueduct of Caesar) feed the inland ones
// takes down the reservoir that was built at (x, y): the source did not lead to a place for the second one
static void restrict_remove_reservoir(int x, int y)
{
    building *b = building_get(map_building_at(map_grid_offset(x, y)));
    map_building_tiles_remove(b->id, b->x, b->y);
    b->state = BUILDING_STATE_UNUSED;
}

// one try: a reservoir at the next source and a second one beside the plan, joined by an aqueduct
static int restrict_build_reservoirs(int player_id, int ox, int oy, int classic_map, int use_pond, int tried[][2],
    int *num_tried, restrict_result *r)
{
    int source_x = -1, source_y = -1;
    int end[2];
    int inland = !classic_map && mp_mapgen_caesar_aqueduct_end(player_id, &end[0], &end[1]);
    int found = 0;
    if (use_pond) {
        // a map with no water the aqueduct can use near the plan: a pond is dug beside the plan, where there is room, as
        // the sea is for the other maps (the same place for every difficulty)
        for (int d = 5; d <= 14 && !found; d++) {
            for (int y = oy - d; y <= oy + RESTRICT_SIZE + d && !found; y += 2) {
                for (int x = ox - d; x <= ox + RESTRICT_SIZE + d && !found; x += 2) {
                    int dx = x < ox ? ox - x : x - (ox + RESTRICT_SIZE - 1), dy = y < oy ? oy - y : y - (oy + RESTRICT_SIZE - 1);
                    if ((dx > dy ? dx : dy) != d || !restrict_area_clear(x, y, 1, 1)) {
                        continue;
                    }
                    map_terrain_add(map_grid_offset(x, y), TERRAIN_WATER);
                    *num_tried = 0;
                    found = restrict_find_site(x, y, 4, restrict_fits_sea, 0, tried, num_tried, &source_x, &source_y);
                    if (found) {
                        r->pond = 1;
                    } else {
                        map_terrain_remove(map_grid_offset(x, y), TERRAIN_WATER);
                    }
                }
            }
        }
        map_routing_update_land();
    } else {
        found = inland ?
            restrict_find_site(end[0], end[1], 3, restrict_fits_aqueduct, end, tried, num_tried, &source_x, &source_y) :
            restrict_find_site(ox + RESTRICT_SIZE / 2, oy + RESTRICT_SIZE / 2, 45, restrict_fits_sea, 0, tried,
                num_tried, &source_x, &source_y);
    }
    if (!found) {
        printf("    player %d: no place for the reservoir of the source\n", player_id + 1);
        return -1;
    }
    if (!build_as(player_id, BUILDING_DRAGGABLE_RESERVOIR, source_x, source_y, source_x, source_y) ||
        !restrict_reservoir_at(source_x, source_y)) {
        printf("    player %d: the reservoir of the source at (%d, %d) was not built (zone owner: player %d)\n",
            player_id + 1, source_x, source_y,
            mp_territory_is_active() ? mp_territory_owner(map_grid_offset(source_x, source_y)) + 1 : 0);
        return 0;
    }
    if (inland && !game_rules_multiplayer_map()) {
        // the aqueduct of Caesar is a feature of the multiplayer map: a classic game has none that carries water. The
        // classic source of the same place is a pond beside the reservoir, the water of the original rules
        for (int d = 0; d < 5; d++) {
            int x = source_x - 2 + d, y = source_y - 2;
            if (restrict_area_clear(x, y, 1, 1)) {
                map_terrain_add(map_grid_offset(x, y), TERRAIN_WATER);
                map_routing_update_land();
                break;
            }
        }
    }
    restrict_plan_site plan = { ox, oy };
    int num_tried_plan = 0, plan_tried[RESTRICT_MAX_TRIES][2], x, y;
    while (restrict_find_site(source_x, source_y, 60, restrict_fits_plan, &plan, plan_tried, &num_tried_plan, &x, &y)) {
        mp_command drag = { .type = MP_COMMAND_BUILD, .player_id = player_id,
            .args = { BUILDING_DRAGGABLE_RESERVOIR, 0, source_x, source_y, x, y, 0, 0 } };
        mp_command_execute(&drag);
        if (restrict_reservoir_at(x, y)) {
            r->reservoir_id[0] = map_building_at(map_grid_offset(source_x, source_y));
            r->reservoir_id[1] = map_building_at(map_grid_offset(x, y));
            r->reservoirs = 2;
            return 1;
        }
    }
    printf("    player %d: no reservoir beside the plan could be joined to the source at (%d, %d)\n", player_id + 1,
        source_x, source_y);
    restrict_remove_reservoir(source_x, source_y);
    return 0;
}

// water for the plan, as a player does it: a reservoir at the source, an aqueduct to a second reservoir beside the
// plan, three fountains on the service row. The reservoirs of Caesar (no, the aqueduct of Caesar) feed the inland ones.
// A real classic map tries several sources (the nearest water may be a lake without room for the aqueduct)
static int restrict_build_water(int player_id, int ox, int oy, int classic_map, restrict_result *r)
{
    int tried[RESTRICT_MAX_TRIES][2], num_tried = 0;
    int built = 0;
    int attempts = classic_map ? 4 : 1;
    for (int attempt = 0; attempt < attempts + classic_map && !built; attempt++) {
        // after the natural water, a pond (the last attempt)
        int result = restrict_build_reservoirs(player_id, ox, oy, classic_map, attempt == attempts, tried, &num_tried, r);
        if (result < 0 && attempt < attempts) {
            attempt = attempts - 1; // no more natural sources
        } else if (result < 0) {
            break;
        }
        built = result > 0;
    }
    if (!built) {
        return 0;
    }
    static const int fountain_x[3] = { 1, 6, 10 };
    for (int i = 0; i < 3; i++) {
        r->fountain_count += restrict_place(player_id, BUILDING_FOUNTAIN, ox + fountain_x[i], oy + 6);
    }
    return 1;
}

// two wheat farms on meadow, each joined to the loop by a road. The food of the first months comes from the
// granary, which the test fills: the farms are slow
static void restrict_build_farms(int player_id, int ox, int oy, restrict_result *r)
{
    int fx = -1, fy = -1;
    for (int i = 0; i < 2; i++) {
        int x, y;
        if (!restrict_find_farm(ox, oy, fx, fy, &x, &y)) {
            break;
        }
        if (restrict_place(player_id, BUILDING_WHEAT_FARM, x, y)) {
            r->farms++;
            fx = x;
            fy = y;
            // a road from the side of the farm that faces the loop, to the nearest tile of the loop
            int to_x = x + 1 < ox ? ox : x + 1 > ox + 12 ? ox + 12 : x + 1;
            int to_y = y + 1 < oy ? oy : y + 1 > oy + 10 ? oy + 10 : y + 1;
            int from_x = x + 1 < ox ? x + 3 : x + 1 > ox + 12 ? x - 1 : x + 1;
            int from_y = y + 1 < oy ? y + 3 : y + 1 > oy + 10 ? y - 1 : y + 1;
            if (from_x == x + 1 && from_y == y + 1) {
                continue; // the farm is on the block: cannot be (the block is clear and the farm is outside)
            }
            restrict_road(player_id, from_x, from_y, to_x, to_y);
        }
    }
}

// the plan has four workshops on its south row: more fit on the free land against the roads of the loop
// (the block is clear by one tile around it only, so every place is checked), so that the jobs outnumber the people
// who can work and a lack of workers can show up
static void restrict_build_extra_workshops(int player_id, int ox, int oy, restrict_result *r)
{
    static const int KINDS[4] = { BUILDING_POTTERY_WORKSHOP, BUILDING_HOSPITAL, BUILDING_FURNITURE_WORKSHOP,
        BUILDING_WEAPONS_WORKSHOP };
    int number = 0;
    for (int side = 0; side < 2; side++) {
        for (int row = 0; row < 4; row++) {
            int x = side ? ox + 13 : ox - 3, y = oy + 3 * row;
            if (restrict_area_clear(x, y, 3, 3) && restrict_place(player_id, KINDS[number++ % 4], x, y)) {
                r->extra++;
            }
        }
    }
    // and a second row of workshops to the south, 6 tiles from the houses, on a road of its own (the workshops
    // pull the desirability of the houses near them down: the houses must still be able to grow)
    restrict_road(player_id, ox, oy + 10, ox, oy + 14);
    restrict_road(player_id, ox, oy + 14, ox + 12, oy + 14);
    for (int column = 0; column < 4; column++) {
        int x = ox + 3 * column, y = oy + 15;
        if (restrict_area_clear(x, y, 3, 3) && restrict_place(player_id, KINDS[number++ % 4], x, y)) {
            r->extra++;
        }
    }
}

// the plan, in coordinates of the block (0, 0) to (12, 12): roads every three tiles with two rows of houses
// between them, a service row (wells, granary, market, temple, prefecture, engineers), workshops to the south
static int restrict_build_city(int player_id, int cx, int cy, int max_distance, int classic_map, restrict_result *r)
{
    int ox, oy;
    // a real map: room around the plan if there is some (the extra workshops need 3 tiles on each side), else the edge
    if (!restrict_find_origin(cx, cy, max_distance, classic_map ? 5 : 0, &ox, &oy) &&
        !(classic_map && restrict_find_origin(cx, cy, max_distance, 0, &ox, &oy))) {
        printf("    player %d: no clear block near (%d, %d)\n", player_id + 1, cx, cy);
        return 0;
    }
    player_context_switch(player_id);
    city_data.finance.treasury = RESTRICT_FUNDS;
    int ex = city_map_entry_point()->x, ey = city_map_entry_point()->y;
    player_context_switch(0);
    // the entry point is joined to the nearest middle of a side of the loop
    int tx[4] = { ox + 6, ox + 6, ox, ox + 12 }, ty[4] = { oy, oy + 10, oy + 5, oy + 5 };
    int best = 0, best_distance = 1 << 30;
    for (int i = 0; i < 4; i++) {
        int dx = tx[i] > ex ? tx[i] - ex : ex - tx[i], dy = ty[i] > ey ? ty[i] - ey : ey - ty[i];
        if (dx + dy < best_distance) {
            best_distance = dx + dy;
            best = i;
        }
    }
    restrict_road(player_id, ex, ey, tx[best], ty[best]);
    if (classic_map && !map_terrain_is(map_grid_offset(tx[best], ty[best]), TERRAIN_ROAD)) {
        printf("    the plan at (%d, %d) cannot be joined to the entry point (%d, %d)\n", ox, oy, ex, ey);
        return 0;
    }
    if (getenv("RESTRICT_TRACE")) {
        printf("  entry (%d, %d), road to (%d, %d): entry %s, end %s\n", ex, ey, tx[best], ty[best],
            map_terrain_is(map_grid_offset(ex, ey), TERRAIN_ROAD) ? "road" : "no road",
            map_terrain_is(map_grid_offset(tx[best], ty[best]), TERRAIN_ROAD) ? "road" : "no road");
    }
    restrict_road(player_id, ox, oy, ox + 12, oy);
    restrict_road(player_id, ox, oy + 3, ox + 12, oy + 3);
    restrict_road(player_id, ox, oy + 7, ox + 12, oy + 7);
    restrict_road(player_id, ox, oy + 10, ox + 12, oy + 10);
    restrict_road(player_id, ox, oy, ox, oy + 10);
    restrict_road(player_id, ox + 12, oy, ox + 12, oy + 10);
    mp_command band1 = { .type = MP_COMMAND_BUILD, .player_id = player_id,
        .args = { BUILDING_HOUSE_VACANT_LOT, 0, ox + 1, oy + 1, ox + 11, oy + 2, 0, 0 } };
    mp_command_execute(&band1);
    mp_command band3 = { .type = MP_COMMAND_BUILD, .player_id = player_id,
        .args = { BUILDING_HOUSE_VACANT_LOT, 0, ox + 1, oy + 8, ox + 11, oy + 9, 0, 0 } };
    mp_command_execute(&band3);
    restrict_place(player_id, BUILDING_WELL, ox + 1, oy + 4);
    restrict_place(player_id, BUILDING_GRANARY, ox + 2, oy + 4);
    restrict_place(player_id, BUILDING_MARKET, ox + 5, oy + 4);
    restrict_place(player_id, BUILDING_SMALL_TEMPLE_CERES, ox + 7, oy + 4);
    restrict_place(player_id, BUILDING_PREFECTURE, ox + 9, oy + 4);
    restrict_place(player_id, BUILDING_ENGINEERS_POST, ox + 10, oy + 4);
    restrict_place(player_id, BUILDING_WELL, ox + 11, oy + 4);
    restrict_place(player_id, BUILDING_WELL, ox + 5, oy + 6);
    restrict_place(player_id, BUILDING_WELL, ox + 8, oy + 6);
    restrict_place(player_id, BUILDING_WELL, ox + 11, oy + 6);
    restrict_place(player_id, BUILDING_POTTERY_WORKSHOP, ox + 1, oy + 11);
    restrict_place(player_id, BUILDING_FURNITURE_WORKSHOP, ox + 4, oy + 11);
    restrict_place(player_id, BUILDING_WEAPONS_WORKSHOP, ox + 7, oy + 11);
    restrict_place(player_id, BUILDING_POTTERY_WORKSHOP, ox + 10, oy + 11);
    if (!restrict_build_water(player_id, ox, oy, classic_map, r) && classic_map) {
        return 0;
    }
    r->origin_x = ox;
    r->origin_y = oy;
    return 1;
}

// the granary of the city now in context holds `amount` of wheat
static void restrict_refill_granary(int amount)
{
    for (int i = BUILDING_FIRST; i < BUILDING_END; i++) {
        building *b = building_get(i);
        if (b->state == BUILDING_STATE_IN_USE && b->type == BUILDING_GRANARY) {
            b->data.granary.resource_stored[RESOURCE_WHEAT] = amount;
        }
    }
}

static void restrict_stock_granary(int player_id, int amount)
{
    player_context_switch(player_id);
    restrict_refill_granary(amount);
    player_context_switch(0);
}

// a classic game has no mission posts at the start: the one of the prepared map, and its missionary, are removed
static void restrict_remove_missions(int player_id)
{
    player_context_switch(player_id);
    for (int i = BUILDING_FIRST; i < BUILDING_END; i++) {
        building *b = building_get(i);
        if (b->state == BUILDING_STATE_IN_USE && b->type == BUILDING_MISSION_POST) {
            map_building_tiles_remove(b->id, b->x, b->y);
            b->state = BUILDING_STATE_UNUSED;
        }
    }
    for (int i = FIGURE_FIRST; i < FIGURE_END; i++) {
        figure *f = figure_get(i);
        if (f->state == FIGURE_STATE_ALIVE && f->type == FIGURE_MISSIONARY) {
            f->state = FIGURE_STATE_DEAD;
        }
    }
    player_context_switch(0);
}

static int restrict_count_buildings(int player_id)
{
    int count = 0;
    player_context_switch(player_id);
    for (int i = BUILDING_FIRST; i < BUILDING_END; i++) {
        building *b = building_get(i);
        count += (b->state == BUILDING_STATE_IN_USE || b->state == BUILDING_STATE_CREATED) &&
            b->type != BUILDING_WHEAT_FARM && b->type != BUILDING_MISSION_POST;
    }
    player_context_switch(0);
    return count;
}

static void restrict_measure(int player_id, restrict_result *r)
{
    player_context_switch(player_id);
    r->population = city_population();
    long desirability = 0;
    for (int i = BUILDING_FIRST; i < BUILDING_END; i++) {
        building *b = building_get(i);
        if (b->state != BUILDING_STATE_IN_USE) {
            continue;
        }
        if (!b->house_size) {
            // the mission post of a multiplayer map is no job of the original: counted apart
            if (b->type == BUILDING_MISSION_POST) {
                r->mission_workers += b->num_workers;
            } else {
                r->staffed += b->num_workers;
            }
            r->cover_market += b->type == BUILDING_MARKET ? b->houses_covered : 0;
            r->cover_temple += b->type == BUILDING_SMALL_TEMPLE_CERES ? b->houses_covered : 0;
            r->cover_prefecture += b->type == BUILDING_PREFECTURE ? b->houses_covered : 0;
            r->cover_engineers += b->type == BUILDING_ENGINEERS_POST ? b->houses_covered : 0;
            continue;
        }
        r->houses++;
        if (b->house_population <= 0) {
            continue;
        }
        r->inhabited++;
        r->level_sum += b->type - BUILDING_HOUSE_VACANT_LOT;
        desirability += b->desirability;
        r->water += b->has_water_access || b->has_well_access;
        r->fountain += b->has_water_access != 0;
        r->well += b->has_well_access != 0;
        r->food += b->data.house.num_foods > 0;
        r->religion += b->data.house.num_gods > 0;
        r->entertainment += b->data.house.entertainment > 0;
        r->education += b->data.house.education > 0;
        r->health += b->data.house.health > 0;
    }
    for (int i = 0; i < 2; i++) {
        building *reservoir = building_get(r->reservoir_id[i]);
        r->reservoirs_watered += r->reservoir_id[i] && reservoir->type == BUILDING_RESERVOIR &&
            reservoir->state == BUILDING_STATE_IN_USE && reservoir->has_water_access;
    }
    r->desirability = r->inhabited ? (int) (10 * desirability / r->inhabited) : 0;
    r->workers_available = city_data.labor.workers_available;
    r->shortage = city_labor_workers_needed();
    r->unemployed = city_labor_workers_unemployed();
    r->unemployment = city_labor_unemployment_percentage();
    r->sentiment = city_sentiment();
    r->migration_percentage = city_data.migration.percentage;
    r->culture = city_rating_culture();
    r->prosperity = city_rating_prosperity();
    r->peace = city_rating_peace();
    r->favor = city_rating_favor();
    r->treasury = city_finance_treasury();
    if (mp_territory_is_active()) {
        for (int y = 0; y < map_grid_height(); y++) {
            for (int x = 0; x < map_grid_width(); x++) {
                int o = map_grid_offset(x, y);
                r->zone += mp_territory_owner(o) == player_id && !map_terrain_is(o, TERRAIN_NOT_CLEAR);
            }
        }
    }
    player_context_switch(0);
}

// plays the two years; the result of every city goes in results[player]
static int restrict_play(const char *file, const restrict_setup *setup, restrict_result *results)
{
    player_context_switch(0);
    player_context_set_num_players(1);
    memset(results, 0, sizeof(restrict_result) * MP_MAPGEN_MAX_PLAYERS);
    game_rules_set_classic();
    int max_distance = 18;
    int base = 0; // buildings the map already has (a classic mission save has a few)
    if (setup->classic) {
        // a real classic map: free land near its entry point, the rules of the original
        max_distance = 80;
        if (!load(setup->classic)) {
            return 0;
        }
        base = restrict_count_buildings(0);
        if (getenv("RESTRICT_TRACE")) {
            int water = 0, meadow = 0;
            for (int y = 0; y < map_grid_height(); y++) {
                for (int x = 0; x < map_grid_width(); x++) {
                    water += map_terrain_is(map_grid_offset(x, y), TERRAIN_WATER) != 0;
                    meadow += (map_terrain_get(map_grid_offset(x, y)) & TERRAIN_MEADOW) != 0;
                }
            }
            printf("  %s: %d x %d tiles, %d water, %d meadow, %d buildings\n", setup->classic, map_grid_width(),
                map_grid_height(), water, meadow, base);
        }
    } else if (!mp_mapgen_create_prepared(file, setup->num_players, 0)) {
        printf("Unable to create the prepared map\n");
        return 0;
    }
    // nothing of the template that harms a city either way: armies, earthquake, revolt, requests, random events
    memset(scenario.invasions, 0, sizeof(scenario.invasions));
    memset(scenario.requests, 0, sizeof(scenario.requests));
    memset(&scenario.random_events, 0, sizeof(scenario.random_events));
    scenario.earthquake.severity = 0;
    scenario.emperor_change.enabled = 0;
    scenario.gladiator_revolt.enabled = 0;
    scenario_invasion_clear();
    set_local_difficulty(setup->difficulty);
    if (setting_gods_enabled()) {
        setting_toggle_gods_enabled();
    }
    if (setup->multiplayer) {
        game_rules_settings rules;
        game_rules_default_multiplayer_settings(&rules);
        rules.difficulty = setup->difficulty;
        rules.gods_enabled = 0;
        rules.ai_invasions = 0;
        rules.territories = 1;
        game_rules_set_multiplayer(&rules);
    } else {
        game_rules_set_classic();
    }
    for (int p = 0; p < setup->num_players; p++) {
        if (setup->only >= 0 && p != setup->only) {
            continue;
        }
        int cx, cy;
        if (setup->classic) {
            cx = city_map_entry_point()->x;
            cy = city_map_entry_point()->y;
        } else {
            mp_mapgen_city_center(p, &cx, &cy);
        }
        // a classic game starts with the favor of its difficulty; the prepared map leaves it at zero, which would
        // bring the legions of Caesar after a year: that is the rule of the original, not what is measured here
        player_context_switch(p);
        city_data.ratings.favor = city_data.ratings.favor_last_year = difficulty_starting_favor();
        player_context_switch(0);
        if (!restrict_build_city(p, cx, cy, max_distance, setup->classic != 0, &results[p])) {
            return 0;
        }
        restrict_stock_granary(p, 3200);
        if (!setup->multiplayer) {
            restrict_remove_missions(p);
        }
        results[p].built = restrict_count_buildings(p) - base;
        player_context_switch(p);
        results[p].spent = RESTRICT_FUNDS - city_finance_treasury();
        player_context_switch(0);
    }
    map_road_network_update_grid();
    for (int p = 0; p < setup->num_players; p++) {
        player_context_switch(p);
        map_road_network_update_largest();
    }
    player_context_switch(0);
    setting_reset_speeds(500, setting_scroll_speed());
    for (int tick = 1; tick <= RESTRICT_MONTHS * RESTRICT_TICKS_PER_MONTH; tick++) {
        run_one_tick();
        if (tick == RESTRICT_EXTRA_MONTH * RESTRICT_TICKS_PER_MONTH) {
            // by now the first houses are lived in: their zone reaches the land around the plan, where the farms and
            // the extra workshops go (at the start of a multiplayer game the zone is that of the mission post only)
            for (int p = 0; p < setup->num_players; p++) {
                if (setup->only < 0 || p == setup->only) {
                    restrict_build_farms(p, results[p].origin_x, results[p].origin_y, &results[p]);
                    restrict_build_extra_workshops(p, results[p].origin_x, results[p].origin_y, &results[p]);
                }
            }
            map_road_network_update_grid();
            for (int p = 0; p < setup->num_players; p++) {
                player_context_switch(p);
                map_road_network_update_largest();
            }
            player_context_switch(0);
        }
        for (int p = 0; p < setup->num_players; p++) {
            player_context_switch(p);
            if (game_time_tick() == 24) { // after the migration of the day
                results[p].immigrated += city_data.migration.immigrated_today;
                results[p].emigrated += city_data.migration.emigrated_today;
                results[p].refused += city_data.migration.refused_immigrants_today;
            }
            // no fire and no collapse: they are chance, and one fire in one city and not in the other (the random
            // numbers do not stay the same once a farm is out of the zone) would overshadow what is measured. No
            // engineer reaches the reservoirs and the aqueduct, far from the plan, in any case
            for (int i = BUILDING_FIRST; i < BUILDING_END; i++) {
                building *b = building_get(i);
                b->fire_risk = 0;
                b->damage_risk = 0;
            }
            if (tick % RESTRICT_TICKS_PER_MONTH == 0) {
                // the granary is refilled each month: the farms are slow and the people must not starve for the
                // houses to climb (what is measured is the reach of the services, not the harvest)
                restrict_refill_granary(3200);
                int population = city_population();
                if (getenv("RESTRICT_TRACE")) {
                    printf("  month %d player %d: population %d, sentiment %d, unemployed %d, lack %d, favor %d\n",
                        tick / RESTRICT_TICKS_PER_MONTH, p + 1, population, city_sentiment(),
                        city_labor_workers_unemployed(), city_labor_workers_needed(), city_rating_favor());
                }
                if (population > results[p].peak) {
                    results[p].peak = population;
                }
                if (tick == 6 * RESTRICT_TICKS_PER_MONTH) {
                    results[p].population_month6 = population;
                }
                if (tick == 12 * RESTRICT_TICKS_PER_MONTH) {
                    results[p].population_year1 = population;
                }
            }
        }
        player_context_switch(0);
    }
    for (int p = 0; p < setup->num_players; p++) {
        restrict_result keep = results[p];
        memset(&results[p], 0, sizeof(restrict_result));
        memcpy(results[p].reservoir_id, keep.reservoir_id, sizeof(keep.reservoir_id));
        restrict_measure(p, &results[p]);
        results[p].reservoirs = keep.reservoirs;
        results[p].fountain_count = keep.fountain_count;
        results[p].extra = keep.extra;
        results[p].origin_x = keep.origin_x;
        results[p].origin_y = keep.origin_y;
        results[p].pond = keep.pond;
        results[p].built = keep.built;
        results[p].farms = keep.farms;
        results[p].peak = keep.peak;
        results[p].population_month6 = keep.population_month6;
        results[p].population_year1 = keep.population_year1;
        results[p].immigrated = keep.immigrated;
        results[p].emigrated = keep.emigrated;
        results[p].refused = keep.refused;
        results[p].spent = keep.spent;
    }
    return 1;
}

// A free map with nothing on it, as the editor of the game makes one (80 x 80 tiles, entry point on the east side), with
// a lake in the north and meadow on all the land: the free land of a classic game, written in the format of the maps of
// the game, so that the test needs no map of the game itself (which is not in the repository)
#define RESTRICT_BLANK_MAP "restrictiveness-blank.map"

static int restrict_write_blank_map(void)
{
    game_file_editor_create_scenario(2);
    for (int y = 0; y < map_grid_height(); y++) {
        for (int x = 0; x < map_grid_width(); x++) {
            int lake = x >= 40 && x < 48 && y >= 8 && y < 16;
            map_terrain_add(map_grid_offset(x, y), lake ? TERRAIN_WATER : TERRAIN_MEADOW);
        }
    }
    return game_file_editor_write_scenario(RESTRICT_BLANK_MAP);
}

// the funds a city starts with, at a difficulty: the same money for both sets of rules (the difficulty decides)
static int restrict_starting_funds(const char *file, int multiplayer, int difficulty)
{
    player_context_switch(0);
    player_context_set_num_players(1);
    set_local_difficulty(difficulty);
    if (multiplayer) {
        game_rules_settings rules;
        game_rules_default_multiplayer_settings(&rules);
        rules.difficulty = difficulty;
        game_rules_set_multiplayer(&rules);
    } else {
        game_rules_set_classic();
    }
    if (!mp_mapgen_create_prepared(file, 1, 0)) {
        return -1;
    }
    int funds = city_finance_treasury();
    game_rules_set_classic();
    return funds;
}

static void restrict_print_header(void)
{
    printf("%-22s %4s %5s %4s %7s %5s %5s %4s %4s %4s %4s %4s %4s %4s %4s %4s %4s %4s %4s %5s %5s %5s %4s %4s\n", "variant", "blds", "fm+ex",
        "pop", "p6/p12", "hous", "level", "wat", "fnt", "food", "rel", "mkt", "tem", "pre", "eng", "staf", "mwrk", "lack", "idle",
        "desir", "immig", "emig", "sent", "pct");
}

static void restrict_print(const char *label, const restrict_result *r)
{
    printf("%-22s %4d %2d+%-2d %4d %3d/%-3d %2d/%-2d %5d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %4d %5d %5d %5d %4d %4d\n", label,
        r->built, r->farms, r->extra, r->population, r->population_month6, r->population_year1, r->inhabited, r->houses, r->inhabited ? 10 * r->level_sum / r->inhabited : 0,
        r->water, r->fountain, r->food, r->religion, r->cover_market, r->cover_temple, r->cover_prefecture, r->cover_engineers,
        r->staffed, r->mission_workers, r->shortage, r->workers_available - r->staffed - r->mission_workers,
        r->desirability, r->immigrated, r->emigrated, r->sentiment, r->migration_percentage);
}

// the two cities are alike: each figure within `percent` percent, plus a slack for the small numbers
static int restrict_close(int a, int b, int percent, int slack)
{
    int difference = a > b ? a - b : b - a;
    int larger = a > b ? a : b;
    return difference <= slack + larger * percent / 100;
}

// b, the multiplayer rules (or the city building with the others), against a: alike, and its people never less happy
static int restrict_alike(const char *what, const restrict_result *a, const restrict_result *b)
{
    int level_a = a->inhabited ? 10 * a->level_sum / a->inhabited : 0;
    int level_b = b->inhabited ? 10 * b->level_sum / b->inhabited : 0;
    // the zone rule leaves a farm or a workshop out of one of the two cities now and then: a place on the border of the
    // zone of a neighbour. Each missing building is a few jobs
    int gap = (a->farms > b->farms ? a->farms - b->farms : b->farms - a->farms) +
        (a->extra > b->extra ? a->extra - b->extra : b->extra - a->extra);
    int ok = a->built == b->built &&
        restrict_close(a->population, b->population, 8, 6) &&               // the people
        restrict_close(a->inhabited, b->inhabited, 15, 1) &&                 // the houses that are lived in
        restrict_close(level_a, level_b, 15, 3) &&                           // how far they evolved
        restrict_close(a->water, b->water, 15, 1) && restrict_close(a->food, b->food, 15, 1) &&
        restrict_close(a->religion, b->religion, 15, 1) &&                   // the houses served
        restrict_close(a->fountain, b->fountain, 15, 1) && restrict_close(a->well, b->well, 15, 1) &&
        a->reservoirs_watered == b->reservoirs_watered &&                    // the reach of the water
        restrict_close(a->cover_market, b->cover_market, 10, 10) &&          // the reach of the buildings
        restrict_close(a->cover_temple, b->cover_temple, 10, 10) &&
        restrict_close(a->cover_prefecture, b->cover_prefecture, 10, 10) &&
        restrict_close(a->cover_engineers, b->cover_engineers, 10, 10) &&
        restrict_close(a->staffed, b->staffed, 8, 6) && restrict_close(a->shortage, b->shortage, 15, 12 + 12 * gap) && // the workers
        restrict_close(a->workers_available - a->staffed - a->mission_workers,
            b->workers_available - b->staffed - b->mission_workers, 25, 8) &&
        restrict_close(a->desirability, b->desirability, 15, 15) &&          // the land
        restrict_close(a->immigrated, b->immigrated, 10, 20) &&              // the newcomers
        // the mood of the people: never worse under the rules of b. Rome pays every multiplayer city the same wages
        // (T5.9, D-079): with classic rules, players 2 to 4 had Rome's raise, and their people were less happy
        b->sentiment >= a->sentiment - 3 * gap &&
        b->migration_percentage >= a->migration_percentage - 25 * gap;
    if (!ok) {
        printf("  DIFFERENT: %s\n", what);
    }
    return ok;
}

// The multiplayer city is not worse off than the same plan on free land of real classic maps: its people, houses served
// by water and jobs filled are at least those of the weakest classic city, within a tolerance. The places differ (trees,
// slopes, the water near the plan, the length of the road from the entry point): the classic cities alone span 306 to
// 406 people, so the multiplayer one is only asked not to fall under that range. The number of houses is not compared:
// they merge into larger ones as they evolve
typedef struct {
    int population, staffed, water; // the weakest classic city: people, employed, houses served by water (percent)
    int cities;
} restrict_floor;

static int restrict_water_percent(const restrict_result *r)
{
    return r->inhabited ? 100 * r->water / r->inhabited : 0;
}

static void restrict_floor_add(restrict_floor *floor, const restrict_result *classic)
{
    if (!floor->cities || classic->population < floor->population) {
        floor->population = classic->population;
    }
    if (!floor->cities || classic->staffed < floor->staffed) {
        floor->staffed = classic->staffed;
    }
    if (!floor->cities || restrict_water_percent(classic) < floor->water) {
        floor->water = restrict_water_percent(classic);
    }
    floor->cities++;
}

static int restrict_no_worse(const char *what, const restrict_result *mp, const restrict_floor *floor)
{
    int ok = mp->population >= floor->population * 85 / 100 - 6 && restrict_water_percent(mp) >= floor->water - 20 &&
        mp->staffed >= floor->staffed * 85 / 100 - 6;
    if (!ok) {
        printf("  DIFFERENT: %s: %d people, water %d%%, %d employed; the weakest classic city has %d, %d%%, %d\n", what,
            mp->population, restrict_water_percent(mp), mp->staffed, floor->population, floor->water, floor->staffed);
    }
    return ok;
}

// A: classic rules, a city alone; B: multiplayer rules, a city alone; C, D: the same with four cities on the map
// for four (C still runs the multiplayer-map code: water ranges of several cities, build permissions); E: multiplayer
// rules, four cities on the map but only one builds. Prints the table, checks that A and B, C and D, D and E are
// alike (the zone rule and the mission post excepted: the mission post of the prepared map is removed from the
// classic cities, and a building on the border of a neighbour's zone is not built, which is all they differ by)
static int command_restrictiveness(const char *file, int num_classic, char **classic)
{
    static const char *NAMES[] = { "very easy", "easy", "normal", "hard", "very hard" };
    static const int DIFFICULTIES[2] = { DIFFICULTY_EASY, DIFFICULTY_HARD };
    int failures = 0;
    // "blank" stands for a free map of the editor of the game, made here, first: the file has the grid of a map of the
    // game, which the prepared maps (larger grids) change
    static char blank_name[] = RESTRICT_BLANK_MAP;
    int blank_written = 0;
    for (int i = 0; i < num_classic; i++) {
        if (strcmp(classic[i], "blank") == 0) {
            if (!blank_written && !restrict_write_blank_map()) {
                printf("Unable to write the blank map\n");
                return 2;
            }
            blank_written = 1;
            classic[i] = blank_name;
        }
    }
    for (int d = 0; d < 2; d++) {
        int classic = restrict_starting_funds(file, 0, DIFFICULTIES[d]);
        int multiplayer = restrict_starting_funds(file, 1, DIFFICULTIES[d]);
        printf("starting funds, %s: %d Dn in a classic game, %d Dn in a multiplayer game\n", NAMES[DIFFICULTIES[d]], classic,
            multiplayer);
        if (classic != multiplayer || classic <= 0) {
            printf("  DIFFERENT: the funds depend on more than the difficulty\n");
            failures++;
        }
    }
    restrict_print_header();
    for (int d = 0; d < 2; d++) {
        int difficulty = DIFFICULTIES[d];
        restrict_result alone[2][MP_MAPGEN_MAX_PLAYERS], four[2][MP_MAPGEN_MAX_PLAYERS], only[MP_MAPGEN_MAX_PLAYERS];
        for (int multiplayer = 0; multiplayer < 2; multiplayer++) {
            restrict_setup setup = { multiplayer, 1, difficulty, -1 };
            if (!restrict_play(file, &setup, alone[multiplayer])) {
                return 2;
            }
            setup.num_players = 4;
            if (!restrict_play(file, &setup, four[multiplayer])) {
                return 2;
            }
        }
        // the city builds alone on the map for four: one inland and one on the coast, to save time, at the hard level
        int checked_only[MP_MAPGEN_MAX_PLAYERS] = { 0 };
        for (int p = 0; p < 4 && difficulty == DIFFICULTY_HARD; p++) {
            int coastal = mp_mapgen_slot_is_coastal(p);
            int twin_seen = 0;
            for (int q = 0; q < p; q++) {
                twin_seen |= checked_only[q] && mp_mapgen_slot_is_coastal(q) == coastal;
            }
            if (twin_seen) {
                continue;
            }
            restrict_setup setup = { 1, 4, difficulty, p };
            restrict_result results[MP_MAPGEN_MAX_PLAYERS];
            if (!restrict_play(file, &setup, results)) {
                return 2;
            }
            only[p] = results[p];
            checked_only[p] = 1;
        }
        char label[128];
        restrict_result real[RESTRICT_MAX_CLASSIC];
        int real_played[RESTRICT_MAX_CLASSIC] = { 0 };
        for (int i = 0; i < num_classic && i < RESTRICT_MAX_CLASSIC; i++) {
            restrict_setup setup = { 0, 1, difficulty, -1, classic[i] };
            restrict_result results[MP_MAPGEN_MAX_PLAYERS];
            // a map without a clear block or without water near the entry point cannot hold the plan: it is left out
            real_played[i] = restrict_play(file, &setup, results);
            real[i] = results[0];
        }
        for (int multiplayer = 0; multiplayer < 2; multiplayer++) {
            snprintf(label, sizeof(label), "%s %s alone", NAMES[difficulty], multiplayer ? "MP" : "classic");
            restrict_print(label, &alone[multiplayer][0]);
        }
        for (int multiplayer = 0; multiplayer < 2; multiplayer++) {
            for (int p = 0; p < 4; p++) {
                snprintf(label, sizeof(label), "%s %s 4p #%d", NAMES[difficulty], multiplayer ? "MP" : "classic", p + 1);
                restrict_print(label, &four[multiplayer][p]);
            }
        }
        for (int p = 0; p < 4; p++) {
            if (checked_only[p]) {
                snprintf(label, sizeof(label), "%s MP 4p #%d only", NAMES[difficulty], p + 1);
                restrict_print(label, &only[p]);
            }
        }
        for (int i = 0; i < num_classic && i < RESTRICT_MAX_CLASSIC; i++) {
            if (!real_played[i]) {
                printf("%s: left out, the plan does not fit\n", classic[i]);
                continue;
            }
            const char *name = strrchr(classic[i], '/');
            snprintf(label, sizeof(label), "%.12s %s", strcmp(classic[i], RESTRICT_BLANK_MAP) == 0 ? "blank map" :
                name ? name + 1 : classic[i], NAMES[difficulty]);
            restrict_print(label, &real[i]);
            printf("    block of the plan at (%d, %d)%s\n", real[i].origin_x, real[i].origin_y,
                real[i].pond ? ", water from a pond dug beside it" : "");
        }
        printf("zone of the multiplayer city after two years, in clear tiles (%s): alone %d; with four %d %d %d %d\n",
            NAMES[difficulty], alone[1][0].zone, four[1][0].zone, four[1][1].zone, four[1][2].zone, four[1][3].zone);
        snprintf(label, sizeof(label), "%s: classic and multiplayer rules, alone", NAMES[difficulty]);
        failures += !restrict_alike(label, &alone[0][0], &alone[1][0]);
        for (int p = 0; p < 4; p++) {
            snprintf(label, sizeof(label), "%s: classic and multiplayer rules, player %d of 4", NAMES[difficulty], p + 1);
            failures += !restrict_alike(label, &four[0][p], &four[1][p]);
            if (checked_only[p]) {
                snprintf(label, sizeof(label), "%s: player %d of 4, with and without the other builders",
                    NAMES[difficulty], p + 1);
                failures += !restrict_alike(label, &four[1][p], &only[p]);
            }
        }
        // the plan on free land of a real classic map: it stands, its water runs, and no multiplayer city is poorer. The
        // free map of the test must be there; a map of the game that does not hold the plan is left out
        restrict_floor floor = { 0, 0, 0, 0 };
        for (int i = 0; i < num_classic && i < RESTRICT_MAX_CLASSIC; i++) {
            int blank = strcmp(classic[i], RESTRICT_BLANK_MAP) == 0;
            if (!real_played[i]) {
                failures += blank;
                continue;
            }
            const restrict_result *c = &real[i];
            if (c->built != RESTRICT_WANTED || c->reservoirs_watered != 2 || c->fountain_count != 3 || c->fountain <= 0 ||
                c->population < 150) {
                printf("  DIFFERENT: %s, %s: %d buildings of %d, %d reservoirs with water of 2, %d fountains of 3 serving %d "
                    "houses, population %d\n", NAMES[difficulty], classic[i], c->built, RESTRICT_WANTED,
                    c->reservoirs_watered, c->fountain_count, c->fountain, c->population);
                failures++;
            }
            restrict_floor_add(&floor, c);
        }
        if (floor.cities) {
            snprintf(label, sizeof(label), "%s: multiplayer alone against the classic maps", NAMES[difficulty]);
            failures += !restrict_no_worse(label, &alone[1][0], &floor);
            for (int p = 0; p < 4; p++) {
                snprintf(label, sizeof(label), "%s: multiplayer player %d of 4 against the classic maps", NAMES[difficulty],
                    p + 1);
                failures += !restrict_no_worse(label, &four[1][p], &floor);
            }
        }
        // the whole plan stands, the water reaches the houses by the fountains, the jobs outnumber the people who
        // can work (so that a lack of workers is there to be compared). The sentiment and the migration are not
        // fixed by the difficulty here: a lack of workers moves them, as in the original; classic and multiplayer
        // are compared with each other above
        const restrict_result *every[10] = { &alone[0][0], &alone[1][0], &four[0][0], &four[0][1], &four[0][2],
            &four[0][3], &four[1][0], &four[1][1], &four[1][2], &four[1][3] };
        for (int i = 0; i < 10; i++) {
            const restrict_result *r = every[i];
            if (r->built != RESTRICT_WANTED || r->reservoirs != 2 || r->reservoirs_watered != 2 ||
                r->fountain_count != 3 || r->fountain <= 0 || r->shortage <= 0 || r->population < 150) {
                printf("  DIFFERENT: %s, city %d of 10: %d buildings of %d, %d reservoirs with water of %d, %d fountains of 3 "
                    "serving %d houses, lack %d, population %d\n", NAMES[difficulty], i + 1, r->built, RESTRICT_WANTED,
                    r->reservoirs_watered, r->reservoirs, r->fountain_count, r->fountain, r->shortage, r->population);
                failures++;
            }
        }
    }
    player_context_switch(0);
    player_context_set_num_players(1);
    game_rules_set_classic();
    scenario_set_custom(0);
    if (blank_written) {
        remove(RESTRICT_BLANK_MAP);
    }
    printf("%s\n", failures ? "DIFFERENT: the multiplayer game is more restrictive than the classic one" :
        "Identical: the multiplayer rules change nothing inside a city (zone and mission post apart)");
    return failures ? 1 : 0;
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
    } else if (strcmp(command, "barrackstimber") == 0) {
        result = command_barrackstimber(file);
    } else if (strcmp(command, "troops") == 0) {
        result = command_troops();
    } else if (strcmp(command, "reservoirlevel") == 0) {
        result = command_reservoirlevel(file);
    } else if (strcmp(command, "drying") == 0 && argc > 3) {
        result = command_drying(file, atoi(argv[3]));
    } else if (strcmp(command, "cityluck") == 0) {
        result = command_cityluck(file);
    } else if (strcmp(command, "romewages") == 0 && argc > 3) {
        result = command_romewages(file, atoi(argv[3]));
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
    } else if (strcmp(command, "tradegame") == 0) {
        result = command_tradegame(file, argc > 3 ? atoi(argv[3]) : 3, argc > 4 ? (unsigned int) atoi(argv[4]) : 0);
    } else if (strcmp(command, "tradesave") == 0) {
        result = argc > 3 ? command_tradesave(file, argv[3]) : usage();
    } else if (strcmp(command, "tradewarehouse") == 0) {
        result = command_tradewarehouse(file, argc > 3 ? atoi(argv[3]) : 3, argc > 4 ? (unsigned int) atoi(argv[4]) : 0);
    } else if (strcmp(command, "tradestatus") == 0) {
        result = command_tradestatus(file, argc > 3 ? atoi(argv[3]) : 3, argc > 4 ? (unsigned int) atoi(argv[4]) : 0);
    } else if (strcmp(command, "traderesume") == 0) {
        result = command_traderesume(file);
    } else if (strcmp(command, "tradebounds") == 0) {
        result = command_tradebounds(file);
    } else if (strcmp(command, "caesarstate") == 0) {
        result = command_caesarstate(file);
    } else if (strcmp(command, "caesarlaurels") == 0) {
        result = command_caesarlaurels(file);
    } else if (strcmp(command, "salaries") == 0) {
        result = command_salaries(file);
    } else if (strcmp(command, "caesargifts") == 0) {
        result = command_caesargifts(file);
    } else if (strcmp(command, "caesarhistory") == 0) {
        result = command_caesarhistory(file);
    } else if (strcmp(command, "war") == 0) {
        result = command_war(file);
    } else if (strcmp(command, "wargate") == 0) {
        result = command_wargate(file);
    } else if (strcmp(command, "caesargate") == 0) {
        result = command_caesargate(file);
    } else if (strcmp(command, "waraqueduct") == 0) {
        result = command_waraqueduct(file);
    } else if (strcmp(command, "warattack") == 0) {
        result = command_warattack(file);
    } else if (strcmp(command, "attacksave") == 0) {
        result = argc > 3 ? command_attacksave(file, argv[3]) : usage();
    } else if (strcmp(command, "wargate3") == 0) {
        result = command_wargate3(file);
    } else if (strcmp(command, "warmotives") == 0) {
        result = command_warmotives(file);
    } else if (strcmp(command, "caesarwrath") == 0) {
        result = command_caesarwrath(file);
    } else if (strcmp(command, "armytolerance") == 0) {
        result = command_armytolerance(file);
    } else if (strcmp(command, "caesarexpedition") == 0) {
        result = command_caesarexpedition(file);
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
    } else if (strcmp(command, "caesarland") == 0 && argc > 3) {
        result = command_caesarland(file, atoi(argv[3]));
    } else if (strcmp(command, "caesarroads") == 0) {
        result = command_caesarroads(file);
    } else if (strcmp(command, "viewcorners") == 0 && argc > 3) {
        result = command_viewcorners(file, atoi(argv[3]));
    } else if (strcmp(command, "privatepopups") == 0 && argc > 3) {
        result = command_privatepopups(file, ticks);
    } else if (strcmp(command, "twins") == 0 && argc > 3) {
        result = command_twins(file, ticks);
    } else if (strcmp(command, "windowwrites") == 0) {
        result = command_windowwrites(file);
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
    } else if (strcmp(command, "restrictiveness") == 0) {
        result = command_restrictiveness(file, argc - 3, argv + 3);
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
