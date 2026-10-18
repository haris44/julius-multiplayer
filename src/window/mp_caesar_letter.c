#include "mp_caesar_letter.h"

#include "building/construction.h"
#include "core/image_group.h"
#include "core/lang.h"
#include "core/string.h"
#include "game/rules.h"
#include "game/time.h"
#include "graphics/button.h"
#include "graphics/graphics.h"
#include "graphics/image_button.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "mp/caesar.h"
#include "mp/caesar_rules.h"
#include "mp/expedition.h"
#include "mp/session.h"
#include "translation/translation.h"
#include "window/advisors.h"
#include "window/city.h"

#define TEXT_SIZE 1200
#define RANK_NAMES_GROUP 32

static void button_ok(int param1, int param2);
static void button_advisor(int param1, int param2);

static void append(uint8_t *text, const uint8_t *part)
{
    int length = string_length(text);
    string_copy(part, text + length, TEXT_SIZE - length);
}

static void append_number(uint8_t *text, int value)
{
    int length = string_length(text);
    if (length < TEXT_SIZE - 16) {
        string_from_int(text + length, value, 0);
    }
}

static int is_judged_to_the_score(void)
{
    return game_rules_end_condition() == GAME_END_CAESAR;
}

// "player 1, player 2 and player 3"
static void append_players(uint8_t *text, int players)
{
    int count = 0;
    for (int p = 0; p < 8; p++) {
        count += (players >> p) & 1;
    }
    int written = 0;
    for (int p = 0; p < 8; p++) {
        if (!((players >> p) & 1)) {
            continue;
        }
        if (written) {
            append(text, written == count - 1 ? translation_for(TR_MP_WAR_AND) : (const uint8_t *) ", ");
        }
        append(text, translation_for(TR_MP_LETTER_PLAYER));
        append_number(text, p + 1);
        written++;
    }
}

// the letters of Caesar's wrath (M10.5): the warning, the ultimatum, the peace he imposes
static int compose_wrath(const mp_caesar_letter *letter, uint8_t *text)
{
    switch (letter->type) {
        case MP_CAESAR_LETTER_WARNING:
            append(text, translation_for(TR_MP_LETTER_WARNING_1));
            append_players(text, letter->param);
            append(text, translation_for(TR_MP_LETTER_WARNING_2));
            return 1;
        case MP_CAESAR_LETTER_ULTIMATUM:
            append(text, translation_for(TR_MP_LETTER_ULTIMATUM_1));
            append_number(text, MP_CAESAR_ULTIMATUM_DELAY);
            append(text, translation_for(TR_MP_LETTER_ULTIMATUM_2));
            if (letter->param >= 0) {
                append(text, translation_for(TR_MP_LETTER_ULTIMATUM_3));
                append_number(text, letter->param + 1);
                append(text, translation_for(TR_MP_LETTER_ULTIMATUM_4));
            }
            return 1;
        case MP_CAESAR_LETTER_PEACE_IMPOSED:
            append(text, translation_for(TR_MP_LETTER_PEACE_IMPOSED_1));
            append_players(text, letter->param);
            append(text, translation_for(TR_MP_LETTER_PEACE_IMPOSED_2));
            return 1;
        default:
            return 0;
    }
}

