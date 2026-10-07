#include "window_sweep.h"

#include "building/building.h"
#include "building/menu.h"
#include "city/constants.h"
#include "figure/figure.h"
#include "game/player_context.h"
#include "game/resource.h"
#include "game/state.h"
#include "graphics/window.h"
#include "map/building.h"
#include "map/figure.h"
#include "map/grid.h"
#include "mp/caesar.h"
#include "mp/checksum.h"
#include "mp/session.h"
#include "window/advisors.h"
#include "window/build_menu.h"
#include "window/building_info.h"
#include "window/city.h"
#include "window/display_options.h"
#include "window/donate_to_city.h"
#include "window/empire.h"
#include "window/file_dialog.h"
#include "window/gift_to_emperor.h"
#include "window/hold_festival.h"
#include "window/labor_priority.h"
#include "window/message_list.h"
#include "window/military_menu.h"
#include "window/mp_caesar_letter.h"
#include "window/mp_results.h"
#include "window/mp_trade.h"
#include "window/overlay_menu.h"
#include "window/popup_dialog.h"
#include "window/resource_settings.h"
#include "window/set_salary.h"
#include "window/sound_options.h"
#include "window/speed_options.h"
#include "window/trade_prices.h"

#include <stdio.h>
#include <string.h>

#define MAX_PIECES 64
#define DRAWS_PER_WINDOW 3
#define TERRAIN_SAMPLES 400

typedef struct {
    int count;
    const char *names[MAX_PIECES];
    uint64_t checksums[MAX_PIECES];
} piece_list;

static struct {
    void (*log)(const char *message, const char *value);
    int changed;
    int windows;
} data;

static void record_piece(const char *name, uint64_t checksum, void *userdata)
{
    piece_list *list = userdata;
    if (list->count < MAX_PIECES) {
        list->names[list->count] = name; // names are string constants of the saved game format
        list->checksums[list->count] = checksum;
        list->count++;
    }
}

static void take(piece_list *list)
{
    list->count = 0;
    mp_checksum_state_pieces(record_piece, list);
}

static void draw_current_window(void)
{
    for (int i = 0; i < DRAWS_PER_WINDOW; i++) {
        window_draw(1);
    }
}

// compares the state before and after: the pieces that changed are logged with the name of the window
static void compare(const char *name, const piece_list *before, int paused_before)
{
    piece_list after;
    take(&after);
    char changed[200] = { 0 };
    for (int i = 0; i < before->count && i < after.count; i++) {
        if (before->checksums[i] != after.checksums[i] && strlen(changed) + strlen(before->names[i]) + 2 < sizeof(changed)) {
            strcat(changed, " ");
            strcat(changed, before->names[i]);
        }
    }
    if (before->count != after.count) {
        strcat(changed, " (number of pieces)");
    }
    if (game_state_is_paused() != paused_before) {
        strcat(changed, " (local pause)");
    }
    data.windows++;
    if (changed[0]) {
        char message[120];
        snprintf(message, sizeof(message), "windowsweep: %s changed the state:", name);
        data.log(message, changed);
        data.changed++;
    }
}

typedef void (*opener)(int param);

// opens one window from the city, draws it, goes back to the city
static void sweep(const char *name, opener open, int param)
{
    piece_list before;
    window_city_show();
    take(&before);
    int paused = game_state_is_paused();
    open(param);
    draw_current_window();
    window_city_show();
    compare(name, &before, paused);
}

static void open_advisor(int advisor)
{
    window_advisors_show_advisor(advisor);
}

static void open_trade_tab(int player_id)
{
    window_mp_trade_show_partner(player_id);
}

static void open_labor_priority(int category)
{
    window_advisors_show_advisor(ADVISOR_LABOR);
    window_labor_priority_show(category);
}

static void open_resource_settings(int resource)
{
    window_advisors_show_advisor(ADVISOR_TRADE);
    window_resource_settings_show(resource);
}

