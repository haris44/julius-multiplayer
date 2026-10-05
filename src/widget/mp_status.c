#include "mp_status.h"

#include "core/encoding.h"
#include "graphics/graphics.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "game/player_context.h"
#include "mp/colors.h"
#include "mp/fog.h"
#include "mp/endgame.h"
#include "mp/lockstep.h"
#include "mp/session.h"

#include <stdio.h>

#define BANNER_HEIGHT 22
#define BANNER_Y 28

void widget_mp_status_draw(void)
{
    mp_lockstep_state state = mp_lockstep_get_state();
    if (state == MP_LOCKSTEP_OFF) {
        return;
    }
    char text[200];
    color_t color = COLOR_FONT_YELLOW;
    if (state == MP_LOCKSTEP_RUNNING) {
        // small reminder of who we are
        snprintf(text, sizeof(text), "Multijoueur - joueur %d", mp_session_local_player_id() + 1);
        color = COLOR_WHITE;
    } else {
        snprintf(text, sizeof(text), "%s", mp_lockstep_status());
        if (state != MP_LOCKSTEP_WAITING_FOR_PLAYERS) {
            color = COLOR_FONT_RED;
        }
    }
    uint8_t encoded[200];
    encoding_from_utf8(text, encoded, sizeof(encoded));
    int width = text_get_width(encoded, FONT_NORMAL_PLAIN) + 16;
    // the scores of the players, each in its color, and the pause
    int num_players = player_context_num_players();
    int scores_width = 0;
    uint8_t scores[PLAYER_CONTEXT_MAX_PLAYERS][32];
    if (state == MP_LOCKSTEP_RUNNING && num_players > 1) {
        for (int p = 0; p < num_players; p++) {
            if (mp_fog_is_active() && p != mp_session_local_player_id()) {
                scores[p][0] = 0; // the fog of war hides the other cities (D-038)
                continue;
            }
            char score[32];
            snprintf(score, sizeof(score), "J%d %d", p + 1, mp_endgame_live_score(p));
            encoding_from_utf8(score, scores[p], sizeof(scores[p]));
            scores_width += text_get_width(scores[p], FONT_NORMAL_PLAIN) + 12;
        }
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
    }
    if (paused_width) {
        text_draw(paused, x_score, banner_y + 6, FONT_NORMAL_PLAIN, COLOR_FONT_YELLOW);
    }
}
