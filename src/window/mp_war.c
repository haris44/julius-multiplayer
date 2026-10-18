#include "mp_war.h"

#include "building/construction.h"
#include "core/image_group.h"
#include "core/string.h"
#include "game/player_context.h"
#include "game/rules.h"
#include "graphics/button.h"
#include "graphics/color.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/panel.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "graphics/tooltip.h"
#include "graphics/window.h"
#include "input/input.h"
#include "mp/actions.h"
#include "mp/battle.h"
#include "mp/colors.h"
#include "mp/session.h"
#include "mp/war.h"
#include "mp/wrath.h"
#include "translation/translation.h"
#include "window/advisors.h"
#include "window/city.h"

#define MAX_OTHERS (PLAYER_CONTEXT_MAX_PLAYERS - 1)

// "War and peace": a page in the frame of the military advisor, over it, in its idiom (icon and title, column headers,
// one framed row per player in an inner panel, bullets under it)
#define PANEL_Y 0
#define PANEL_HEIGHT_BLOCKS 26
#define HEADER_Y (PANEL_Y + 58)
#define TABLE_Y (PANEL_Y + 72)
#define TABLE_HEIGHT_BLOCKS 11
#define ROW_X 36
#define ROW_WIDTH 568
#define ROW_HEIGHT 52
#define ROW_SPACING 56
#define FIRST_ROW_Y (TABLE_Y + 6)
#define TEXT_X 44
#define STATE_X 124
#define LINE_1_DY 10
#define LINE_2_DY 32
#define BUTTON_DY 5
#define BUTTON_HEIGHT 25
#define BRUTAL_X 278
#define BRUTAL_WIDTH 84
#define HONOURABLE_X 366
#define HONOURABLE_WIDTH 100
#define PEACE_X 470
#define PEACE_WIDTH 130
#define RULES_Y (TABLE_Y + 16 * TABLE_HEIGHT_BLOCKS + 14)
#define RULE_SPACING 20
#define OK_X 256
#define OK_Y (PANEL_Y + 16 * PANEL_HEIGHT_BLOCKS - 56)
#define OK_WIDTH 128

#define ROW_Y(row) (FIRST_ROW_Y + ROW_SPACING * (row))

enum {
    ACTION_BRUTAL = 0,
    ACTION_HONOURABLE = 1,
    ACTION_PEACE = 2,
    ACTIONS_PER_ROW = 3
};

static void button_war(int row, int action);
static void button_ok(int param1, int param2);

#define ROW_BUTTONS(row) \
    {BRUTAL_X, ROW_Y(row) + BUTTON_DY, BRUTAL_WIDTH, BUTTON_HEIGHT, button_war, button_none, row, ACTION_BRUTAL}, \
    {HONOURABLE_X, ROW_Y(row) + BUTTON_DY, HONOURABLE_WIDTH, BUTTON_HEIGHT, button_war, button_none, row, \
        ACTION_HONOURABLE}, \
    {PEACE_X, ROW_Y(row) + BUTTON_DY, PEACE_WIDTH, BUTTON_HEIGHT, button_war, button_none, row, ACTION_PEACE}

static generic_button buttons[] = {
    ROW_BUTTONS(0),
    ROW_BUTTONS(1),
    ROW_BUTTONS(2),
    {OK_X, OK_Y, OK_WIDTH, BUTTON_HEIGHT, button_ok, button_none, 0, 0},
};
#define NUM_BUTTONS (sizeof(buttons) / sizeof(generic_button))
#define OK_BUTTON (NUM_BUTTONS - 1)

static struct {
    int focus_button_id;
    int armed_player;  // a declaration waits for its second click: the other player, or -1
    int armed_form;
    uint8_t tooltip[400];
} data;

static void append(uint8_t *text, int size, const uint8_t *part)
{
    int length = string_length(text);
    string_copy(part, text + length, size - length);
}

static void append_number(uint8_t *text, int size, int value)
{
    int length = string_length(text);
    if (length < size - 16) {
        string_from_int(text + length, value, 0);
    }
}