static void open_build_menu(int submenu)
{
    window_build_menu_show(submenu);
}

static void open_file_dialog(int type)
{
    window_file_dialog_show(FILE_TYPE_SAVED_GAME, type);
}

static void open_overlay(int overlay)
{
    game_state_set_overlay(overlay);
    window_city_show();
}

static void close_overlay(void)
{
    game_state_set_overlay(OVERLAY_NONE);
}

static void no_callback(int accepted)
{
}

static void back_to_city(void)
{
    window_city_show();
}

static void open_simple(int which)
{
    switch (which) {
        case 0: window_advisors_show_advisor(ADVISOR_IMPERIAL); window_set_salary_show(); break;
        case 1: window_advisors_show_advisor(ADVISOR_IMPERIAL); window_gift_to_emperor_show(); break;
        case 2: window_advisors_show_advisor(ADVISOR_IMPERIAL); window_donate_to_city_show(); break;
        case 3: window_advisors_show_advisor(ADVISOR_ENTERTAINMENT); window_hold_festival_show(); break;
        case 4: window_advisors_show_advisor(ADVISOR_TRADE); window_trade_prices_show(); break;
        case 5: window_empire_show(); break;
        case 6: window_message_list_show(); break;
        case 7: window_overlay_menu_show(); break;
        case 8: window_military_menu_show(); break;
        case 9: window_display_options_show(back_to_city); break;
        case 10: window_sound_options_show(back_to_city); break;
        case 11: window_speed_options_show(back_to_city); break;
        case 12: window_popup_dialog_show(POPUP_DIALOG_QUIT, no_callback, 1); break;
        case 13: window_mp_results_show(); break;
        default: break;
    }
}

static const char *SIMPLE_NAMES[] = {
    "salary", "gift to Caesar", "donation", "festival", "trade prices", "empire map", "message list",
    "overlay menu", "military menu", "display options", "sound options", "speed options", "quit dialog",
    "results of the game"
};

static void open_caesar_letter(int type)
{
    mp_caesar_add_letter(type, 1);
    window_mp_caesar_letter_show_pending();
}

// building information: every building of a type, of every player and of Caesar, in one pass per type
static int building_type_present(int type)
{
    for (int id = 1; id < BUILDING_ARRAY_SIZE; id++) {
        building *b = building_get(id);
        if (b->state == BUILDING_STATE_IN_USE && b->type == type) {
            return 1;
        }
    }
    return 0;
}

static void open_buildings_of_type(int type)
{
    for (int id = 1; id < BUILDING_ARRAY_SIZE; id++) {
        building *b = building_get(id);
        if (b->state != BUILDING_STATE_IN_USE || b->type != type || !map_grid_is_valid_offset(b->grid_offset)) {
            continue;
        }
        window_building_info_show(b->grid_offset);
        draw_current_window();
        window_city_show();
    }
}

static void open_figure_tiles(int param)
{
    int opened = 0;
    for (int id = 1; id < FIGURE_ARRAY_SIZE && opened < 300; id++) {
        figure *f = figure_get(id);
        if (f->state != FIGURE_STATE_ALIVE || !map_grid_is_valid_offset(f->grid_offset)) {
            continue;
        }
        window_building_info_show(f->grid_offset);
        draw_current_window();
        window_city_show();
        opened++;
    }
}

static void open_terrain_tiles(int param)
{
    int tiles = map_grid_width() * map_grid_height();
    for (int i = 0; i < TERRAIN_SAMPLES; i++) {
        int x = (int) ((i * 7919L) % map_grid_width());
        int y = (int) ((i * 104729L / 7) % map_grid_height());
        int grid_offset = map_grid_offset(x, y);
        if (tiles <= 0 || !map_grid_is_valid_offset(grid_offset)) {
            continue;
        }
        window_building_info_show(grid_offset);
        draw_current_window();
        window_city_show();
    }
}

