#include "colors.h"

#include "building/building.h"
#include "figure/figure.h"
#include "game/player_context.h"
#include "map/building.h"
#include "map/owner.h"
#include "mp/session.h"

static const color_t PLAYER_COLORS[PLAYER_CONTEXT_MAX_PLAYERS] = {
    0xff3060e0, 0xffd03030, 0xff30a030, 0xffe0c020
};
// light tints (D-039): multiplied with the images, they keep them readable; the borders of the zones show the rest
static const color_t PLAYER_TINTS[PLAYER_CONTEXT_MAX_PLAYERS] = {
    0xffd0dcff, 0xffffd0d0, 0xffd4f6d4, 0xfffff6cc
};

color_t mp_colors_player(int player_id)
{
    return player_id >= 0 && player_id < PLAYER_CONTEXT_MAX_PLAYERS ? PLAYER_COLORS[player_id] : COLOR_BLACK;
}

color_t mp_colors_tint_for_player(int player_id)
{
    if (player_context_num_players() <= 1 || player_id < 0 || player_id >= PLAYER_CONTEXT_MAX_PLAYERS ||
        player_id == mp_session_local_player_id()) {
        return 0;
    }
    return PLAYER_TINTS[player_id];
}

color_t mp_colors_tint_for_building(int building_id)
{
    return building_id > 0 ? mp_colors_tint_for_player(BUILDING_OWNER(building_id)) : 0;
}

color_t mp_colors_tint_for_tile(int grid_offset)
{
    int building_id = map_building_at(grid_offset);
    if (building_id) {
        return mp_colors_tint_for_building(building_id);
    }
    int owner = map_owner_get_claimed(grid_offset);
    return owner == MAP_OWNER_NONE ? 0 : mp_colors_tint_for_player(owner);
}

color_t mp_colors_tint_for_figure(int figure_id)
{
    return figure_id > 0 ? mp_colors_tint_for_player(FIGURE_OWNER(figure_id)) : 0;
}