int window_mp_war_is_available(void)
{
    return game_rules_is_multiplayer() && player_context_num_players() > 1;
}

// another player who proposed peace to the local player, still waiting for his answer, or -1
static int peace_offered_to_local(void)
{
    int local = mp_session_local_player_id();
    for (int p = 0; p < player_context_num_players(); p++) {
        if (p != local && mp_war_peace_proposed(p, local) && !mp_war_peace_proposed(local, p)) {
            return p;
        }
    }
    return -1;
}

int window_mp_war_advisor_text(uint8_t *text, int size)
{
    text[0] = 0;
    if (!window_mp_war_is_available()) {
        return 0;
    }
    int local = mp_session_local_player_id();
    int offer = peace_offered_to_local();
    int notice = -1;
    if (offer >= 0) {
        // the way to peace first: "Player 2 offers you peace" (the button "War and peace" signs it)
        mp_war_compose(text, size, translation_for(TR_MP_MILITARY_PEACE_OFFERED), offer, -1);
    } else {
        for (int p = 0; p < player_context_num_players(); p++) {
            if (p == local) {
                continue;
            }
            if (mp_war_is_fighting(local, p)) {
                // "At war with player 2", ", 3" for each other enemy
                if (!text[0]) {
                    string_copy(translation_for(TR_MP_MILITARY_AT_WAR), text, size);
                } else {
                    append(text, size, (const uint8_t *) ", ");
                }
                append_number(text, size, p + 1);
            } else if (notice < 0 && mp_war_status(local, p) == MP_WAR_NOTICE) {
                notice = p;
            }
        }
    }
    int enemies = mp_battle_enemy_soldiers_in_land(local);
    if (text[0] && enemies > 0 && string_length(text) < size - 48) {
        // "...; enemy soldiers in the city: 16" (M10.8)
        append(text, size, translation_for(TR_MP_MILITARY_ENEMIES_IN_CITY));
        append_number(text, size, enemies);
    }
    if (!text[0] && notice >= 0) {
        // "Notice: fighting with player 2 in 30 days"
        string_copy(translation_for(TR_MP_MILITARY_NOTICE), text, size);
        append_number(text, size, notice + 1);
        append(text, size, translation_for(TR_MP_MILITARY_IN));
        append_number(text, size, mp_war_days_until_fighting(local, notice));
        append(text, size, translation_for(TR_MP_WAR_DAYS));
    }
    return text[0] != 0;
}

// "8 soldiers, 1 legion" (no legion: "8 soldiers")
static void append_losses(uint8_t *text, int size, int soldiers, int legions)
{
    if (string_length(text) >= size - 40) {
        return;
    }
    append_number(text, size, soldiers);
    append(text, size, translation_for(soldiers == 1 ? TR_MP_MILITARY_SOLDIER : TR_MP_MILITARY_SOLDIERS));
    if (legions > 0) {
        append(text, size, (const uint8_t *) ", ");
        append_number(text, size, legions);
        append(text, size, translation_for(legions == 1 ? TR_MP_MILITARY_LEGION : TR_MP_MILITARY_LEGIONS));
    }
}

int window_mp_war_advisor_losses_text(uint8_t *text, int size)
{
    text[0] = 0;
    if (!window_mp_war_is_available()) {
        return 0;
    }
    int local = mp_session_local_player_id();
    int lost = mp_battle_soldiers_lost(local);
    int killed = mp_battle_enemy_soldiers_killed(local);
    if (!lost && !killed) {
        return 0;
    }
    string_copy(translation_for(TR_MP_MILITARY_LOSSES), text, size);
    append_losses(text, size, lost, mp_battle_legions_lost(local));
    append(text, size, translation_for(TR_MP_MILITARY_ENEMY_LOSSES));
    append_losses(text, size, killed, mp_battle_enemy_legions_destroyed(local));
    return 1;
}

