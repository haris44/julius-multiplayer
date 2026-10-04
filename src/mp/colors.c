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
// light tints: multiplied with the images, they keep them readable
static const color_t PLAYER_TINTS[PLAYER_CONTEXT_MAX_PLAYERS] = {
    0xffa8c0ff, 0xffffa8a8, 0xffb0f0b0, 0xfffff0a0
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
