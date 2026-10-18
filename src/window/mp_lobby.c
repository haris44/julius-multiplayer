#include "mp_lobby.h"

#include "building/menu.h"
#include "building/building.h"
#include "city/map.h"
#include "city/view.h"
#include "core/time.h"
#include "core/dir.h"
#include "core/encoding.h"
#include "core/file.h"
#include "core/lang.h"
#include "core/string.h"
#include "figure/figure.h"
#include "game/player_context.h"
#include "game/rules.h"
#include "graphics/arrow_button.h"
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
#include "platform/file_manager.h"
#include "platform/net.h"
#include "translation/translation.h"
#include "widget/input_box.h"
#include "window/city.h"
#include "window/main_menu.h"
#include "window/mp_results.h"

#include <stdio.h>
#include <string.h>

#define MAX_FILES 200
#define FILE_NAME_LENGTH FILE_NAME_MAX
#define ADDRESS_LENGTH 40

// The lobby is a 640 x 480 dialog in the idiom of the original windows (D-090): a column to host a game (its list,
// the number of players, the rules, "Host"), a column to join one (the games found, the address, "Join"), and under
// them a panel of white text with the rules in short and the state of the game being prepared. The rules themselves
// are chosen in a dialog over the lobby, like the options of the original game.

// the two lists, in inner panels of the same height, with rows of 16 pixels like the file dialog
#define LIST_Y 72
#define LIST_HEIGHT_BLOCKS 9
#define LIST_ROW_Y 80
#define LIST_ROW_HEIGHT 16
#define FILES_IN_VIEW 8
#define GAMES_IN_VIEW MP_DISCOVERY_MAX_GAMES
#define HOST_X 16
#define HOST_LIST_WIDTH_BLOCKS 16
#define JOIN_X 336
#define JOIN_LIST_WIDTH_BLOCKS 18

#define PLAYERS_Y 226
#define PLAYERS_ARROWS_X 208

// the panel of the state, under the two columns
#define STATE_Y 334
#define STATE_HEIGHT_BLOCKS 5
#define STATE_TEXT_X 32
#define STATE_TEXT_WIDTH 576

static void button_select_file(int index, int param2);
static void arrow_button_players(int is_down, int param2);
static void button_rules(int param1, int param2);
static void button_host(int param1, int param2);
static void button_select_game(int index, int param2);
static void button_join(int param1, int param2);
static void button_back(int param1, int param2);
static void button_rule(int rule, int param2);
static void button_rules_ok(int param1, int param2);
static void on_scroll(void);

static generic_button file_buttons[] = {
    {24, LIST_ROW_Y, 240, LIST_ROW_HEIGHT, button_select_file, button_none, 0, 0},
    {24, LIST_ROW_Y + 16, 240, LIST_ROW_HEIGHT, button_select_file, button_none, 1, 0},
    {24, LIST_ROW_Y + 32, 240, LIST_ROW_HEIGHT, button_select_file, button_none, 2, 0},
    {24, LIST_ROW_Y + 48, 240, LIST_ROW_HEIGHT, button_select_file, button_none, 3, 0},
    {24, LIST_ROW_Y + 64, 240, LIST_ROW_HEIGHT, button_select_file, button_none, 4, 0},
    {24, LIST_ROW_Y + 80, 240, LIST_ROW_HEIGHT, button_select_file, button_none, 5, 0},
    {24, LIST_ROW_Y + 96, 240, LIST_ROW_HEIGHT, button_select_file, button_none, 6, 0},
    {24, LIST_ROW_Y + 112, 240, LIST_ROW_HEIGHT, button_select_file, button_none, 7, 0},
};