// the other player shown on a row, or -1
static int player_of_row(int row)
{
    int local = mp_session_local_player_id();
    int index = 0;
    for (int p = 0; p < player_context_num_players(); p++) {
        if (p == local) {
            continue;
        }
        if (index == row) {
            return p;
        }
        index++;
    }
    return -1;
}

static int action_available(int other, int action)
{
    int status = mp_war_status(mp_session_local_player_id(), other);
    switch (action) {
        case ACTION_BRUTAL:
            return status != MP_WAR_FIGHTING; // during the notice too: the defender strikes first
        case ACTION_HONOURABLE:
            // during the notice, the declarer may withdraw his declaration instead (D-080)
            return status == MP_WAR_PEACE || mp_war_may_withdraw(mp_session_local_player_id(), other);
        case ACTION_PEACE:
            return status != MP_WAR_PEACE;
        default:
            return 0;
    }
}

// "At peace" in green, "Notice: 46 days" and "At war" in red, as the chief advisor tells what goes well or not
static void draw_state(int local, int other, int y)
{
    int status = mp_war_status(local, other);
    if (status == MP_WAR_PEACE) {
        text_draw(translation_for(TR_MP_WAR_STATUS_PEACE), STATE_X, y, FONT_NORMAL_GREEN, 0);
    } else if (status == MP_WAR_NOTICE) {
        uint8_t text[64] = { 0 };
        append(text, sizeof(text), translation_for(TR_MP_WAR_STATUS_NOTICE));
        append_number(text, sizeof(text), mp_war_days_until_fighting(local, other));
        append(text, sizeof(text), translation_for(TR_MP_WAR_DAYS));
        text_draw(text, STATE_X, y, FONT_NORMAL_RED, 0);
    } else {
        text_draw(translation_for(TR_MP_WAR_STATUS_FIGHTING), STATE_X, y, FONT_NORMAL_RED, 0);
    }
}

// ---------- Caesar's judgement (M10.4, D-084) ----------

// laurels from tenths: "-15", or "-1,5"
static void append_laurels(uint8_t *text, int size, int tenths)
{
    int length = string_length(text);
    if (length >= size - 16) {
        return;
    }
    uint8_t *out = text + length;
    if (tenths < 0) {
        *out++ = '-';
        tenths = -tenths;
    }
    out += string_from_int(out, tenths / 10, 0);
    if (tenths % 10) {
        *out++ = ',';
        out += string_from_int(out, tenths % 10, 0);
    }
    *out = 0;
}

static translation_key motive_name(int motive)
{
    switch (motive) {
        case MP_WAR_MOTIVE_RETALIATION:
            return TR_MP_WRATH_MOTIVE_RETALIATION;
        case MP_WAR_MOTIVE_MANDATE:
            return TR_MP_WRATH_MOTIVE_MANDATE;
        default:
            return TR_MP_WRATH_MOTIVE_UNJUST;
    }
}

// what a declaration of the local player would be worth to Caesar: "Without motive: -15 laurels when the fighting
// starts", "Retaliation: just; brutal: -15 laurels at once"; `full`: why the wrath of a war declared again is double
static void compose_declaration(uint8_t *text, int size, int local, int other, int form, int full)
{
    int redeclared = 0;
    int motive = mp_wrath_motive_if_declared(local, other, &redeclared);
    int price = mp_wrath_war_price(motive, form);
    append(text, size, translation_for(motive_name(motive)));
    if (motive == MP_WAR_MOTIVE_UNJUST) {
        append(text, size, translation_for(TR_MP_WRATH_PRICE));
        append_laurels(text, size, price);
        append(text, size, translation_for(form == MP_WAR_BRUTAL ?
            TR_MP_WRATH_LAURELS_AT_ONCE : TR_MP_WRATH_LAURELS_AT_FIGHTING));
        if (redeclared) {
            append(text, size, translation_for(full ? TR_MP_WRATH_REDECLARED : TR_MP_WAR_DOUBLE_WRATH));
        }
    } else {
        append(text, size, translation_for(TR_MP_WRATH_JUST));
        if (form == MP_WAR_BRUTAL) {
            append(text, size, translation_for(TR_MP_WRATH_BRUTAL_PRICE));
            append_laurels(text, size, price);
            append(text, size, translation_for(TR_MP_WRATH_LAURELS_AT_ONCE));
        }
    }
}

