#include "mp_lobby.h"

#include "building/menu.h"
#include "city/view.h"
#include "core/dir.h"
#include "core/encoding.h"
#include "core/string.h"
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
#include "map/grid.h"
#include "mp/compose.h"
#include "mp/discovery.h"
#include "mp/endgame.h"
#include "mp/lockstep.h"
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
#define FILES_IN_VIEW 12
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
    {24, 264, 264, 16, button_select_file, button_none, 10, 0},
    {24, 280, 264, 16, button_select_file, button_none, 11, 0},
};

#define BUTTON_LESS 0
#define BUTTON_MORE 1
#define BUTTON_HOST 2
#define BUTTON_JOIN 3
#define BUTTON_BACK 4
#define BUTTON_END 5
#define BUTTON_INVASIONS 6
#define NUM_ACTION_BUTTONS 7
static generic_button action_buttons[] = {
    {176, 310, 24, 20, button_players, button_none, -1, 0},
    {208, 310, 24, 20, button_players, button_none, 1, 0},
    {64, 382, 192, 25, button_host, button_none, 0, 0},
    {384, 348, 192, 25, button_join, button_none, 0, 0},
    {432, 440, 176, 25, button_back, button_none, 0, 0},
    {16, 334, 296, 20, button_end, button_none, 0, 0},
    {16, 356, 296, 20, button_invasions, button_none, 0, 0},
};

// end of the game: none, or by score after these years
static const int END_YEARS[] = { 0, 5, 10, 20 };
#define NUM_END_CHOICES 4

static generic_button game_buttons[] = {
    {336, 104, 280, 20, button_select_game, button_none, 0, 0},
    {336, 124, 280, 20, button_select_game, button_none, 1, 0},
    {336, 144, 280, 20, button_select_game, button_none, 2, 0},
    {336, 164, 280, 20, button_select_game, button_none, 3, 0},
    {336, 184, 280, 20, button_select_game, button_none, 4, 0},
    {336, 204, 280, 20, button_select_game, button_none, 5, 0},
};

static scrollbar_type scrollbar = {296, 96, 208, 264, FILES_IN_VIEW, on_scroll, 1};