// the punitive expedition (M10.6): the soldiers in the city of this player and what he lost, as the expedition left
// them (they change only with the next one)
static int compose_expedition(const mp_caesar_letter *letter, uint8_t *text)
{
    int local = mp_session_local_player_id();
    switch (letter->type) {
        case MP_CAESAR_LETTER_EXPEDITION: {
            append(text, translation_for(TR_MP_LETTER_EXPEDITION_1));
            append_number(text, mp_expedition_soldiers_landed(local));
            append(text, translation_for(TR_MP_LETTER_EXPEDITION_2));
            if (letter->param >= 0) {
                append(text, translation_for(TR_MP_LETTER_EXPEDITION_3));
                append_number(text, letter->param + 1);
                append(text, translation_for(TR_MP_LETTER_EXPEDITION_4));
                append_number(text, MP_CAESAR_CULPRIT_LOSS_PERCENT);
                append(text, translation_for(TR_MP_LETTER_EXPEDITION_5));
            }
            int lost = mp_expedition_laurels_lost(local) / 10;
            if (lost > 0) {
                append(text, translation_for(TR_MP_LETTER_EXPEDITION_6));
                append_number(text, lost);
                append(text, translation_for(TR_MP_LETTER_EXPEDITION_7));
            }
            append(text, translation_for(TR_MP_LETTER_EXPEDITION_8));
            append_number(text, MP_CAESAR_DISGRACE_MONTHS);
            append(text, translation_for(TR_MP_LETTER_EXPEDITION_9));
            append_number(text, MP_CAESAR_EXPEDITION_DEFEATED_LAURELS / 10);
            append(text, translation_for(TR_MP_LETTER_EXPEDITION_10));
            return 1;
        }
        case MP_CAESAR_LETTER_EXPEDITION_DEFEATED:
            append(text, translation_for(TR_MP_LETTER_EXPEDITION_DEFEATED_1));
            append_number(text, letter->param / 10);
            append(text, translation_for(TR_MP_LETTER_EXPEDITION_DEFEATED_2));
            return 1;
        case MP_CAESAR_LETTER_EXPEDITION_LEFT:
            append(text, translation_for(TR_MP_LETTER_EXPEDITION_LEFT));
            return 1;
        default:
            return 0;
    }
}

static void compose(const mp_caesar_letter *letter, uint8_t *text)
{
    text[0] = 0;
    if (compose_wrath(letter, text) || compose_expedition(letter, text)) {
        return;
    }
    if (letter->type == MP_CAESAR_LETTER_WELCOME) {
        if (is_judged_to_the_score()) {
            append(text, translation_for(TR_MP_LETTER_WELCOME_1));
            append_number(text, game_rules_caesar_score());
            append(text, translation_for(TR_MP_LETTER_WELCOME_2));
        } else {
            append(text, translation_for(TR_MP_LETTER_WELCOME_ENDLESS));
        }
        return;
    }
    // a new rank: its name in the texts of the game, then what is left to the score
    append(text, translation_for(TR_MP_LETTER_PROMOTION_1));
    append(text, lang_get_string(RANK_NAMES_GROUP, letter->param));
    if (is_judged_to_the_score()) {
        int left = game_rules_caesar_score() - mp_caesar_laurels(mp_session_local_player_id()) / 10;
        append(text, translation_for(TR_MP_LETTER_PROMOTION_2));
        append_number(text, left > 0 ? left : 0);
        append(text, translation_for(TR_MP_LETTER_PROMOTION_3));
    } else {
        append(text, translation_for(TR_MP_LETTER_PROMOTION_ENDLESS));
    }
}

// the letter is drawn as the messages of the Emperor in the original game (D-090): at their place, over the city
// and not the sidebar, with the title of its subject, the date and its recipient on top of the text, the button of
// the imperial advisor at the bottom left and the close button at the bottom right
#define LETTER_X 0
#define LETTER_Y 88
#define LETTER_WIDTH_BLOCKS 30
#define TEXT_X (LETTER_X + 16)
#define TEXT_WIDTH_BLOCKS 28
#define MIN_TEXT_HEIGHT_BLOCKS 14

static image_button advisor_button[] = {
    {0, 0, 27, 27, IB_NORMAL, GROUP_MESSAGE_ADVISOR_BUTTONS, 6, button_advisor, button_none, 0, 0, 1}
};
static image_button close_button[] = {
    {0, 0, 24, 24, IB_NORMAL, GROUP_CONTEXT_ICONS, 4, button_ok, button_none, 0, 0, 1}
};

static struct {
    int y;
    int height_blocks;
} layout;

static translation_key title_of(mp_caesar_letter_type type)
{
    switch (type) {
        case MP_CAESAR_LETTER_PROMOTION: return TR_MP_LETTER_TITLE_PROMOTION;
        case MP_CAESAR_LETTER_WARNING: return TR_MP_LETTER_TITLE_WARNING;
        case MP_CAESAR_LETTER_ULTIMATUM: return TR_MP_LETTER_TITLE_ULTIMATUM;
        case MP_CAESAR_LETTER_PEACE_IMPOSED: return TR_MP_LETTER_TITLE_PEACE;
        case MP_CAESAR_LETTER_EXPEDITION: return TR_MP_LETTER_TITLE_EXPEDITION;
        case MP_CAESAR_LETTER_EXPEDITION_DEFEATED: return TR_MP_LETTER_TITLE_EXPEDITION_DEFEATED;
        case MP_CAESAR_LETTER_EXPEDITION_LEFT: return TR_MP_LETTER_TITLE_EXPEDITION_LEFT;
        default: return TR_MP_LETTER_TITLE;
    }
}