// the motive of a war declared and its month: "Motive: without motive, month of war: 1"; during the notice, what the
// declarer will pay when the fighting starts
static void compose_war_motive(uint8_t *text, int size, int local, int other)
{
    int status = mp_war_status(local, other);
    append(text, size, translation_for(TR_MP_WRATH_WAR_MOTIVE));
    append(text, size, translation_for(motive_name(mp_wrath_motive(local, other))));
    if (status == MP_WAR_FIGHTING) {
        append(text, size, translation_for(TR_MP_WRATH_MONTH_OF_WAR));
        append_number(text, size, mp_wrath_months_fighting(local, other) + 1); // the month going on
    } else if (mp_war_declarer(local, other) == local && mp_wrath_motive(local, other) == MP_WAR_MOTIVE_UNJUST) {
        append(text, size, translation_for(TR_MP_WRATH_PRICE));
        append_laurels(text, size, mp_wrath_war_price(MP_WAR_MOTIVE_UNJUST, MP_WAR_HONOURABLE));
        append(text, size, translation_for(TR_MP_WRATH_LAURELS_AT_FIGHTING));
    }
}

// the second line of a row: before a declaration, its motive and its price (the form whose button waits for its
// second click, honourable otherwise); during a war, the proposals of peace, else its motive and its month
static void draw_detail(int local, int other, int y)
{
    uint8_t text[200] = { 0 };
    int status = mp_war_status(local, other);
    font_t font = FONT_NORMAL_WHITE;
    if (data.armed_player == other) {
        compose_declaration(text, sizeof(text), local, other, data.armed_form, 0);
    } else if (status == MP_WAR_PEACE) {
        compose_declaration(text, sizeof(text), local, other, MP_WAR_HONOURABLE, 0);
    } else if (mp_war_peace_proposed(other, local)) {
        append(text, sizeof(text), translation_for(TR_MP_WAR_PEACE_OFFERED));
        font = FONT_NORMAL_GREEN;
    } else if (mp_war_peace_proposed(local, other)) {
        append(text, sizeof(text), translation_for(TR_MP_WAR_PEACE_SENT));
    } else {
        compose_war_motive(text, sizeof(text), local, other);
    }
    text_draw(text, TEXT_X, y, font, 0);
}

static translation_key button_label(int local, int other, int action)
{
    if (action != ACTION_PEACE && data.armed_player == other &&
        data.armed_form == (action == ACTION_BRUTAL ? MP_WAR_BRUTAL : MP_WAR_HONOURABLE)) {
        return TR_MP_WAR_CONFIRM;
    } else if (action == ACTION_BRUTAL) {
        return TR_MP_WAR_BRUTAL;
    } else if (action == ACTION_HONOURABLE) {
        return mp_war_may_withdraw(local, other) ? TR_MP_WAR_WITHDRAW : TR_MP_WAR_HONOURABLE;
    } else if (mp_war_peace_proposed(local, other)) {
        return TR_MP_WAR_WITHDRAW_PEACE;
    } else if (mp_war_peace_proposed(other, local)) {
        return TR_MP_WAR_ACCEPT_PEACE;
    }
    return TR_MP_WAR_PROPOSE_PEACE;
}

static void draw_row(int row, int other, int local)
{
    int y = ROW_Y(row);
    button_border_draw(ROW_X, y, ROW_WIDTH, ROW_HEIGHT, 0);
    uint8_t name[32] = { 0 };
    append(name, sizeof(name), translation_for(TR_MP_PLAYER));
    append_number(name, sizeof(name), other + 1);
    text_draw(name, TEXT_X, y + LINE_1_DY, FONT_NORMAL_PLAIN, mp_colors_player(other));
    draw_state(local, other, y + LINE_1_DY);
    for (int action = ACTION_BRUTAL; action < ACTIONS_PER_ROW; action++) {
        if (!action_available(other, action)) {
            continue;
        }
        int index = ACTIONS_PER_ROW * row + action;
        const generic_button *b = &buttons[index];
        button_border_draw(b->x, b->y, b->width, b->height, data.focus_button_id == index + 1);
        text_draw_centered(translation_for(button_label(local, other, action)), b->x, b->y + 6, b->width,
            FONT_NORMAL_WHITE, 0);
    }
    draw_detail(local, other, y + LINE_2_DY);
}