static generic_button game_buttons[] = {
    {344, LIST_ROW_Y, 272, LIST_ROW_HEIGHT, button_select_game, button_none, 0, 0},
    {344, LIST_ROW_Y + 16, 272, LIST_ROW_HEIGHT, button_select_game, button_none, 1, 0},
    {344, LIST_ROW_Y + 32, 272, LIST_ROW_HEIGHT, button_select_game, button_none, 2, 0},
    {344, LIST_ROW_Y + 48, 272, LIST_ROW_HEIGHT, button_select_game, button_none, 3, 0},
    {344, LIST_ROW_Y + 64, 272, LIST_ROW_HEIGHT, button_select_game, button_none, 4, 0},
    {344, LIST_ROW_Y + 80, 272, LIST_ROW_HEIGHT, button_select_game, button_none, 5, 0},
    {344, LIST_ROW_Y + 96, 272, LIST_ROW_HEIGHT, button_select_game, button_none, 6, 0},
    {344, LIST_ROW_Y + 112, 272, LIST_ROW_HEIGHT, button_select_game, button_none, 7, 0},
};

#define BUTTON_RULES 0
#define BUTTON_HOST 1
#define BUTTON_JOIN 2
#define BUTTON_BACK 3
#define NUM_ACTION_BUTTONS 4
static generic_button action_buttons[] = {
    {64, 258, 192, 25, button_rules, button_none, 0, 0},
    {64, 294, 192, 25, button_host, button_none, 0, 0},
    {384, 294, 192, 25, button_join, button_none, 0, 0},
    {432, 432, 192, 25, button_back, button_none, 0, 0},
};

// the number of players: the arrows of the original options (down, then up)
static arrow_button player_arrows[] = {
    {0, 0, 17, 24, arrow_button_players, 1, 0},
    {24, 0, 15, 24, arrow_button_players, 0, 0},
};

// the dialog of the rules: one row per rule, its value on a label that gives the next value, as the original sound
// options do; OK closes it (so does a right click)
#define RULES_X 64
#define RULES_Y 56
#define RULES_WIDTH_BLOCKS 32
#define RULES_HEIGHT_BLOCKS 23
#define RULES_ROW_Y 136
#define RULES_LABEL_X 96
#define RULES_VALUE_X 288
#define RULES_VALUE_WIDTH_BLOCKS 16
#define RULES_VALUE_WIDTH (16 * RULES_VALUE_WIDTH_BLOCKS)
#define NUM_RULES 7
#define RULES_OK_Y 372
static generic_button rule_buttons[] = {
    {RULES_VALUE_X, RULES_ROW_Y, RULES_VALUE_WIDTH, 20, button_rule, button_none, MP_LOBBY_RULE_MAP, 0},
    {RULES_VALUE_X, RULES_ROW_Y + 30, RULES_VALUE_WIDTH, 20, button_rule, button_none, MP_LOBBY_RULE_DIFFICULTY, 0},
    {RULES_VALUE_X, RULES_ROW_Y + 60, RULES_VALUE_WIDTH, 20, button_rule, button_none, MP_LOBBY_RULE_GODS, 0},
    {RULES_VALUE_X, RULES_ROW_Y + 90, RULES_VALUE_WIDTH, 20, button_rule, button_none, MP_LOBBY_RULE_INVASIONS, 0},
    {RULES_VALUE_X, RULES_ROW_Y + 120, RULES_VALUE_WIDTH, 20, button_rule, button_none, MP_LOBBY_RULE_FOG, 0},
    {RULES_VALUE_X, RULES_ROW_Y + 150, RULES_VALUE_WIDTH, 20, button_rule, button_none, MP_LOBBY_RULE_END, 0},
    {RULES_VALUE_X, RULES_ROW_Y + 180, RULES_VALUE_WIDTH, 20, button_rule, button_none,
        MP_LOBBY_RULE_SECOND_EXPEDITION, 0},
    {224, RULES_OK_Y, 192, 20, button_rules_ok, button_none, 0, 0},
};

static scrollbar_type scrollbar = {
    HOST_X + 16 * HOST_LIST_WIDTH_BLOCKS, LIST_Y, 16 * LIST_HEIGHT_BLOCKS, 240, FILES_IN_VIEW, on_scroll, 1
};

