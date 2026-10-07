#include "mp_lobby.h"

#include "building/menu.h"
#include "building/building.h"
#include "city/map.h"
#include "city/view.h"
#include "core/time.h"
#include "core/dir.h"
#include "core/encoding.h"
#include "core/string.h"
#include "figure/figure.h"
#include "game/player_context.h"
#include "game/rules.h"
#include "graphics/button.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/scrollbar.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "map/data.h"
#include "map/grid.h"
#include "mp/caesar.h"
#include "mp/compose.h"
#include "mp/discovery.h"
#include "mp/endgame.h"
#include "mp/lobby.h"
#include "mp/lockstep.h"
#include "mp/mapgen.h"
#include "mp/missionary.h"
#include "mp/savegame.h"
#include "mp/session.h"
#include "platform/net.h"
#include "translation/translation.h"
#include "widget/input_box.h"
#include "window/city.h"
#include "window/main_menu.h"
#include "window/mp_results.h"

#include <stdio.h>
#include <string.h>

#define MAX_FILES 200
#define FILE_NAME_LENGTH 64
#define FILES_IN_VIEW 10
#define GAMES_IN_VIEW 6
#define ADDRESS_LENGTH 40

static void button_select_file(int index, int param2);
static void button_players(int change, int param2);
static void button_host(int param1, int param2);
static void button_select_game(int index, int param2);
static void button_join(int param1, int param2);
static void button_back(int param1, int param2);
static void on_scroll(void);
static void button_end(int param1, int param2);
static void button_invasions(int param1, int param2);
static void button_fog(int param1, int param2);
static void button_difficulty(int param1, int param2);
static void button_gods(int param1, int param2);
static void button_map(int param1, int param2);

static generic_button file_buttons[] = {
    {24, 104, 264, 16, button_select_file, button_none, 0, 0},
    {24, 120, 264, 16, button_select_file, button_none, 1, 0},
    {24, 136, 264, 16, button_select_file, button_none, 2, 0},
    {24, 152, 264, 16, button_select_file, button_none, 3, 0},
    {24, 168, 264, 16, button_select_file, button_none, 4, 0},
    {24, 184, 264, 16, button_select_file, button_none, 5, 0},
    {24, 200, 264, 16, button_select_file, button_none, 6, 0},
    {24, 216, 264, 16, button_select_file, button_none, 7, 0},
    {24, 232, 264, 16, button_select_file, button_none, 8, 0},
    {24, 248, 264, 16, button_select_file, button_none, 9, 0},
};

#define BUTTON_LESS 0
#define BUTTON_MORE 1
#define BUTTON_HOST 2
#define BUTTON_JOIN 3
#define BUTTON_BACK 4
#define BUTTON_END 5
#define BUTTON_INVASIONS 6
#define BUTTON_DIFFICULTY 7
#define BUTTON_GODS 8
#define BUTTON_FOG 9
#define BUTTON_MAP 10
#define NUM_ACTION_BUTTONS 11
static generic_button action_buttons[] = {
    {176, 280, 24, 20, button_players, button_none, -1, 0},
    {208, 280, 24, 20, button_players, button_none, 1, 0},
    {64, 394, 192, 25, button_host, button_none, 0, 0},
    {384, 348, 192, 25, button_join, button_none, 0, 0},
    {432, 440, 176, 25, button_back, button_none, 0, 0},
    {16, 326, 296, 20, button_end, button_none, 0, 0},
    {16, 348, 170, 20, button_invasions, button_none, 0, 0},
    {16, 304, 188, 20, button_difficulty, button_none, 0, 0},
    {208, 304, 104, 20, button_gods, button_none, 0, 0},
    {16, 370, 296, 20, button_fog, button_none, 0, 0},
    {190, 348, 136, 20, button_map, button_none, 0, 0},
};

static generic_button game_buttons[] = {
    {336, 104, 280, 20, button_select_game, button_none, 0, 0},
    {336, 124, 280, 20, button_select_game, button_none, 1, 0},
    {336, 144, 280, 20, button_select_game, button_none, 2, 0},
    {336, 164, 280, 20, button_select_game, button_none, 3, 0},
    {336, 184, 280, 20, button_select_game, button_none, 4, 0},
    {336, 204, 280, 20, button_select_game, button_none, 5, 0},
};