// the text starts under the line of the date, as in the messages of the original game; a long letter makes the
// window taller rather than running out of its panel
static void update_layout(const uint8_t *text)
{
    int text_height = 56 + 16 * text_measure_multiline(text, 16 * (TEXT_WIDTH_BLOCKS - 1), FONT_NORMAL_WHITE, 0);
    int text_blocks = (text_height + 8 + 15) / 16;
    if (text_blocks < MIN_TEXT_HEIGHT_BLOCKS) {
        text_blocks = MIN_TEXT_HEIGHT_BLOCKS;
    }
    layout.height_blocks = text_blocks + 6;
    layout.y = LETTER_Y;
    if (layout.y + 16 * layout.height_blocks > 480) {
        layout.y = (480 - 16 * layout.height_blocks) / 2;
    }
}

static void draw_background(void)
{
    window_draw_underlying_window();
}

static void draw_foreground(void)
{
    const mp_caesar_letter *letter = mp_caesar_get_letter(0);
    if (!letter) {
        return;
    }
    static uint8_t text[TEXT_SIZE];
    compose(letter, text);
    update_layout(text);
    graphics_in_dialog();
    int y = layout.y;
    int text_y = y + 48;
    outer_panel_draw(LETTER_X, y, LETTER_WIDTH_BLOCKS, layout.height_blocks);
    text_draw_centered(translation_for(title_of(letter->type)), LETTER_X, y + 14, 16 * LETTER_WIDTH_BLOCKS,
        FONT_LARGE_BLACK, 0);
    inner_panel_draw(TEXT_X, text_y, TEXT_WIDTH_BLOCKS, layout.height_blocks - 6);
    // the date, then the recipient: the local player
    int width = lang_text_draw(25, game_time_month(), TEXT_X + 10, text_y + 6, FONT_NORMAL_WHITE);
    width += lang_text_draw_year(game_time_year(), TEXT_X + 12 + width, text_y + 6, FONT_NORMAL_WHITE);
    width += lang_text_draw(63, 5, TEXT_X + width + 60, text_y + 6, FONT_NORMAL_WHITE);
    width += text_draw(translation_for(TR_MP_PLAYER), TEXT_X + width + 60, text_y + 6, FONT_NORMAL_WHITE, 0);
    text_draw_number(mp_session_local_player_id() + 1, 0, "", TEXT_X + width + 60, text_y + 6, FONT_NORMAL_WHITE);
    text_draw_multiline(text, TEXT_X + 8, text_y + 56, 16 * (TEXT_WIDTH_BLOCKS - 1), FONT_NORMAL_WHITE, 0);
    int bottom = y + 16 * layout.height_blocks;
    image_buttons_draw(LETTER_X + 16, bottom - 40, advisor_button, 1);
    image_buttons_draw(LETTER_X + 16 * LETTER_WIDTH_BLOCKS - 38, bottom - 36, close_button, 1);
    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);
    int bottom = layout.y + 16 * layout.height_blocks;
    if (image_buttons_handle_mouse(m_dialog, LETTER_X + 16, bottom - 40, advisor_button, 1, 0) ||
        image_buttons_handle_mouse(m_dialog, LETTER_X + 16 * LETTER_WIDTH_BLOCKS - 38, bottom - 36,
            close_button, 1, 0)) {
        return;
    }
    if (input_go_back_requested(m, h) || h->enter_pressed) {
        button_ok(0, 0);
    }
}

static void button_ok(int param1, int param2)
{
    mp_caesar_remove_first_letter();
    window_city_show();
}

// the imperial advisor, as the button of the original messages of the Emperor
static void button_advisor(int param1, int param2)
{
    mp_caesar_remove_first_letter();
    if (!window_advisors_show_advisor(ADVISOR_IMPERIAL)) {
        window_city_show();
    }
}

void window_mp_caesar_letter_show_pending(void)
{
    if (!mp_caesar_num_letters() || !window_is(WINDOW_CITY) || building_construction_in_progress()) {
        return;
    }
    window_type window = {
        WINDOW_MP_CAESAR_LETTER,
        draw_background,
        draw_foreground,
        handle_input
    };
    window_show(&window);
}