static struct {
    char files[MAX_FILES][FILE_NAME_LENGTH];
    int num_files;
    int selected_file;
    int num_players;
    int joined_num_players; // the players of the game joined from the list, 0 when joined by address
    int missing_template;
    int failed; // hosting or joining just failed: the lobby shows why until the next try
    int rules_open;
    uint8_t address[ADDRESS_LENGTH];
    char local_address[16];
    int focus_file;
    int focus_action;
    int focus_game;
    int focus_rule;
} data;

// the first entry starts a new game on the prepared map (D-044); the others are multiplayer games to resume
#define NEW_GAME 0

static input_box address_input = {JOIN_X, 250, 18, 2, FONT_NORMAL_WHITE, 1, data.address, ADDRESS_LENGTH};

static int has_suffix(const char *name, const char *suffix)
{
    size_t length = strlen(name);
    size_t suffix_length = strlen(suffix);
    return length > suffix_length &&
        platform_file_manager_compare_filename(name + length - suffix_length, suffix) == 0;
}

// the multiplayer games and maps: by the end of their name, which may hold other dots ("partie 7.10.mpsav": the
// listing by extension reads from the first dot, T5.4)
static void add_files(void)
{
    const dir_listing *list = dir_find_files_with_extension(0);
    for (int i = 0; i < list->num_files && data.num_files < MAX_FILES; i++) {
        // the files written by network games for themselves are not starting points; their monthly saved game is
        // (autosave.mpsav, T5.3)
        if (strncmp(list->files[i], "mp-", 3) == 0 || strlen(list->files[i]) >= FILE_NAME_LENGTH ||
            (!has_suffix(list->files[i], ".mpsav") && !has_suffix(list->files[i], ".mpmap"))) {
            continue;
        }
        snprintf(data.files[data.num_files++], FILE_NAME_LENGTH, "%s", list->files[i]);
    }
}

// a multiplayer game goes on with as many players as it has cities
static void set_players_of_selected_file(void)
{
    int cities = data.selected_file == NEW_GAME ? 0 : mp_savegame_num_players(data.files[data.selected_file]);
    if (cities > 0) {
        data.num_players = cities;
    }
}