static struct {
    char files[MAX_FILES][FILE_NAME_LENGTH];
    int num_files;
    int selected_file;
    int num_players;
    int end_choice;
    int ai_invasions;
    int rules_initialized;
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

static void init(void)
{
    data.num_files = 0;
    add_files("map");
    add_files("sav");
    if (data.selected_file >= data.num_files) {
        data.selected_file = 0;
    }
    if (data.num_players < 1) {
        data.num_players = 2;
    }
    if (!data.rules_initialized) {
        data.rules_initialized = 1;
        data.ai_invasions = 1;
    }
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
    inner_panel_draw(16, 96, 18, 13);
    if (!data.num_files) {
        text_draw(translation_for(TR_MP_NO_MAP), 24, 106, FONT_NORMAL_WHITE, 0);
    }
    for (int i = 0; i < FILES_IN_VIEW && scrollbar.scroll_position + i < data.num_files; i++) {
        int index = scrollbar.scroll_position + i;
        font_t font = index == data.selected_file || data.focus_file == i + 1 ? FONT_NORMAL_WHITE : FONT_NORMAL_GREEN;
        uint8_t name[FILE_NAME_LENGTH];
        encoding_from_utf8(data.files[index], name, FILE_NAME_LENGTH);
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
        char line[128];
        snprintf(line, sizeof(line), "%s (%d/%d) %s", game->map_name, game->joined_players, game->num_players,
            game->address);
        draw_text_utf8(line, 344, 108 + 20 * i, data.focus_game == i + 1 ? FONT_NORMAL_WHITE : FONT_NORMAL_GREEN);
    }
}

static void draw_status(void)
{
    int width = text_draw(translation_for(TR_MP_YOUR_ADDRESS), 16, 428, FONT_NORMAL_BLACK, 0);
    draw_text_utf8(data.local_address, 16 + width, 428, FONT_NORMAL_BLACK);
    mp_lockstep_state state = mp_lockstep_get_state();
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

static void draw_foreground(void)
{
    graphics_in_dialog();
    mp_discovery_poll();

    outer_panel_draw(0, 0, 40, 30);
    text_draw_centered(translation_for(TR_MP_LOBBY_TITLE), 0, 16, 640, FONT_LARGE_BLACK, 0);

    // hosting
    text_draw(translation_for(TR_MP_HOST_TITLE), 16, 56, FONT_NORMAL_BLACK, 0);
    draw_files();
    int width = text_draw(translation_for(TR_MP_PLAYERS), 16, 315, FONT_NORMAL_BLACK, 0);
    text_draw_number(data.num_players, 0, "", 16 + width, 315, FONT_NORMAL_BLACK);
    font_t end_font = data.focus_action == BUTTON_END + 1 ? FONT_NORMAL_WHITE : FONT_NORMAL_BLACK;
    width = text_draw(translation_for(TR_MP_END), 16, 338, end_font, 0);
    if (END_YEARS[data.end_choice]) {
        width += text_draw(translation_for(TR_MP_END_SCORE), 16 + width, 338, end_font, 0);
        width += text_draw_number(END_YEARS[data.end_choice], 0, "", 16 + width, 338, end_font);
        text_draw(translation_for(TR_MP_YEARS), 16 + width, 338, end_font, 0);
    } else {
        text_draw(translation_for(TR_MP_END_NONE), 16 + width, 338, end_font, 0);
    }
    font_t invasions_font = data.focus_action == BUTTON_INVASIONS + 1 ? FONT_NORMAL_WHITE : FONT_NORMAL_BLACK;
    width = text_draw(translation_for(TR_MP_INVASIONS), 16, 360, invasions_font, 0);
    text_draw(translation_for(data.ai_invasions ? TR_MP_YES : TR_MP_NO), 16 + width, 360, invasions_font, 0);
    draw_button(&action_buttons[BUTTON_LESS], string_from_ascii("-"), data.focus_action == BUTTON_LESS + 1);
    draw_button(&action_buttons[BUTTON_MORE], string_from_ascii("+"), data.focus_action == BUTTON_MORE + 1);
    draw_button(&action_buttons[BUTTON_HOST], translation_for(TR_MP_HOST_BUTTON), data.focus_action == BUTTON_HOST + 1);
    text_draw(translation_for(TR_MP_SEPARATE_CITIES), 16, 412, FONT_SMALL_PLAIN, 0);

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
    }
}

static void button_players(int change, int param2)
{
    data.num_players += change;
    if (data.num_players < 1) {
        data.num_players = 1;
    }
    if (data.num_players > MP_LOCKSTEP_MAX_PLAYERS) {
        data.num_players = MP_LOCKSTEP_MAX_PLAYERS;
    }
}

static void button_end(int param1, int param2)
{
    data.end_choice = (data.end_choice + 1) % NUM_END_CHOICES;
}

static void button_invasions(int param1, int param2)
{
    data.ai_invasions = !data.ai_invasions;
}

static void button_host(int param1, int param2)
{
    if (!data.num_files || mp_lockstep_get_state() != MP_LOCKSTEP_OFF) {
        return;
    }
    game_rules_settings rules;
    game_rules_default_multiplayer_settings(&rules);
    rules.ai_invasions = data.ai_invasions;
    rules.end_condition = END_YEARS[data.end_choice] ? GAME_END_SCORE : GAME_END_NONE;
    rules.score_years = END_YEARS[data.end_choice] ? END_YEARS[data.end_choice] : rules.score_years;
    mp_lockstep_set_rules(&rules);
    mp_lockstep_set_started_callback(window_mp_lobby_show_started_game);
    mp_lockstep_host(MP_LOCKSTEP_DEFAULT_PORT, data.num_players, data.files[data.selected_file], 1);
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
    // separate cities: the view starts on the city of this player
    if (player_context_num_players() > 1) {
        int x, y, size;
        mp_compose_city_area(mp_session_local_player_id(), MP_COMPOSE_CITY_GAP, &x, &y, &size);
        city_view_go_to_grid_offset(map_grid_offset(x + size / 2, y + size / 2));
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
