#include "mp_results.h"

#include "core/string.h"
#include "game/player_context.h"
#include "game/rules.h"
#include "graphics/button.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "mp/discovery.h"
#include "mp/endgame.h"
#include "mp/lockstep.h"
#include "mp/session.h"
#include "translation/translation.h"
#include "window/city.h"
#include "window/main_menu.h"

// The end of the game in the idiom of the original victory dialog (D-090): the title, the heir of Caesar named in
// large letters under a short line, the laurels of every city in a panel, best first, and the two choices on large
// labels.
#define PANEL_X 48
#define PANEL_WIDTH_BLOCKS 34
#define TABLE_X 64
#define TABLE_WIDTH_BLOCKS 32
#define TABLE_Y 124 // under the title, the line and the name of the heir
#define ROW_HEIGHT 20
#define LABEL_X 80
#define LABEL_WIDTH_BLOCKS 30

static void button_watch(int param1, int param2);
static void button_main_menu(int param1, int param2);

static generic_button buttons[] = {
    {LABEL_X, 0, 16 * LABEL_WIDTH_BLOCKS, 25, button_watch, button_none, 0, 0},
    {LABEL_X, 0, 16 * LABEL_WIDTH_BLOCKS, 25, button_main_menu, button_none, 0, 0},
};

static struct {
    int focus_button_id;
    int y;
    int height_blocks;
    int table_y;
    int table_height_blocks;
    int rule_y;
} data;

// the windows of 640 x 480 are centred: the rows of the table and a rule under it for the score
static void update_layout(int rome, int caesar)
{
    int rows = player_context_num_players();
    data.table_height_blocks = (rows * ROW_HEIGHT + 16 + 15) / 16;
    int height = TABLE_Y + 16 * data.table_height_blocks + 16;
    data.rule_y = 0;
    if (!rome && !caesar) {
        data.rule_y = height - 6;
        height += 26;
    }
    int buttons_y = height;
    height += 2 * 32 + 16;
    data.height_blocks = (height + 15) / 16;
    data.y = (480 - 16 * data.height_blocks) / 2;
    data.table_y = data.y + TABLE_Y;
    buttons[0].y = data.y + buttons_y;
    buttons[1].y = data.y + buttons_y + 32;
}

static void draw_background(void)
{
    window_draw_underlying_window();
}

static void draw_player_name(int player, int x, int y, font_t font, int centered_width)
{
    uint8_t name[64];
    int length = string_length(translation_for(TR_MP_PLAYER));
    string_copy(translation_for(TR_MP_PLAYER), name, sizeof(name));
    length += string_from_int(name + length, player + 1, 0);
    if (player == mp_session_local_player_id()) {
        name[length++] = ' ';
        string_copy(translation_for(TR_MP_YOU), name + length, sizeof(name) - length);
    }
    if (centered_width) {
        text_draw_centered(name, x, y, centered_width, font, 0);
    } else {
        text_draw(name, x, y, font, 0);
    }
}

static void draw_foreground(void)
{
    graphics_in_dialog();
    // a second punitive expedition, with the option of the lobby: no heir (M10.6, D-086)
    int rome = mp_endgame_rome_took_province();
    // with Caesar: the laurels of each city, the winner is his heir (doc/mp/CESAR.md §4.3)
    int caesar = game_rules_end_condition() == GAME_END_CAESAR;
    update_layout(rome, caesar);
    int width = 16 * PANEL_WIDTH_BLOCKS;
    outer_panel_draw(PANEL_X, data.y, PANEL_WIDTH_BLOCKS, data.height_blocks);
    text_draw_centered(translation_for(rome ? TR_MP_ROME_TAKES_TITLE : TR_MP_RESULTS_TITLE), PANEL_X, data.y + 16,
        width, FONT_LARGE_BLACK, 0);
    if (rome) {
        text_draw_multiline(translation_for(TR_MP_ROME_TAKES_RULE), LABEL_X, data.y + 52, 16 * LABEL_WIDTH_BLOCKS,
            FONT_NORMAL_BLACK, 0);
    } else {
        text_draw_centered(translation_for(caesar ? TR_MP_RESULTS_HEIR_IS : TR_MP_RESULTS_WINNER_IS), PANEL_X,
            data.y + 52, width, FONT_NORMAL_BLACK, 0);
        if (mp_endgame_winner() >= 0) {
            draw_player_name(mp_endgame_winner(), PANEL_X, data.y + 70, FONT_LARGE_BLACK, width);
        }
    }

    // the cities: the winner first (he wins a tie, as the end of the game counts them), then by laurels
    text_draw(translation_for(TR_MP_RESULTS_CITY), TABLE_X + 16, data.table_y - 12, FONT_SMALL_PLAIN, 0);
    text_draw(translation_for(caesar || rome ? TR_MP_NOTES_LAURELS : TR_MP_RESULTS_SCORE), TABLE_X + 288,
        data.table_y - 12, FONT_SMALL_PLAIN, 0);
    inner_panel_draw(TABLE_X, data.table_y, TABLE_WIDTH_BLOCKS, data.table_height_blocks);
    int num_players = player_context_num_players();
    int order[PLAYER_CONTEXT_MAX_PLAYERS];
    for (int p = 0; p < num_players; p++) {
        order[p] = p;
    }
    int winner = mp_endgame_winner();
    for (int i = 1; i < num_players; i++) {
        for (int j = i; j > 0; j--) {
            int a = order[j - 1], b = order[j];
            int b_first = b == winner || (a != winner && mp_endgame_score(b) > mp_endgame_score(a));
            if (!b_first) {
                break;
            }
            order[j - 1] = b;
            order[j] = a;
        }
    }
    for (int i = 0; i < num_players; i++) {
        int y = data.table_y + 10 + ROW_HEIGHT * i;
        draw_player_name(order[i], TABLE_X + 16, y, FONT_NORMAL_WHITE, 0);
        text_draw_number(mp_endgame_score(order[i]), 0, "", TABLE_X + 288, y, FONT_NORMAL_WHITE);
    }
    if (data.rule_y) {
        text_draw_centered(translation_for(TR_MP_SCORE_RULE), PANEL_X, data.y + data.rule_y, width,
            FONT_NORMAL_BLACK, 0);
    }

    for (int i = 0; i < 2; i++) {
        large_label_draw(buttons[i].x, buttons[i].y, LABEL_WIDTH_BLOCKS, data.focus_button_id == i + 1);
    }
    text_draw_centered(translation_for(TR_MP_WATCH), buttons[0].x, buttons[0].y + 6, buttons[0].width,
        FONT_NORMAL_GREEN, 0);
    text_draw_centered(translation_for(TR_MP_MAIN_MENU), buttons[1].x, buttons[1].y + 6, buttons[1].width,
        FONT_NORMAL_GREEN, 0);
    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    if (generic_buttons_handle_mouse(mouse_in_dialog(m), 0, 0, buttons, 2, &data.focus_button_id)) {
        return;
    }
    if (input_go_back_requested(m, h)) {
        button_watch(0, 0);
    }
}

static void button_watch(int param1, int param2)
{
    window_city_show();
}

static void button_main_menu(int param1, int param2)
{
    mp_lockstep_stop();
    window_main_menu_show(1);
}

void window_mp_results_show(void)
{
    window_type window = {
        WINDOW_MP_RESULTS,
        draw_background,
        draw_foreground,
        handle_input
    };
    data.focus_button_id = 0;
    window_show(&window);
}