static void init(void)
{
    mp_lockstep_state state = mp_lockstep_get_state();
    if (state == MP_LOCKSTEP_RUNNING || state == MP_LOCKSTEP_DESYNC || state == MP_LOCKSTEP_DISCONNECTED) {
        // the game played before is over for this computer: the lobby hosts or joins again (T5.3)
        mp_lockstep_stop();
    }
    char selected[FILE_NAME_LENGTH];
    snprintf(selected, sizeof(selected), "%s", data.selected_file > NEW_GAME && data.selected_file < data.num_files ?
        data.files[data.selected_file] : "");
    data.num_files = 1;
    data.files[NEW_GAME][0] = 0;
    add_files();
    // the same file as before, where the list now has it (new saved games move it)
    data.selected_file = NEW_GAME;
    for (int i = 1; i < data.num_files && selected[0]; i++) {
        if (strcmp(data.files[i], selected) == 0) {
            data.selected_file = i;
        }
    }
    if (data.num_players < 1) {
        data.num_players = 2;
    }
    data.failed = 0;
    data.rules_open = 0;
    set_players_of_selected_file();
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

static void append(uint8_t *text, int size, const uint8_t *part)
{
    int length = string_length(text);
    if (length < size - 1) {
        string_copy(part, text + length, size - length);
    }
}

static void append_number(uint8_t *text, int size, int value)
{
    int length = string_length(text);
    if (length < size - 12) {
        string_from_int(text + length, value, 0);
    }
}

static int in_game(void)
{
    return mp_lockstep_get_state() != MP_LOCKSTEP_OFF;
}

static int is_joined(void)
{
    return in_game() && !mp_lockstep_is_host();
}

static int is_hosting(void)
{
    return mp_lockstep_get_state() == MP_LOCKSTEP_WAITING_FOR_PLAYERS && mp_lockstep_is_host();
}

// once hosting or joined, the number of players is the one of the game, read-only: a player who joined sees the
// number of the host, sent with its rules (a game joined from the list also gave it before: T4.11)
static int shown_players(void)
{
    if (!in_game()) {
        return data.num_players;
    }
    int players = mp_lockstep_lobby_num_players();
    if (!players && is_joined()) {
        players = data.joined_num_players;
    }
    return players;
}

// the number of players is chosen before hosting a new game: a saved game has its own
static int players_editable(void)
{
    return !in_game() &&
        (data.selected_file == NEW_GAME || mp_savegame_num_players(data.files[data.selected_file]) <= 0);
}

static int map_choice(const game_rules_settings *rules)
{
    return rules->prepared_map >= GAME_MAP_1 && rules->prepared_map <= GAME_MAP_RANDOM ?
        rules->prepared_map : GAME_MAP_RANDOM;
}

static void draw_files(void)
{
    inner_panel_draw(HOST_X, LIST_Y, HOST_LIST_WIDTH_BLOCKS, LIST_HEIGHT_BLOCKS);
    for (int i = 0; i < FILES_IN_VIEW && scrollbar.scroll_position + i < data.num_files; i++) {
        int index = scrollbar.scroll_position + i;
        font_t font = index == data.selected_file || data.focus_file == i + 1 ? FONT_NORMAL_WHITE : FONT_NORMAL_GREEN;
        uint8_t name[FILE_NAME_LENGTH];
        if (index == NEW_GAME) {
            string_copy(translation_for(TR_MP_NEW_GAME), name, FILE_NAME_LENGTH);
        } else {
            encoding_from_utf8(data.files[index], name, FILE_NAME_LENGTH);
        }
        text_ellipsize(name, font, file_buttons[i].width - 8);
        text_draw(name, file_buttons[i].x, file_buttons[i].y + 2, font, 0);
    }
    scrollbar_draw(&scrollbar);
}

static void draw_games(void)
{
    inner_panel_draw(JOIN_X, LIST_Y, JOIN_LIST_WIDTH_BLOCKS, LIST_HEIGHT_BLOCKS);
    if (!mp_discovery_count()) {
        text_draw(translation_for(TR_MP_NO_GAME_FOUND), game_buttons[0].x, game_buttons[0].y + 2,
            FONT_NORMAL_GREEN, 0);
    }
    int right = game_buttons[0].x + game_buttons[0].width;
    for (int i = 0; i < GAMES_IN_VIEW && i < mp_discovery_count(); i++) {
        const mp_discovered_game *game = mp_discovery_get(i);
        char name[FILE_NAME_LENGTH];
        if (strcmp(game->map_name, MP_DISCOVERY_NEW_GAME) == 0) {
            encoding_to_utf8(translation_for(TR_MP_NEW_GAME), name, sizeof(name), 0);
        } else {
            snprintf(name, sizeof(name), "%s", game->map_name);
        }
        // the players and the address of the host on the right, the name before them: a long name is shortened
        // rather than drawn over the border of the list
        font_t font = data.focus_game == i + 1 ? FONT_NORMAL_WHITE : FONT_NORMAL_GREEN;
        int y = game_buttons[i].y + 2;
        char line[64];
        snprintf(line, sizeof(line), "%d/%d  %s", game->joined_players, game->num_players, game->address);
        uint8_t converted[200];
        encoding_from_utf8(line, converted, sizeof(converted));
        int right_width = text_get_width(converted, font);
        text_draw(converted, right - right_width, y, font, 0);
        encoding_from_utf8(name, converted, sizeof(converted));
        text_ellipsize(converted, font, right - right_width - 12 - game_buttons[i].x);
        text_draw(converted, game_buttons[i].x, y, font, 0);
    }
}

// the players of the game: chosen before hosting a new game; while hosting, those already there
static void draw_players(void)
{
    int y = PLAYERS_Y + 6;
    if (is_hosting()) {
        // the host counts the players there, itself included
        int width = text_draw(translation_for(TR_MP_LOBBY_PLAYERS_HERE), HOST_X, y, FONT_NORMAL_BLACK, 0);
        width += text_draw_number(mp_lockstep_connected_players(), 0, "", HOST_X + width, y, FONT_NORMAL_BLACK);
        width += text_draw(translation_for(TR_MP_LOBBY_OF), HOST_X + width, y, FONT_NORMAL_BLACK, 0);
        text_draw_number(data.num_players, 0, "", HOST_X + width, y, FONT_NORMAL_BLACK);
        return;
    }
    int width = text_draw(translation_for(TR_MP_PLAYERS), HOST_X, y, FONT_NORMAL_BLACK, 0);
    int players = shown_players();
    if (players) {
        text_draw_number(players, 0, "", HOST_X + width, y, FONT_NORMAL_BLACK);
    }
    if (players_editable()) {
        arrow_buttons_draw(PLAYERS_ARROWS_X, PLAYERS_Y, player_arrows, 2);
    }
}

// the rules in a few words: the map, the difficulty and the end of the game, then the rules that differ from those
// the lobby proposes
static void compose_rules_summary(const game_rules_settings *rules, uint8_t *text, int size)
{
    text[0] = 0;
    append(text, size, translation_for(TR_MP_LOBBY_RULES_SUMMARY));
    append(text, size, translation_for(TR_MP_MAP_CHOICE_1 + map_choice(rules)));
    append(text, size, string_from_ascii(", "));
    append(text, size, translation_for(TR_MP_DIFFICULTY_0 + rules->difficulty));
    append(text, size, string_from_ascii(", "));
    if (rules->end_condition == GAME_END_CAESAR) {
        append(text, size, translation_for(TR_MP_END_CAESAR));
        // (a game saved before the score has 0: the default score)
        append_number(text, size, game_rules_settings_caesar_score(rules));
        append(text, size, translation_for(TR_MP_LAURELS));
    } else {
        append(text, size, translation_for(TR_MP_LOBBY_ENDLESS));
    }
    const int others[] = { // 0: as the lobby proposes
        rules->gods_enabled ? 0 : TR_MP_LOBBY_NO_GODS,
        rules->ai_invasions ? 0 : TR_MP_LOBBY_NO_INVASIONS,
        rules->fog_of_war ? 0 : TR_MP_LOBBY_NO_FOG,
        rules->rome_takes_province ? TR_MP_LOBBY_ROME_TAKES : 0
    };
    for (int i = 0; i < 4; i++) {
        if (others[i]) {
            append(text, size, string_from_ascii(", "));
            append(text, size, translation_for(others[i]));
        }
    }
}

static int draw_state_line_utf8(const char *text, int y)
{
    uint8_t converted[300];
    encoding_from_utf8(text, converted, sizeof(converted));
    return text_draw_multiline(converted, STATE_TEXT_X, y, STATE_TEXT_WIDTH, FONT_NORMAL_WHITE, 0);
}

// the panel of the state: the rules in short, then what happens now (the state of the network game)
static void draw_state(void)
{
    inner_panel_draw(HOST_X, STATE_Y, 38, STATE_HEIGHT_BLOCKS);
    uint8_t text[300];
    compose_rules_summary(mp_lobby_rules_shown(), text, sizeof(text));
    int y = STATE_Y + 10;
    y += text_draw_multiline(text, STATE_TEXT_X, y, STATE_TEXT_WIDTH, FONT_NORMAL_WHITE, 0);
    mp_lockstep_state state = mp_lockstep_get_state();
    if (state == MP_LOCKSTEP_OFF) {
        if (data.missing_template) {
            text_draw_multiline(translation_for(TR_MP_NO_TEMPLATE), STATE_TEXT_X, y, STATE_TEXT_WIDTH,
                FONT_NORMAL_WHITE, 0);
        } else if (data.failed) {
            draw_state_line_utf8(mp_lockstep_status(), y);
        } else {
            text_draw_multiline(translation_for(TR_MP_LOBBY_HINT), STATE_TEXT_X, y, STATE_TEXT_WIDTH,
                FONT_NORMAL_WHITE, 0);
        }
        return;
    }
    draw_state_line_utf8(mp_lockstep_status(), y);
}

static void draw_lobby(void)
{
    outer_panel_draw(0, 0, 40, 30);
    text_draw_centered(translation_for(TR_MP_LOBBY_TITLE), 0, 16, 640, FONT_LARGE_BLACK, 0);

    // hosting: a game of the list, its players and its rules
    text_draw(translation_for(TR_MP_HOST_TITLE), HOST_X, 52, FONT_NORMAL_BLACK, 0);
    draw_files();
    draw_players();
    draw_button(&action_buttons[BUTTON_RULES], translation_for(TR_MP_LOBBY_RULES),
        data.focus_action == BUTTON_RULES + 1);
    // once hosting, the same button starts the game when every player is there; a player who joined has no game
    // to host
    if (!is_joined()) {
        draw_button(&action_buttons[BUTTON_HOST], translation_for(is_hosting() ? TR_MP_START : TR_MP_HOST_BUTTON),
            data.focus_action == BUTTON_HOST + 1);
    }

    // joining: a game found on the network, or the address of its host
    text_draw(translation_for(TR_MP_JOIN_TITLE), JOIN_X, 52, FONT_NORMAL_BLACK, 0);
    draw_games();
    text_draw(translation_for(TR_MP_ADDRESS), JOIN_X, PLAYERS_Y + 6, FONT_NORMAL_BLACK, 0);
    input_box_draw(&address_input);
    if (!in_game()) {
        draw_button(&action_buttons[BUTTON_JOIN], translation_for(TR_MP_JOIN_BUTTON),
            data.focus_action == BUTTON_JOIN + 1);
    }

    draw_state();
    int width = text_draw(translation_for(TR_MP_YOUR_ADDRESS), HOST_X, 439, FONT_NORMAL_BLACK, 0);
    draw_text_utf8(data.local_address, HOST_X + width, 439, FONT_NORMAL_BLACK);
    draw_button(&action_buttons[BUTTON_BACK], translation_for(TR_MP_BACK), data.focus_action == BUTTON_BACK + 1);
}

static const uint8_t *rule_label(int rule)
{
    switch (rule) {
        case MP_LOBBY_RULE_MAP: return translation_for(TR_MP_MAP_KIND);
        case MP_LOBBY_RULE_DIFFICULTY: return translation_for(TR_MP_DIFFICULTY);
        case MP_LOBBY_RULE_GODS: return translation_for(TR_MP_GODS);
        case MP_LOBBY_RULE_INVASIONS: return translation_for(TR_MP_INVASIONS);
        case MP_LOBBY_RULE_FOG: return translation_for(TR_MP_FOG_OF_WAR);
        case MP_LOBBY_RULE_END: return translation_for(TR_MP_RULES_END);
        default: return translation_for(TR_MP_RULES_EXPEDITION);
    }
}

static const uint8_t *yes_no(int value)
{
    return translation_for(value ? TR_MP_RULES_YES : TR_MP_RULES_NO);
}

static void rule_value(const game_rules_settings *rules, int rule, uint8_t *text, int size)
{
    text[0] = 0;
    switch (rule) {
        case MP_LOBBY_RULE_MAP:
            append(text, size, translation_for(map_choice(rules) == GAME_MAP_RANDOM ? TR_MP_RULES_MAP_RANDOM :
                TR_MP_MAP_CHOICE_1 + map_choice(rules)));
            break;
        case MP_LOBBY_RULE_DIFFICULTY:
            // the names of the original difficulty dialog
            append(text, size, lang_get_string(153, rules->difficulty + 1));
            break;
        case MP_LOBBY_RULE_GODS:
            append(text, size, yes_no(rules->gods_enabled));
            break;
        case MP_LOBBY_RULE_INVASIONS:
            append(text, size, yes_no(rules->ai_invasions));
            break;
        case MP_LOBBY_RULE_FOG:
            append(text, size, yes_no(rules->fog_of_war));
            break;
        case MP_LOBBY_RULE_END:
            if (rules->end_condition == GAME_END_CAESAR) {
                append(text, size, translation_for(TR_MP_RULES_HEIR));
                append_number(text, size, game_rules_settings_caesar_score(rules));
                append(text, size, translation_for(TR_MP_LAURELS));
            } else {
                append(text, size, translation_for(TR_MP_RULES_ENDLESS));
            }
            break;
        default:
            append(text, size, translation_for(rules->rome_takes_province ? TR_MP_SECOND_EXPEDITION_ENDS :
                TR_MP_RULES_EXPEDITION_STRIKES));
            break;
    }
}

// a player who joined sees the rules of the host, a resumed game keeps those of its saved game (T4.11, D-073):
// their values are then written without a label to click
static void draw_rules(void)
{
    outer_panel_draw(RULES_X, RULES_Y, RULES_WIDTH_BLOCKS, RULES_HEIGHT_BLOCKS);
    int width = 16 * RULES_WIDTH_BLOCKS;
    text_draw_centered(translation_for(TR_MP_LOBBY_RULES), RULES_X, RULES_Y + 16, width, FONT_LARGE_BLACK, 0);
    int editable = mp_lobby_rules_editable();
    translation_key why = editable ? TR_MP_RULES_HOST_CHANGES :
        is_joined() ? TR_MP_RULES_OF_HOST : TR_MP_RULES_OF_SAVED_GAME;
    text_draw_centered(translation_for(why), RULES_X, RULES_Y + 50, width, FONT_NORMAL_BLACK, 0);
    const game_rules_settings *rules = mp_lobby_rules_shown();
    for (int i = 0; i < NUM_RULES; i++) {
        const generic_button *b = &rule_buttons[i];
        text_draw(rule_label(b->parameter1), RULES_LABEL_X, b->y + 5, FONT_NORMAL_BLACK, 0);
        uint8_t value[100];
        rule_value(rules, b->parameter1, value, sizeof(value));
        if (editable) {
            label_draw(b->x, b->y, RULES_VALUE_WIDTH_BLOCKS, data.focus_rule == i + 1 ? 1 : 2);
            text_draw_centered(value, b->x, b->y + 4, b->width, FONT_NORMAL_GREEN, 0);
        } else {
            text_draw_centered(value, b->x, b->y + 5, b->width, FONT_NORMAL_BLACK, 0);
        }
    }
    const generic_button *ok = &rule_buttons[NUM_RULES];
    label_draw(ok->x, ok->y, ok->width / 16, data.focus_rule == NUM_RULES + 1 ? 1 : 2);
    text_draw_centered(translation_for(TR_BUTTON_OK), ok->x, ok->y + 4, ok->width, FONT_NORMAL_GREEN, 0);
}

static void draw_background(void)
{
    window_draw_underlying_window();
}

static void draw_foreground(void)
{
    graphics_in_dialog();
    mp_discovery_poll();
    draw_lobby();
    if (data.rules_open) {
        draw_rules();
    }
    graphics_reset_dialog();
}

static void handle_rules_input(const mouse *m_dialog, const hotkeys *h)
{
    if (generic_buttons_handle_mouse(m_dialog, 0, 0, rule_buttons, NUM_RULES + 1, &data.focus_rule)) {
        return;
    }
    if (input_go_back_requested(m_dialog, h) || h->enter_pressed) {
        button_rules_ok(0, 0);
    }
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);
    data.focus_file = data.focus_action = data.focus_game = data.focus_rule = 0;
    if (data.rules_open) {
        input_box_is_accepted(&address_input); // a return closes the rules, it does not join
        handle_rules_input(m_dialog, h);
        return;
    }
    if (input_box_is_accepted(&address_input)) {
        button_join(0, 0);
        return;
    }
    if (scrollbar_handle_mouse(&scrollbar, m_dialog) ||
        input_box_handle_mouse(m_dialog, &address_input) ||
        generic_buttons_handle_mouse(m_dialog, 0, 0, file_buttons, FILES_IN_VIEW, &data.focus_file) ||
        generic_buttons_handle_mouse(m_dialog, 0, 0, game_buttons, GAMES_IN_VIEW, &data.focus_game) ||
        generic_buttons_handle_mouse(m_dialog, 0, 0, action_buttons, NUM_ACTION_BUTTONS, &data.focus_action) ||
        (players_editable() &&
        arrow_buttons_handle_mouse(m_dialog, PLAYERS_ARROWS_X, PLAYERS_Y, player_arrows, 2, 0))) {
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
        set_players_of_selected_file();
    }
}

