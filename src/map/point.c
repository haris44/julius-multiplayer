#include "map/point.h"
#include "game/player_context.h"
#include "game/player_clone.h"

static map_point last = {0, 0};

void map_point_store_result(int x, int y, map_point *point)
{
    point->x = last.x = x;
    point->y = last.y = y;
}

void map_point_get_last_result(map_point *point)
{
    point->x = last.x;
    point->y = last.y;
}

void map_point_reset_last_result(void)
{
    last.x = 0;
    last.y = 0;
}

void map_point_save_state(buffer *buf)
{
    buffer_write_i32(buf, last.x);
    buffer_write_i32(buf, last.y);
}

void map_point_load_state(buffer *buf)
{
    last.x = buffer_read_i32(buf);
    last.y = buffer_read_i32(buf);
}

void map_point_register_player_state(void)
{
    player_context_register(&last, sizeof(last), "map_point_last");
}

void map_point_clone_fixup(const player_clone *c)
{
    last.x += c->dx;
    last.y += c->dy;
}
