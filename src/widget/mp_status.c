#include "mp_status.h"

#include "core/encoding.h"
#include "core/string.h"
#include "graphics/graphics.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "game/player_context.h"
#include "game/rules.h"
#include "mp/caesar.h"
#include "mp/colors.h"
#include "mp/fog.h"
#include "mp/endgame.h"
#include "mp/lockstep.h"
#include "mp/missionary.h"
#include "mp/session.h"
#include "mp/territory.h"
#include "translation/translation.h"

#include <stdio.h>

#define BANNER_HEIGHT 22
#define BANNER_Y 28

// a player without any mission nor land is told how to found his city again (D-037, D-045)
static void draw_objective(int banner_y)
{
    static const int LINES[] = { TR_MP_OBJECTIVE_MISSION_1, TR_MP_OBJECTIVE_MISSION_2 };
    int num_lines = sizeof(LINES) / sizeof(LINES[0]);
    int width = 0;
    for (int i = 0; i < num_lines; i++) {
        int line_width = text_get_width(translation_for(LINES[i]), FONT_NORMAL_PLAIN);
        width = line_width > width ? line_width : width;
    }
    int height = 16 * num_lines + 8;
    int y = banner_y - height - 4;
    graphics_fill_rect(4, y, width + 16, height, COLOR_BLACK);
    for (int i = 0; i < num_lines; i++) {
        text_draw(translation_for(LINES[i]), 12, y + 6 + 16 * i, FONT_NORMAL_PLAIN, COLOR_FONT_YELLOW);
    }
}

void widget_mp_status_draw(void)
{
    mp_lockstep_state state = mp_lockstep_get_state();
    if (state == MP_LOCKSTEP_OFF) {
        return;
    }
    // the lobby shows the state of the network itself, under its rules: no banner over its title on a small screen
    if (state != MP_LOCKSTEP_RUNNING && window_is(WINDOW_MP_LOBBY)) {
        return;
    }
    // on a small screen, the windows over the city (advisors, build menus) reach the bottom: no banner over them
    if (state == MP_LOCKSTEP_RUNNING && screen_height() < 600 && !window_is(WINDOW_CITY) &&
        !window_is(WINDOW_CITY_MILITARY)) {
        return;
    }
    color_t color = COLOR_FONT_YELLOW;
    uint8_t encoded[200] = { 0 };
    if (state == MP_LOCKSTEP_RUNNING) {
        // small reminder of who we are
        string_copy(translation_for(TR_MP_BANNER_PLAYER), encoded, 100);
        string_from_int(encoded + string_length(encoded), mp_session_local_player_id() + 1, 0);
        color = COLOR_WHITE;
    } else {
        encoding_from_utf8(mp_lockstep_status(), encoded, sizeof(encoded));
        if (state != MP_LOCKSTEP_WAITING_FOR_PLAYERS) {
            color = COLOR_FONT_RED;
        }
    }
    int width = text_get_width(encoded, FONT_NORMAL_PLAIN) + 16;
    // the laurels of the players, each in its color, public (D-053); the provisional score of a game ended by years
    // is hidden by the fog of war (D-038); then the score of Caesar's heir, and the pause
    int num_players = player_context_num_players();
    int scores_width = 0;
    uint8_t scores[PLAYER_CONTEXT_MAX_PLAYERS][32];
    int by_years = game_rules_end_condition() == GAME_END_SCORE;
    if (state == MP_LOCKSTEP_RUNNING && (num_players > 1 || mp_caesar_is_active())) {
        for (int p = 0; p < num_players; p++) {
            if (by_years && mp_fog_is_active() && p != mp_session_local_player_id()) {
                scores[p][0] = 0;
                continue;
            }
            char score[32];
            snprintf(score, sizeof(score), "J%d %d", p + 1, by_years ? mp_endgame_live_score(p) :
                mp_caesar_laurels(p) / 10);
            encoding_from_utf8(score, scores[p], sizeof(scores[p]));
            scores_width += text_get_width(scores[p], FONT_NORMAL_PLAIN) + 12;
        }
    }
    uint8_t target[64] = { 0 };
    if (state == MP_LOCKSTEP_RUNNING && game_rules_end_condition() == GAME_END_CAESAR) {
        string_copy(translation_for(TR_MP_BANNER_TARGET), target, 40);
        string_from_int(target + string_length(target), game_rules_caesar_score(), 0);
        string_copy(translation_for(TR_MP_LAURELS), target + string_length(target), 60 - string_length(target));
        scores_width += text_get_width(target, FONT_NORMAL_PLAIN) + 12;
    }
    uint8_t paused[32];
    int paused_width = 0;
    if (state == MP_LOCKSTEP_RUNNING && mp_lockstep_is_paused()) {
        encoding_from_utf8("PAUSE", paused, sizeof(paused));
        paused_width = text_get_width(paused, FONT_NORMAL_PLAIN) + 12;
    }
    int x = state == MP_LOCKSTEP_RUNNING ? 4 : (screen_width() - width) / 2;
    // during the game, at the bottom of the view: the warnings of the game use the top
    int banner_y = state == MP_LOCKSTEP_RUNNING ? screen_height() - BANNER_HEIGHT - 4 : BANNER_Y;
    graphics_fill_rect(x, banner_y, width + scores_width + paused_width, BANNER_HEIGHT, COLOR_BLACK);
    text_draw(encoded, x + 8, banner_y + 6, FONT_NORMAL_PLAIN, color);
    int x_score = x + width;
    if (scores_width) {
        for (int p = 0; p < num_players; p++) {
            if (scores[p][0]) {
                x_score += text_draw(scores[p], x_score, banner_y + 6, FONT_NORMAL_PLAIN, mp_colors_player(p)) + 12;
            }
        }
        if (target[0]) {
            x_score += text_draw(target, x_score, banner_y + 6, FONT_NORMAL_PLAIN, COLOR_FONT_LIGHT_GRAY) + 12;
        }
    }
    if (paused_width) {
        text_draw(paused, x_score, banner_y + 6, FONT_NORMAL_PLAIN, COLOR_FONT_YELLOW);
    }
    if (state == MP_LOCKSTEP_RUNNING && mp_territory_is_active() && !mp_mission_exists() &&
        !mp_territory_owns_land()) {
        draw_objective(banner_y);
    }
}