static scrollbar_type scrollbar = {296, 96, 176, 264, FILES_IN_VIEW, on_scroll, 1};

static struct {
    char files[MAX_FILES][FILE_NAME_LENGTH];
    int num_files;
    int selected_file;
    int num_players;
    int missing_template;
    uint8_t address[ADDRESS_LENGTH];
    char local_address[16];
    int focus_file;
    int focus_action;
    int focus_game;
} data;

static input_box address_input = {336, 264, 18, 2, FONT_NORMAL_WHITE, 1, data.address, ADDRESS_LENGTH};

static void add_files(const char *extension)
{
    const dir_listing *list = dir_find_files_with_extension(extension);
    for (int i = 0; i < list->num_files && data.num_files < MAX_FILES; i++) {
        // files written by network games are not starting points
        if (strncmp(list->files[i], "mp-", 3) == 0 || strncmp(list->files[i], "autosave", 8) == 0) {
            continue;
        }
        snprintf(data.files[data.num_files++], FILE_NAME_LENGTH, "%s", list->files[i]);
    }
}

// the first entry starts a new game on the prepared map (D-044); the others are multiplayer games to resume
#define NEW_GAME 0

static void init(void)
{
    data.num_files = 1;
    data.files[NEW_GAME][0] = 0;
    add_files("mpsav");
    add_files("mpmap");
    if (data.selected_file >= data.num_files) {
        data.selected_file = 0;
    }
    if (data.num_players < 1) {
        data.num_players = 2;
    }
    mp_lobby_rules_init();
    scrollbar_init(&scrollbar, 0, data.num_files);
    net_local_address(data.local_address);
    mp_discovery_start();
    input_box_start(&address_input);
}

static void draw_text_utf8(const char *text, int x, int y, font_t font)
{
    uint8_t converted[200];
    encoding_from_utf8(text, converted, sizeof(converted));
    text_draw(converted, x, y, font, 0);
}

static void draw_button(const generic_button *b, const uint8_t *text, int focused)
{
    button_border_draw(b->x, b->y, b->width, b->height, focused);
    text_draw_centered(text, b->x, b->y + (b->height - 10) / 2, b->width, FONT_NORMAL_BLACK, 0);
}

static void draw_background(void)
{
    window_draw_underlying_window();
}

static void draw_files(void)
{
    text_draw(translation_for(TR_MP_MAP), 16, 80, FONT_NORMAL_BLACK, 0);
    inner_panel_draw(16, 96, 18, 11);
    for (int i = 0; i < FILES_IN_VIEW && scrollbar.scroll_position + i < data.num_files; i++) {
        int index = scrollbar.scroll_position + i;
        font_t font = index == data.selected_file || data.focus_file == i + 1 ? FONT_NORMAL_WHITE : FONT_NORMAL_GREEN;
        uint8_t name[FILE_NAME_LENGTH];
        if (index == NEW_GAME) {
            string_copy(translation_for(TR_MP_NEW_GAME), name, FILE_NAME_LENGTH);
        } else {
            encoding_from_utf8(data.files[index], name, FILE_NAME_LENGTH);
        }
        text_ellipsize(name, font, 260);
        text_draw(name, 24, 106 + 16 * i, font, 0);
    }
    scrollbar_draw(&scrollbar);
}

static void draw_games(void)
{
    text_draw(translation_for(TR_MP_FOUND_GAMES), 336, 80, FONT_NORMAL_BLACK, 0);
    inner_panel_draw(328, 96, 18, 8);
    if (!mp_discovery_count()) {
        text_draw(translation_for(TR_MP_NO_GAME_FOUND), 344, 108, FONT_NORMAL_WHITE, 0);
    }
    for (int i = 0; i < GAMES_IN_VIEW && i < mp_discovery_count(); i++) {
        const mp_discovered_game *game = mp_discovery_get(i);
        char name[FILE_NAME_LENGTH];
        if (strcmp(game->map_name, MP_DISCOVERY_NEW_GAME) == 0) {
            encoding_to_utf8(translation_for(TR_MP_NEW_GAME), name, sizeof(name), 0);
        } else {
            snprintf(name, sizeof(name), "%s", game->map_name);
        }
        char line[128];
        snprintf(line, sizeof(line), "%s (%d/%d) %s", name, game->joined_players, game->num_players, game->address);
        draw_text_utf8(line, 344, 108 + 20 * i, data.focus_game == i + 1 ? FONT_NORMAL_WHITE : FONT_NORMAL_GREEN);
    }
}