static void draw_rule(translation_key key, int y)
{
    image_draw(image_group(GROUP_BULLET), 40, y + 1);
    text_draw(translation_for(key), 60, y, FONT_NORMAL_BLACK, 0);
}

static void draw_background(void)
{
    window_draw_underlying_window();
    graphics_reset_clip_rectangle();
    graphics_shade_rect(0, 0, screen_width(), screen_height(), 0);
}

static void draw_foreground(void)
{
    graphics_in_dialog();
    outer_panel_draw(0, PANEL_Y, 40, PANEL_HEIGHT_BLOCKS);
    image_draw(image_group(GROUP_ADVISOR_ICONS) + 1, 10, PANEL_Y + 10);
    text_draw(translation_for(TR_MP_WAR_TITLE), 60, PANEL_Y + 12, FONT_LARGE_BLACK, 0);

    text_draw(translation_for(TR_MP_WAR_HEADER_PLAYER), TEXT_X, HEADER_Y, FONT_SMALL_PLAIN, 0);
    text_draw(translation_for(TR_MP_WAR_HEADER_STATE), STATE_X, HEADER_Y, FONT_SMALL_PLAIN, 0);
    text_draw_centered(translation_for(TR_MP_WAR_HEADER_DECLARE), BRUTAL_X, HEADER_Y,
        HONOURABLE_X + HONOURABLE_WIDTH - BRUTAL_X, FONT_SMALL_PLAIN, 0);
    text_draw_centered(translation_for(TR_MP_WAR_HEADER_PEACE), PEACE_X, HEADER_Y, PEACE_WIDTH, FONT_SMALL_PLAIN, 0);

    inner_panel_draw(32, TABLE_Y, 36, TABLE_HEIGHT_BLOCKS);
    int local = mp_session_local_player_id();
    int rows = 0;
    for (int row = 0; row < MAX_OTHERS; row++) {
        int other = player_of_row(row);
        if (other >= 0) {
            draw_row(row, other, local);
            rows++;
        }
    }
    if (!rows) {
        text_draw(translation_for(TR_MP_WAR_ALONE), TEXT_X, FIRST_ROW_Y + LINE_1_DY, FONT_NORMAL_WHITE, 0);
    }

    draw_rule(TR_MP_WAR_RULE_BRUTAL, RULES_Y);
    draw_rule(TR_MP_WAR_RULE_HONOURABLE, RULES_Y + RULE_SPACING);
    draw_rule(TR_MP_WAR_RULE_PEACE, RULES_Y + 2 * RULE_SPACING);

    const generic_button *ok = &buttons[OK_BUTTON];
    button_border_draw(ok->x, ok->y, ok->width, ok->height, data.focus_button_id == OK_BUTTON + 1);
    text_draw_centered(translation_for(TR_BUTTON_OK), ok->x, ok->y + 6, ok->width, FONT_NORMAL_BLACK, 0);
    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    if (generic_buttons_handle_mouse(mouse_in_dialog(m), 0, 0, buttons, NUM_BUTTONS, &data.focus_button_id)) {
        return;
    }
    if (input_go_back_requested(m, h) || h->enter_pressed) {
        button_ok(0, 0);
    }
}

