#include "extra_state.h"

#include "building/count.h"
#include "building/list.h"
#include "building/maintenance.h"
#include "city/labor.h"
#include "core/random.h"
#include "map/image_context.h"
#include "map/point.h"
#include "map/soldier_strength.h"

#include <stdlib.h>

void game_extra_state_reset(void)
{
    random_reset();
    building_count_reset_extra_state();
    building_maintenance_reset_extra_state();
    map_image_context_init();
    map_soldier_strength_clear();
    map_point_reset_last_result();
    building_list_reset_extra_state();
    city_labor_reset_extra_state();
}

void game_extra_state_save(buffer *buf)
{
    random_save_extra_state(buf);
    building_count_save_extra_state(buf);
    building_maintenance_save_extra_state(buf);
    map_image_context_save_state(buf);
    map_soldier_strength_save_state(buf);
    map_point_save_state(buf);
    building_list_save_extra_state(buf);
    city_labor_save_extra_state(buf);
}

void game_extra_state_load(buffer *buf)
{
    random_load_extra_state(buf);
    building_count_load_extra_state(buf);
    building_maintenance_load_extra_state(buf);
    map_image_context_load_state(buf);
    map_soldier_strength_load_state(buf);
    map_point_load_state(buf);
    building_list_load_extra_state(buf);
    city_labor_load_extra_state(buf);
}

int game_extra_state_size(void)
{
    static int size = 0;
    if (!size) {
        // Measure by saving into a buffer large enough for every part
        int capacity = 64 * 1024;
        void *data = malloc(capacity);
        if (!data) {
            return 0;
        }
        buffer buf;
        buffer_init(&buf, data, capacity);
        game_extra_state_save(&buf);
        size = buf.index;
        free(data);
    }
    return size;
}