static void arrow_button_players(int is_down, int param2)
{
    if (!players_editable()) {
        return; // hosting or joined: the number of players of the game is fixed; or fixed by the saved game
    }
    data.num_players += is_down ? -1 : 1;
    if (data.num_players < 1) {
        data.num_players = 1;
    }
    if (data.num_players > MP_LOCKSTEP_MAX_PLAYERS) {
        data.num_players = MP_LOCKSTEP_MAX_PLAYERS;
    }
}

static void button_rules(int param1, int param2)
{
    data.rules_open = 1;
    data.focus_rule = 0;
    input_box_pause(&address_input);
}

static void button_rules_ok(int param1, int param2)
{
    data.rules_open = 0;
    input_box_resume(&address_input);
}

// while hosting, a change goes to the game and to the players already there (mp_lobby_change_rule)
static void button_rule(int rule, int param2)
{
    mp_lobby_change_rule((mp_lobby_rule) rule);
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
        return; // a player who joined has no game to host
    }
    int new_game = data.selected_file == NEW_GAME;
    const char *file = new_game ? mp_mapgen_prepared_template() : data.files[data.selected_file];
    data.missing_template = !file;
    data.failed = 0;
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
    } else {
        data.failed = 1; // the port cannot be opened, the saved game cannot be read...: the lobby says it
    }
}