// the detail of a button, or of the state of a war: the rule, and what Caesar will think of it
static const uint8_t *tooltip_text(int x, int y)
{
    uint8_t *text = data.tooltip;
    int size = sizeof(data.tooltip);
    text[0] = 0;
    int local = mp_session_local_player_id();
    int id = data.focus_button_id;
    if (id > 0 && id <= OK_BUTTON) {
        int row = (id - 1) / ACTIONS_PER_ROW;
        int action = (id - 1) % ACTIONS_PER_ROW;
        int other = player_of_row(row);
        if (other < 0 || !action_available(other, action)) {
            return 0;
        }
        if (action == ACTION_BRUTAL) {
            int notice = mp_war_status(local, other) == MP_WAR_NOTICE;
            append(text, size, translation_for(notice ? TR_MP_WAR_TIP_STRIKE_FIRST : TR_MP_WAR_TIP_BRUTAL));
            append(text, size, (const uint8_t *) " ");
            compose_declaration(text, size, local, other, MP_WAR_BRUTAL, 1);
            append(text, size, (const uint8_t *) ".");
        } else if (action == ACTION_HONOURABLE && mp_war_may_withdraw(local, other)) {
            append(text, size, translation_for(TR_MP_WAR_TIP_WITHDRAW));
        } else if (action == ACTION_HONOURABLE) {
            append(text, size, translation_for(TR_MP_WAR_TIP_HONOURABLE));
            append(text, size, (const uint8_t *) " ");
            compose_declaration(text, size, local, other, MP_WAR_HONOURABLE, 1);
            append(text, size, (const uint8_t *) ".");
        } else if (mp_war_peace_proposed(local, other)) {
            append(text, size, translation_for(TR_MP_WAR_TIP_WITHDRAW_PEACE));
        } else if (mp_war_peace_proposed(other, local)) {
            append(text, size, translation_for(TR_MP_WAR_TIP_ACCEPT));
        } else {
            append(text, size, translation_for(TR_MP_WAR_TIP_PROPOSE));
        }
        return text;
    }
    // the player and his state, on the left of a row during a war: its motive, and what war means
    for (int row = 0; row < MAX_OTHERS; row++) {
        int other = player_of_row(row);
        if (other < 0 || x < ROW_X || x >= BRUTAL_X || y < ROW_Y(row) || y >= ROW_Y(row) + ROW_HEIGHT ||
            mp_war_status(local, other) == MP_WAR_PEACE) {
            continue;
        }
        compose_war_motive(text, size, local, other);
        append(text, size, (const uint8_t *) ". ");
        append(text, size, translation_for(TR_MP_WAR_TIP_AT_WAR));
        return text;
    }
    return 0;
}

static void get_tooltip(tooltip_context *c)
{
    const uint8_t *text = tooltip_text(c->mouse_x - screen_dialog_offset_x(), c->mouse_y - screen_dialog_offset_y());
    if (text) {
        c->type = TOOLTIP_BUTTON;
        c->precomposed_text = text;
    }
}

static void button_war(int row, int action)
{
    int other = player_of_row(row);
    if (other < 0 || !action_available(other, action)) {
        return;
    }
    if (action == ACTION_PEACE) {
        data.armed_player = -1;
        mp_action_propose_peace(other, !mp_war_peace_proposed(mp_session_local_player_id(), other));
    } else {
        int form = action == ACTION_BRUTAL ? MP_WAR_BRUTAL : MP_WAR_HONOURABLE;
        if (data.armed_player == other && data.armed_form == form) {
            data.armed_player = -1; // second click: declared, or withdrawn during the notice
            if (action == ACTION_HONOURABLE && mp_war_may_withdraw(mp_session_local_player_id(), other)) {
                mp_action_withdraw_war(other);
            } else {
                mp_action_declare_war(other, form);
            }
        } else {
            data.armed_player = other;
            data.armed_form = form;
        }
    }
    window_invalidate();
}

static void button_ok(int param1, int param2)
{
    data.armed_player = -1;
    window_advisors_show_advisor(ADVISOR_MILITARY);
}

void window_mp_war_show(void)
{
    window_type window = {
        WINDOW_MP_WAR,
        draw_background,
        draw_foreground,
        handle_input,
        get_tooltip
    };
    data.focus_button_id = 0;
    data.armed_player = -1;
    window_show(&window);
}

