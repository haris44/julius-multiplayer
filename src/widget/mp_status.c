#include "mp_status.h"

#include "core/encoding.h"
#include "graphics/graphics.h"
#include "graphics/screen.h"
#include "graphics/text.h"
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
    int x = state == MP_LOCKSTEP_RUNNING ? 4 : (screen_width() - width) / 2;
    graphics_fill_rect(x, BANNER_Y, width, BANNER_HEIGHT, COLOR_BLACK);
    text_draw(encoded, x + 8, BANNER_Y + 6, FONT_NORMAL_PLAIN, color);
}