static void draw_status(void)
{
    int width = text_draw(translation_for(TR_MP_YOUR_ADDRESS), 16, 428, FONT_NORMAL_BLACK, 0);
    draw_text_utf8(data.local_address, 16 + width, 428, FONT_NORMAL_BLACK);
    mp_lockstep_state state = mp_lockstep_get_state();
    if (state == MP_LOCKSTEP_OFF && data.missing_template) {
        text_draw(translation_for(TR_MP_NO_TEMPLATE), 16, 448, FONT_NORMAL_BLACK, 0);
    }
    if (state != MP_LOCKSTEP_OFF) {
        char status[200];
        if (state == MP_LOCKSTEP_WAITING_FOR_PLAYERS && mp_lockstep_connected_players()) {
            snprintf(status, sizeof(status), "%s (%d/%d)", mp_lockstep_status(), mp_lockstep_connected_players(),
                data.num_players);
        } else {
            snprintf(status, sizeof(status), "%s", mp_lockstep_status());
        }
        draw_text_utf8(status, 16, 448, FONT_NORMAL_BLACK);
    }
}

// a rule under the mouse is white, unless it is a rule of the host, which a player who joined cannot change
static font_t rule_font(int button, int editable)
{
    return editable && data.focus_action == button + 1 ? FONT_NORMAL_WHITE : FONT_NORMAL_BLACK;
}

static void draw_foreground(void)
{
    graphics_in_dialog();
    mp_discovery_poll();

    outer_panel_draw(0, 0, 40, 30);
    text_draw_centered(translation_for(TR_MP_LOBBY_TITLE), 0, 16, 640, FONT_LARGE_BLACK, 0);

    // hosting
    text_draw(translation_for(TR_MP_HOST_TITLE), 16, 56, FONT_NORMAL_BLACK, 0);
    draw_files();
    int width = text_draw(translation_for(TR_MP_PLAYERS), 16, 285, FONT_NORMAL_BLACK, 0);
    text_draw_number(data.num_players, 0, "", 16 + width, 285, FONT_NORMAL_BLACK);
    // a player who joined sees the rules of the host, and cannot change them (T4.11)
    const game_rules_settings *rules = mp_lobby_rules_shown();
    int editable = mp_lobby_rules_editable();
    font_t difficulty_font = rule_font(BUTTON_DIFFICULTY, editable);
    width = text_draw(translation_for(TR_MP_DIFFICULTY), 16, 308, difficulty_font, 0);
    text_draw(translation_for(TR_MP_DIFFICULTY_0 + rules->difficulty), 16 + width, 308, difficulty_font, 0);
    font_t gods_font = rule_font(BUTTON_GODS, editable);
    width = text_draw(translation_for(TR_MP_GODS), 212, 308, gods_font, 0);
    text_draw(translation_for(rules->gods_enabled ? TR_MP_YES : TR_MP_NO), 212 + width, 308, gods_font, 0);
    font_t end_font = rule_font(BUTTON_END, editable);
    width = text_draw(translation_for(TR_MP_END), 16, 330, end_font, 0);
    if (rules->end_condition == GAME_END_CAESAR) {
        width += text_draw(translation_for(TR_MP_END_CAESAR), 16 + width, 330, end_font, 0);
        width += text_draw_number(rules->caesar_score, 0, "", 16 + width, 330, end_font);
        text_draw(translation_for(TR_MP_LAURELS), 16 + width, 330, end_font, 0);
    } else {
        text_draw(translation_for(TR_MP_END_NONE), 16 + width, 330, end_font, 0);
    }
    font_t invasions_font = rule_font(BUTTON_INVASIONS, editable);
    width = text_draw(translation_for(TR_MP_INVASIONS), 16, 352, invasions_font, 0);
    text_draw(translation_for(rules->ai_invasions ? TR_MP_YES : TR_MP_NO), 16 + width, 352, invasions_font, 0);
    font_t fog_font = rule_font(BUTTON_FOG, editable);
    width = text_draw(translation_for(TR_MP_FOG_OF_WAR), 16, 374, fog_font, 0);
    text_draw(translation_for(rules->fog_of_war ? TR_MP_YES : TR_MP_NO), 16 + width, 374, fog_font, 0);
    // the prepared map of a new game: map 1, map 2 or drawn by lot (T4.14)
    int map_choice = rules->prepared_map >= GAME_MAP_1 && rules->prepared_map <= GAME_MAP_RANDOM ?
        rules->prepared_map : GAME_MAP_RANDOM;
    text_draw(translation_for(TR_MP_MAP_CHOICE_1 + map_choice), 194, 352, rule_font(BUTTON_MAP, editable), 0);
    draw_button(&action_buttons[BUTTON_LESS], string_from_ascii("-"), data.focus_action == BUTTON_LESS + 1);
    draw_button(&action_buttons[BUTTON_MORE], string_from_ascii("+"), data.focus_action == BUTTON_MORE + 1);
    // once hosting, the same button starts the game when every player is there
    int hosting = mp_lockstep_get_state() == MP_LOCKSTEP_WAITING_FOR_PLAYERS && mp_lockstep_is_host();
    draw_button(&action_buttons[BUTTON_HOST], translation_for(hosting ? TR_MP_START : TR_MP_HOST_BUTTON),
        data.focus_action == BUTTON_HOST + 1);

    // joining
    text_draw(translation_for(TR_MP_JOIN_TITLE), 336, 56, FONT_NORMAL_BLACK, 0);
    draw_games();
    text_draw(translation_for(TR_MP_ADDRESS), 336, 248, FONT_NORMAL_BLACK, 0);
    input_box_draw(&address_input);
    draw_button(&action_buttons[BUTTON_JOIN], translation_for(TR_MP_JOIN_BUTTON), data.focus_action == BUTTON_JOIN + 1);

    draw_status();
    draw_button(&action_buttons[BUTTON_BACK], translation_for(TR_MP_BACK), data.focus_action == BUTTON_BACK + 1);

    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);
    data.focus_file = data.focus_action = data.focus_game = 0;
    if (input_box_is_accepted(&address_input)) {
        button_join(0, 0);
        return;
    }
    if (scrollbar_handle_mouse(&scrollbar, m_dialog) ||
        input_box_handle_mouse(m_dialog, &address_input) ||
        generic_buttons_handle_mouse(m_dialog, 0, 0, file_buttons, FILES_IN_VIEW, &data.focus_file) ||
        generic_buttons_handle_mouse(m_dialog, 0, 0, game_buttons, GAMES_IN_VIEW, &data.focus_game) ||
        generic_buttons_handle_mouse(m_dialog, 0, 0, action_buttons, NUM_ACTION_BUTTONS, &data.focus_action)) {
        return;
    }
    if (input_go_back_requested(m, h)) {
        button_back(0, 0);
    }
}