// ---------- the proposal of peace received (display only, never saved) ----------

// as the popup dialogs of the original: 480 x 160 in the middle, a title, two lines, two buttons
#define OFFER_X 80
#define OFFER_Y 80
#define OFFER_WIDTH 480
#define OFFER_BUTTON_WIDTH 160
#define OFFER_BUTTON_Y 194

static void button_offer(int accept, int param2);

static generic_button offer_buttons[] = {
    {OFFER_X + 64, OFFER_BUTTON_Y, OFFER_BUTTON_WIDTH, BUTTON_HEIGHT, button_offer, button_none, 1, 0},
    {OFFER_X + OFFER_WIDTH - 64 - OFFER_BUTTON_WIDTH, OFFER_BUTTON_Y, OFFER_BUTTON_WIDTH, BUTTON_HEIGHT,
        button_offer, button_none, 0, 0},
};

static struct {
    int focus_button_id;
    int proposer;
} offer;

static int offer_is_open(void)
{
    int local = mp_session_local_player_id();
    return mp_war_peace_proposed(offer.proposer, local) && !mp_war_peace_proposed(local, offer.proposer);
}

static void draw_offer_background(void)
{
    window_draw_underlying_window();
    graphics_in_dialog();
    outer_panel_draw(OFFER_X, OFFER_Y, OFFER_WIDTH / 16, 10);
    text_draw_centered(translation_for(TR_MP_PEACE_OFFER_TITLE), OFFER_X, OFFER_Y + 20, OFFER_WIDTH,
        FONT_LARGE_BLACK, 0);
    uint8_t text[100];
    mp_war_compose(text, sizeof(text), translation_for(TR_MP_PEACE_OFFER_TEXT), offer.proposer, -1);
    text_draw_centered(text, OFFER_X, OFFER_Y + 60, OFFER_WIDTH, FONT_NORMAL_BLACK, 0);
    text_draw_centered(translation_for(TR_MP_PEACE_OFFER_DETAIL), OFFER_X, OFFER_Y + 80, OFFER_WIDTH,
        FONT_NORMAL_BLACK, 0);
    graphics_reset_dialog();
}

static void draw_offer_foreground(void)
{
    graphics_in_dialog();
    for (int i = 0; i < 2; i++) {
        const generic_button *b = &offer_buttons[i];
        button_border_draw(b->x, b->y, b->width, b->height, offer.focus_button_id == i + 1);
        text_draw_centered(translation_for(b->parameter1 ? TR_MP_PEACE_OFFER_ACCEPT : TR_MP_PEACE_OFFER_LATER),
            b->x, b->y + 6, b->width, FONT_NORMAL_BLACK, 0);
    }
    graphics_reset_dialog();
}

static void handle_offer_input(const mouse *m, const hotkeys *h)
{
    if (!offer_is_open()) {
        window_city_show(); // withdrawn, or signed meanwhile
        return;
    }
    if (generic_buttons_handle_mouse(mouse_in_dialog(m), 0, 0, offer_buttons, 2, &offer.focus_button_id)) {
        return;
    }
    if (input_go_back_requested(m, h)) {
        button_offer(0, 0);
    } else if (h->enter_pressed) {
        button_offer(1, 0);
    }
}

// "Accept" signs the peace as the button of the window "War and peace" does; "Later" leaves the proposal there
static void button_offer(int accept, int param2)
{
    if (accept && offer_is_open()) {
        mp_action_propose_peace(offer.proposer, 1);
    }
    window_city_show();
}

void window_mp_war_show_peace_offer_pending(void)
{
    if (!window_mp_war_is_available() || !window_is(WINDOW_CITY) || building_construction_in_progress()) {
        return;
    }
    int proposer = mp_war_take_peace_offer();
    if (proposer < 0) {
        return;
    }
    window_type window = {
        WINDOW_MP_WAR,
        draw_offer_background,
        draw_offer_foreground,
        handle_offer_input
    };
    offer.focus_button_id = 0;
    offer.proposer = proposer;
    window_show(&window);
}