int platform_window_sweep(void (*log)(const char *message, const char *value))
{
    data.log = log;
    data.changed = 0;
    data.windows = 0;
    static const char *ADVISOR_NAMES[] = {
        "", "labor advisor", "military advisor", "imperial advisor", "ratings advisor", "trade advisor",
        "population advisor", "health advisor", "education advisor", "entertainment advisor", "religion advisor",
        "financial advisor", "chief advisor"
    };
    for (int advisor = ADVISOR_LABOR; advisor <= ADVISOR_CHIEF; advisor++) {
        sweep(ADVISOR_NAMES[advisor], open_advisor, advisor);
    }
    if (window_mp_trade_is_active()) {
        char name[40];
        for (int p = 0; p < player_context_num_players(); p++) {
            snprintf(name, sizeof(name), "trade page of player %d", p + 1);
            sweep(name, open_trade_tab, p);
        }
        sweep("stock tab of the trade page", open_trade_tab, -1);
    }
    for (int category = 0; category < 9; category++) {
        sweep("labor priority", open_labor_priority, category);
    }
    for (int resource = RESOURCE_MIN; resource < RESOURCE_MAX; resource++) {
        sweep("resource settings", open_resource_settings, resource);
    }
    for (int i = 0; i < (int) (sizeof(SIMPLE_NAMES) / sizeof(SIMPLE_NAMES[0])); i++) {
        sweep(SIMPLE_NAMES[i], open_simple, i);
    }
    for (int submenu = 0; submenu < BUILD_MENU_MAX; submenu++) {
        sweep("build menu", open_build_menu, submenu);
    }
    sweep("save dialog", open_file_dialog, FILE_DIALOG_SAVE);
    sweep("load dialog", open_file_dialog, FILE_DIALOG_LOAD);
    sweep("delete dialog", open_file_dialog, FILE_DIALOG_DELETE);
    if (mp_caesar_is_active()) {
        sweep("letter of Caesar", open_caesar_letter, MP_CAESAR_LETTER_PROMOTION);
        while (mp_caesar_num_letters()) {
            mp_caesar_remove_first_letter();
        }
    }
    // the overlays of the overlay menu
    static const int OVERLAYS[] = {
        OVERLAY_WATER, OVERLAY_RELIGION, OVERLAY_FIRE, OVERLAY_DAMAGE, OVERLAY_CRIME, OVERLAY_ENTERTAINMENT,
        OVERLAY_THEATER, OVERLAY_AMPHITHEATER, OVERLAY_COLOSSEUM, OVERLAY_HIPPODROME, OVERLAY_EDUCATION,
        OVERLAY_SCHOOL, OVERLAY_LIBRARY, OVERLAY_ACADEMY, OVERLAY_BARBER, OVERLAY_BATHHOUSE, OVERLAY_CLINIC,
        OVERLAY_HOSPITAL, OVERLAY_TAX_INCOME, OVERLAY_FOOD_STOCKS, OVERLAY_DESIRABILITY,
        OVERLAY_NATIVE, OVERLAY_PROBLEMS
    };
    for (int i = 0; i < (int) (sizeof(OVERLAYS) / sizeof(OVERLAYS[0])); i++) {
        char name[40];
        snprintf(name, sizeof(name), "overlay %d", OVERLAYS[i]);
        sweep(name, open_overlay, OVERLAYS[i]);
    }
    close_overlay();
    for (int type = 1; type < BUILDING_TYPE_MAX; type++) {
        if (building_type_present(type)) {
            char name[48];
            snprintf(name, sizeof(name), "information of buildings of type %d", type);
            sweep(name, open_buildings_of_type, type);
        }
    }
    sweep("information of figures", open_figure_tiles, 0);
    sweep("information of terrain", open_terrain_tiles, 0);
    window_city_show();
    char summary[80];
    snprintf(summary, sizeof(summary), "%d windows opened, %d changed the state", data.windows, data.changed);
    data.log("windowsweep:", summary);
    return data.changed;
}