static void on_scroll(void)
{
}

static void button_select_file(int index, int param2)
{
    if (scrollbar.scroll_position + index < data.num_files) {
        data.selected_file = scrollbar.scroll_position + index;
        // a multiplayer game goes on with as many players as it has cities
        int cities = data.selected_file == NEW_GAME ? 0 : mp_savegame_num_players(data.files[data.selected_file]);
        if (cities > 0) {
            data.num_players = cities;
        }
    }
}

static void button_players(int change, int param2)
{
    if (data.selected_file != NEW_GAME && mp_savegame_num_players(data.files[data.selected_file]) > 0) {
        return; // fixed by the saved game
    }
    data.num_players += change;
    if (data.num_players < 1) {
        data.num_players = 1;
    }
    if (data.num_players > MP_LOCKSTEP_MAX_PLAYERS) {
        data.num_players = MP_LOCKSTEP_MAX_PLAYERS;
    }
}

// while hosting, a change goes to the game and to the players already there (mp_lobby_change_rule)
static void button_end(int param1, int param2)
{
    mp_lobby_change_rule(MP_LOBBY_RULE_END);
}

static void button_fog(int param1, int param2)
{
    mp_lobby_change_rule(MP_LOBBY_RULE_FOG);
}

static void button_invasions(int param1, int param2)
{
    mp_lobby_change_rule(MP_LOBBY_RULE_INVASIONS);
}

static void button_difficulty(int param1, int param2)
{
    mp_lobby_change_rule(MP_LOBBY_RULE_DIFFICULTY);
}

static void button_gods(int param1, int param2)
{
    mp_lobby_change_rule(MP_LOBBY_RULE_GODS);
}