static void join(const char *address, int port)
{
    if (mp_lockstep_get_state() != MP_LOCKSTEP_OFF) {
        return;
    }
    mp_lockstep_set_started_callback(window_mp_lobby_show_started_game);
    data.missing_template = 0;
    data.failed = !mp_lockstep_join(address, port);
}

static void button_select_game(int index, int param2)
{
    const mp_discovered_game *game = mp_discovery_get(index);
    if (game && mp_lockstep_get_state() == MP_LOCKSTEP_OFF) {
        data.joined_num_players = game->num_players;
        join(game->address, game->port);
    }
}

static void button_join(int param1, int param2)
{
    char address[ADDRESS_LENGTH];
    encoding_to_utf8(data.address, address, ADDRESS_LENGTH, 0);
    if (address[0] && mp_lockstep_get_state() == MP_LOCKSTEP_OFF) {
        data.joined_num_players = 0;
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

static int select_file(const char *filename)
{
    for (int i = 1; i < data.num_files; i++) {
        if (platform_file_manager_compare_filename(data.files[i], filename) == 0) {
            data.selected_file = i;
            set_players_of_selected_file();
            if (data.selected_file >= scrollbar.scroll_position + FILES_IN_VIEW ||
                data.selected_file < scrollbar.scroll_position) {
                int last = data.num_files - FILES_IN_VIEW;
                scrollbar_reset(&scrollbar, data.selected_file < last ? data.selected_file : last > 0 ? last : 0);
            }
            return 1;
        }
    }
    return 0;
}

void window_mp_lobby_show_saved_game(const char *filename)
{
    window_mp_lobby_show();
    select_file(filename);
}

int window_mp_lobby_lists_file(const char *filename)
{
    for (int i = 1; i < data.num_files; i++) {
        if (platform_file_manager_compare_filename(data.files[i], filename) == 0) {
            return 1;
        }
    }
    return 0;
}

const char *window_mp_lobby_selected_file(void)
{
    return data.selected_file > NEW_GAME && data.selected_file < data.num_files ? data.files[data.selected_file] : "";
}

int window_mp_lobby_num_players(void)
{
    return data.num_players;
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