static void button_map(int param1, int param2)
{
    mp_lobby_change_rule(MP_LOBBY_RULE_MAP);
}

static void button_host(int param1, int param2)
{
    if (mp_lockstep_get_state() == MP_LOCKSTEP_WAITING_FOR_PLAYERS && mp_lockstep_is_host()) {
        if (mp_lockstep_connected_players() == data.num_players) {
            mp_lobby_start_game(); // with the rules of this moment (T4.11)
        }
        return;
    }
    if (mp_lockstep_get_state() != MP_LOCKSTEP_OFF) {
        return;
    }
    int new_game = data.selected_file == NEW_GAME;
    const char *file = new_game ? mp_mapgen_prepared_template() : data.files[data.selected_file];
    data.missing_template = !file;
    if (!file) {
        return;
    }
    game_rules_settings rules;
    mp_lobby_rules_settings(&rules);
    mp_lockstep_set_rules(&rules);
    mp_lockstep_set_started_callback(window_mp_lobby_show_started_game);
    if (mp_lockstep_host(MP_LOCKSTEP_DEFAULT_PORT, data.num_players, file, 1)) {
        mp_lockstep_set_manual_start(1);
        // only multiplayer maps (D-033, D-044): a new game is played on the prepared map for its number of players,
        // whose template only gives the empire, start year and funds; a multiplayer game or map goes on as saved
        mp_lockstep_set_generated_map(new_game, (unsigned int) time_get_millis());
    }
}

static void join(const char *address, int port)
{
    if (mp_lockstep_get_state() != MP_LOCKSTEP_OFF) {
        return;
    }
    mp_lockstep_set_started_callback(window_mp_lobby_show_started_game);
    mp_lockstep_join(address, port);
}

static void button_select_game(int index, int param2)
{
    const mp_discovered_game *game = mp_discovery_get(index);
    if (game) {
        join(game->address, game->port);
    }
}

static void button_join(int param1, int param2)
{
    char address[ADDRESS_LENGTH];
    encoding_to_utf8(data.address, address, ADDRESS_LENGTH, 0);
    if (address[0]) {
        join(address, MP_LOCKSTEP_DEFAULT_PORT);
    }
}

static void button_back(int param1, int param2)
{
    mp_lockstep_stop();
    mp_discovery_stop();
    input_box_stop(&address_input);
    window_main_menu_show(0);
}

void window_mp_lobby_show_started_game(void)
{
    mp_discovery_stop();
    input_box_stop(&address_input);
    building_menu_update(); // the buildings this city may build (interface state, rebuilt on loading)
    mp_endgame_set_over_callback(window_mp_results_show);
    // a new game: Caesar tells how he will judge (not again when a game goes on from a saved one)
    int laurels = 0;
    for (int p = 0; p < player_context_num_players(); p++) {
        laurels += mp_caesar_laurels(p);
    }
    if (!laurels) {
        mp_caesar_add_letter(MP_CAESAR_LETTER_WELCOME, 0);
    }
    // the view starts on the city of this player: its buildings, else its missionary, else its arrival point
    {
        int count = 0, x = 0, y = 0;
        for (int i = BUILDING_FIRST; i < BUILDING_END; i++) {
            building *b = building_get(i);
            if (b->state == BUILDING_STATE_IN_USE) {
                x += b->x;
                y += b->y;
                count++;
            }
        }
        int missionary = mp_missionary_first();
        if (count) {
            city_view_go_to_grid_offset(map_grid_offset(x / count, y / count));
        } else if (missionary) {
            city_view_go_to_grid_offset(figure_get(missionary)->grid_offset);
        } else {
            // the arrival point is on the edge of the map: look a little inside
            const map_tile *entry = city_map_entry_point();
            int dx = map_data.width / 2 - entry->x;
            int dy = map_data.height / 2 - entry->y;
            dx = dx > 40 ? 40 : dx < -40 ? -40 : dx;
            dy = dy > 40 ? 40 : dy < -40 ? -40 : dy;
            city_view_go_to_grid_offset(map_grid_offset(entry->x + dx, entry->y + dy));
        }
    }
    window_city_show();
}

void window_mp_lobby_show(void)
{
    window_type window = {
        WINDOW_MP_LOBBY,
        draw_background,
        draw_foreground,
        handle_input
    };
    init();
    window_show(&window);
}
