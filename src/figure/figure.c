#include "figure/figure.h"

#include "building/building.h"
#include "city/emperor.h"
#include "core/random.h"
#include "empire/city.h"
#include "figure/name.h"
#include "figure/route.h"
#include "figure/trader.h"
#include "map/figure.h"
#include "map/grid.h"
#include "figure/formation.h"
#include "game/player_clone.h"
#include "game/save_format.h"

#include <string.h>

static struct {
    int created_sequence;
    figure figures[FIGURE_ARRAY_SIZE];
} data = {0};

figure *figure_get(int id)
{
    return &data.figures[id];
}

figure *figure_create(figure_type type, int x, int y, direction_type dir)
{
    int id = 0;
    for (int i = FIGURE_FIRST; i < FIGURE_END; i++) {
        if (!data.figures[i].state) {
            id = i;
            break;
        }
    }
    if (!id) {
        return &data.figures[0];
    }
    figure *f = &data.figures[id];
    f->state = FIGURE_STATE_ALIVE;
    f->faction_id = 1;
    f->type = type;
    f->use_cross_country = 0;
    f->is_friendly = 1;
    f->created_sequence = data.created_sequence++;
    f->direction = dir;
    f->source_x = f->destination_x = f->previous_tile_x = f->x = GRID_COORD(x);
    f->source_y = f->destination_y = f->previous_tile_y = f->y = GRID_COORD(y);
    f->grid_offset = map_grid_offset(x, y);
    f->cross_country_x = 15 * x;
    f->cross_country_y = 15 * y;
    f->progress_on_tile = 15;
    f->phrase_sequence_city = f->phrase_sequence_exact = random_byte() & 3;
    f->name = figure_name_get(type, 0);
    map_figure_add(f);
    if (type == FIGURE_TRADE_CARAVAN || type == FIGURE_TRADE_SHIP) {
        f->trader_id = trader_create();
    }
    return f;
}

void figure_delete(figure *f)
{
    building *b = building_get(f->building_id);
    switch (f->type) {
        case FIGURE_LABOR_SEEKER:
        case FIGURE_MARKET_BUYER:
            if (f->building_id) {
                b->figure_id2 = 0;
            }
            break;
        case FIGURE_BALLISTA:
            b->figure_id4 = 0;
            break;
        case FIGURE_DOCKER:
            for (int i = 0; i < 3; i++) {
                if (b->data.dock.docker_ids[i] == f->id) {
                    b->data.dock.docker_ids[i] = 0;
                }
            }
            break;
        case FIGURE_ENEMY_CAESAR_LEGIONARY:
            city_emperor_mark_soldier_killed();
            break;
        case FIGURE_EXPLOSION:
        case FIGURE_FORT_STANDARD:
        case FIGURE_ARROW:
        case FIGURE_JAVELIN:
        case FIGURE_BOLT:
        case FIGURE_SPEAR:
        case FIGURE_FISH_GULLS:
        case FIGURE_SHEEP:
        case FIGURE_WOLF:
        case FIGURE_ZEBRA:
        case FIGURE_DELIVERY_BOY:
        case FIGURE_PATRICIAN:
            // nothing to do here
            break;
        default:
            if (f->building_id) {
                b->figure_id = 0;
            }
            break;
    }
    if (f->empire_city_id) {
        empire_city_remove_trader(f->empire_city_id, f->id);
    }
    if (f->immigrant_building_id) {
        b->immigrant_figure_id = 0;
    }
    figure_route_remove(f);
    map_figure_delete(f);

    int figure_id = f->id;
    memset(f, 0, sizeof(figure));
    f->id = figure_id;
}

int figure_is_dead(const figure *f)
{
    return f->state != FIGURE_STATE_ALIVE || f->action_state == FIGURE_ACTION_149_CORPSE;
}

int figure_is_enemy(const figure *f)
{
    return f->type >= FIGURE_ENEMY43_SPEAR && f->type <= FIGURE_ENEMY_CAESAR_LEGIONARY;
}

int figure_is_legion(const figure *f)
{
    return f->type >= FIGURE_FORT_JAVELIN && f->type <= FIGURE_FORT_LEGIONARY;
}

int figure_is_herd(const figure *f)
{
    return f->type >= FIGURE_SHEEP && f->type <= FIGURE_ZEBRA;
}

void figure_init_scenario(void)
{
    for (int i = 0; i < FIGURE_ARRAY_SIZE; i++) {
        memset(&data.figures[i], 0, sizeof(figure));
        data.figures[i].id = i;
    }
    data.created_sequence = 0;
}

static void figure_save(buffer *buf, const figure *f)
{
    buffer_write_u8(buf, f->alternative_location_index);
    buffer_write_u8(buf, f->image_offset);
    buffer_write_u8(buf, f->is_enemy_image);
    buffer_write_u8(buf, f->flotsam_visible);
    buffer_write_i16(buf, f->image_id);
    buffer_write_i16(buf, f->cart_image_id);
    buffer_write_i16(buf, f->next_figure_id_on_same_tile);
    buffer_write_u8(buf, f->type);
    buffer_write_u8(buf, f->resource_id);
    buffer_write_u8(buf, f->use_cross_country);
    buffer_write_u8(buf, f->is_friendly);
    buffer_write_u8(buf, f->state);
    buffer_write_u8(buf, f->faction_id);
    buffer_write_u8(buf, f->action_state_before_attack);
    buffer_write_i8(buf, f->direction);
    buffer_write_i8(buf, f->previous_tile_direction);
    buffer_write_i8(buf, f->attack_direction);
    save_write_coord(buf, f->x);
    save_write_coord(buf, f->y);
    save_write_coord(buf, f->previous_tile_x);
    save_write_coord(buf, f->previous_tile_y);
    buffer_write_u8(buf, f->missile_damage);
    buffer_write_u8(buf, f->damage);
    save_write_offset(buf, f->grid_offset);
    save_write_coord(buf, f->destination_x);
    save_write_coord(buf, f->destination_y);
    save_write_offset(buf, f->destination_grid_offset);
    save_write_coord(buf, f->source_x);
    save_write_coord(buf, f->source_y);
    if (save_format_wide) {
        buffer_write_i16(buf, f->formation_position_x.enemy); // same bits as the soldier position
        buffer_write_i16(buf, f->formation_position_y.enemy);
    } else {
        buffer_write_u8(buf, (uint8_t) f->formation_position_x.soldier);
        buffer_write_u8(buf, (uint8_t) f->formation_position_y.soldier);
    }
    buffer_write_i16(buf, f->__unused_24);
    buffer_write_i16(buf, f->wait_ticks);
    buffer_write_u8(buf, f->action_state);
    buffer_write_u8(buf, f->progress_on_tile);
    buffer_write_i16(buf, f->routing_path_id);
    buffer_write_i16(buf, f->routing_path_current_tile);
    buffer_write_i16(buf, f->routing_path_length);
    buffer_write_u8(buf, f->in_building_wait_ticks);
    buffer_write_u8(buf, f->is_on_road);
    buffer_write_i16(buf, f->max_roam_length);
    buffer_write_i16(buf, f->roam_length);
    buffer_write_u8(buf, f->roam_choose_destination);
    buffer_write_u8(buf, f->roam_random_counter);
    buffer_write_i8(buf, f->roam_turn_direction);
    buffer_write_i8(buf, f->roam_ticks_until_next_turn);
    buffer_write_i16(buf, f->cross_country_x);
    buffer_write_i16(buf, f->cross_country_y);
    buffer_write_i16(buf, f->cc_destination_x);
    buffer_write_i16(buf, f->cc_destination_y);
    buffer_write_i16(buf, f->cc_delta_x);
    buffer_write_i16(buf, f->cc_delta_y);
    buffer_write_i16(buf, f->cc_delta_xy);
    buffer_write_u8(buf, f->cc_direction);
    buffer_write_u8(buf, f->speed_multiplier);
    buffer_write_i16(buf, f->building_id);
    buffer_write_i16(buf, f->immigrant_building_id);
    buffer_write_i16(buf, f->destination_building_id);
    buffer_write_i16(buf, f->formation_id);
    buffer_write_u8(buf, f->index_in_formation);
    buffer_write_u8(buf, f->formation_at_rest);
    buffer_write_u8(buf, f->migrant_num_people);
    buffer_write_u8(buf, f->is_ghost);
    buffer_write_u8(buf, f->min_max_seen);
    buffer_write_u8(buf, f->__unused_57);
    buffer_write_i16(buf, f->leading_figure_id);
    buffer_write_u8(buf, f->attack_image_offset);
    buffer_write_u8(buf, f->wait_ticks_missile);
    buffer_write_i8(buf, f->x_offset_cart);
    buffer_write_i8(buf, f->y_offset_cart);
    buffer_write_u8(buf, f->empire_city_id);
    buffer_write_u8(buf, f->trader_amount_bought);
    buffer_write_i16(buf, f->name);
    buffer_write_u8(buf, f->terrain_usage);
    buffer_write_u8(buf, f->loads_sold_or_carrying);
    buffer_write_u8(buf, f->is_boat);
    buffer_write_u8(buf, f->height_adjusted_ticks);
    buffer_write_u8(buf, f->current_height);
    buffer_write_u8(buf, f->target_height);
    buffer_write_u8(buf, f->collecting_item_id);
    buffer_write_u8(buf, f->trade_ship_failed_dock_attempts);
    buffer_write_u8(buf, f->phrase_sequence_exact);
    buffer_write_i8(buf, f->phrase_id);
    buffer_write_u8(buf, f->phrase_sequence_city);
    buffer_write_u8(buf, f->trader_id);
    buffer_write_u8(buf, f->wait_ticks_next_target);
    buffer_write_u8(buf, f->__unused_6f);
    buffer_write_i16(buf, f->target_figure_id);
    buffer_write_i16(buf, f->targeted_by_figure_id);
    buffer_write_u16(buf, f->created_sequence);
    buffer_write_u16(buf, f->target_figure_created_sequence);
    buffer_write_u8(buf, f->figures_on_same_tile_index);
    buffer_write_u8(buf, f->num_attackers);
    buffer_write_i16(buf, f->attacker_id1);
    buffer_write_i16(buf, f->attacker_id2);
    buffer_write_i16(buf, f->opponent_id);
}

static void figure_load(buffer *buf, figure *f)
{
    f->alternative_location_index = buffer_read_u8(buf);
    f->image_offset = buffer_read_u8(buf);
    f->is_enemy_image = buffer_read_u8(buf);
    f->flotsam_visible = buffer_read_u8(buf);
    f->image_id = buffer_read_i16(buf);
    f->cart_image_id = buffer_read_i16(buf);
    f->next_figure_id_on_same_tile = buffer_read_i16(buf);
    f->type = buffer_read_u8(buf);
    f->resource_id = buffer_read_u8(buf);
    f->use_cross_country = buffer_read_u8(buf);
    f->is_friendly = buffer_read_u8(buf);
    f->state = buffer_read_u8(buf);
    f->faction_id = buffer_read_u8(buf);
    f->action_state_before_attack = buffer_read_u8(buf);
    f->direction = buffer_read_i8(buf);
    f->previous_tile_direction = buffer_read_i8(buf);
    f->attack_direction = buffer_read_i8(buf);
    f->x = GRID_COORD(save_read_coord(buf));
    f->y = GRID_COORD(save_read_coord(buf));
    f->previous_tile_x = GRID_COORD(save_read_coord(buf));
    f->previous_tile_y = GRID_COORD(save_read_coord(buf));
    f->missile_damage = buffer_read_u8(buf);
    f->damage = buffer_read_u8(buf);
    f->grid_offset = save_read_offset(buf);
    f->destination_x = GRID_COORD(save_read_coord(buf));
    f->destination_y = GRID_COORD(save_read_coord(buf));
    f->destination_grid_offset = save_read_offset(buf);
    f->source_x = GRID_COORD(save_read_coord(buf));
    f->source_y = GRID_COORD(save_read_coord(buf));
    // one byte in classic saved games: signed for enemies, unsigned for soldiers
    if (save_format_wide) {
        f->formation_position_x.enemy = buffer_read_i16(buf);
        f->formation_position_y.enemy = buffer_read_i16(buf);
    } else if (figure_is_enemy(f)) {
        f->formation_position_x.enemy = buffer_read_i8(buf);
        f->formation_position_y.enemy = buffer_read_i8(buf);
    } else {
        f->formation_position_x.soldier = buffer_read_u8(buf);
        f->formation_position_y.soldier = buffer_read_u8(buf);
    }
    f->__unused_24 = buffer_read_i16(buf);
    f->wait_ticks = buffer_read_i16(buf);
    f->action_state = buffer_read_u8(buf);
    f->progress_on_tile = buffer_read_u8(buf);
    f->routing_path_id = buffer_read_i16(buf);
    f->routing_path_current_tile = buffer_read_i16(buf);
    f->routing_path_length = buffer_read_i16(buf);
    f->in_building_wait_ticks = buffer_read_u8(buf);
    f->is_on_road = buffer_read_u8(buf);
    f->max_roam_length = buffer_read_i16(buf);
    f->roam_length = buffer_read_i16(buf);
    f->roam_choose_destination = buffer_read_u8(buf);
    f->roam_random_counter = buffer_read_u8(buf);
    f->roam_turn_direction = buffer_read_i8(buf);
    f->roam_ticks_until_next_turn = buffer_read_i8(buf);
    f->cross_country_x = buffer_read_i16(buf);
    f->cross_country_y = buffer_read_i16(buf);
    f->cc_destination_x = buffer_read_i16(buf);
    f->cc_destination_y = buffer_read_i16(buf);
    f->cc_delta_x = buffer_read_i16(buf);
    f->cc_delta_y = buffer_read_i16(buf);
    f->cc_delta_xy = buffer_read_i16(buf);
    f->cc_direction = buffer_read_u8(buf);
    f->speed_multiplier = buffer_read_u8(buf);
    f->building_id = buffer_read_i16(buf);
    f->immigrant_building_id = buffer_read_i16(buf);
    f->destination_building_id = buffer_read_i16(buf);
    f->formation_id = buffer_read_i16(buf);
    f->index_in_formation = buffer_read_u8(buf);
    f->formation_at_rest = buffer_read_u8(buf);
    f->migrant_num_people = buffer_read_u8(buf);
    f->is_ghost = buffer_read_u8(buf);
    f->min_max_seen = buffer_read_u8(buf);
    f->__unused_57 = buffer_read_u8(buf);
    f->leading_figure_id = buffer_read_i16(buf);
    f->attack_image_offset = buffer_read_u8(buf);
    f->wait_ticks_missile = buffer_read_u8(buf);
    f->x_offset_cart = buffer_read_i8(buf);
    f->y_offset_cart = buffer_read_i8(buf);
    f->empire_city_id = buffer_read_u8(buf);
    f->trader_amount_bought = buffer_read_u8(buf);
    f->name = buffer_read_i16(buf);
    f->terrain_usage = buffer_read_u8(buf);
    f->loads_sold_or_carrying = buffer_read_u8(buf);
    f->is_boat = buffer_read_u8(buf);
    f->height_adjusted_ticks = buffer_read_u8(buf);
    f->current_height = buffer_read_u8(buf);
    f->target_height = buffer_read_u8(buf);
    f->collecting_item_id = buffer_read_u8(buf);
    f->trade_ship_failed_dock_attempts = buffer_read_u8(buf);
    f->phrase_sequence_exact = buffer_read_u8(buf);
    f->phrase_id = buffer_read_i8(buf);
    f->phrase_sequence_city = buffer_read_u8(buf);
    f->trader_id = buffer_read_u8(buf);
    f->wait_ticks_next_target = buffer_read_u8(buf);
    f->__unused_6f = buffer_read_u8(buf);
    f->target_figure_id = buffer_read_i16(buf);
    f->targeted_by_figure_id = buffer_read_i16(buf);
    f->created_sequence = buffer_read_u16(buf);
    f->target_figure_created_sequence = buffer_read_u16(buf);
    f->figures_on_same_tile_index = buffer_read_u8(buf);
    f->num_attackers = buffer_read_u8(buf);
    f->attacker_id1 = buffer_read_i16(buf);
    f->attacker_id2 = buffer_read_i16(buf);
    f->opponent_id = buffer_read_i16(buf);
}

void figure_save_state(buffer *list, buffer *seq)
{
    buffer_write_i32(seq, data.created_sequence);

    // the slice of the current player: the whole array in a classic game
    int base = FIGURE_FIRST - 1;
    for (int i = 0; i < MAX_FIGURES; i++) {
        figure_save(list, &data.figures[base + i]);
    }
}

void figure_load_state(buffer *list, buffer *seq)
{
    data.created_sequence = buffer_read_i32(seq);

    int base = FIGURE_FIRST - 1;
    for (int i = 0; i < MAX_FIGURES; i++) {
        figure_load(list, &data.figures[base + i]);
        data.figures[base + i].id = base + i;
    }
}

void figure_register_player_state(void)
{
    player_context_register(&data.created_sequence, sizeof(data.created_sequence), "figure_sequence");
}

void figure_relocate_grid_offsets(int (*remap)(int grid_offset))
{
    for (int i = 0; i < FIGURE_ARRAY_SIZE; i++) {
        data.figures[i].grid_offset = remap(data.figures[i].grid_offset);
        data.figures[i].destination_grid_offset = remap(data.figures[i].destination_grid_offset);
    }
}

void figure_clone_record(figure *f, const figure *source, int new_id, const player_clone *c)
{
#define B(id) player_clone_id(c, id, MAX_BUILDINGS)
#define F(id) player_clone_id(c, id, MAX_FIGURES)
    *f = *source;
    f->id = new_id;
    if (!f->state) {
        return;
    }
    f->x += c->dx;
    f->y += c->dy;
    f->previous_tile_x += c->dx;
    f->previous_tile_y += c->dy;
    f->destination_x += c->dx;
    f->destination_y += c->dy;
    f->source_x += c->dx;
    f->source_y += c->dy;
    f->grid_offset += c->grid_delta;
    if (f->destination_grid_offset) {
        f->destination_grid_offset += c->grid_delta;
    }
    f->cross_country_x += 15 * c->dx;
    f->cross_country_y += 15 * c->dy;
    if (f->cc_destination_x || f->cc_destination_y) { // 0: no cross-country movement yet
        f->cc_destination_x += 15 * c->dx;
        f->cc_destination_y += 15 * c->dy;
    }
    if (figure_is_legion(f)) {
        f->formation_position_x.soldier += c->dx; // map coordinates for soldiers, offsets for enemies
        f->formation_position_y.soldier += c->dy;
    }
    f->next_figure_id_on_same_tile = F(f->next_figure_id_on_same_tile);
    f->routing_path_id = player_clone_id(c, f->routing_path_id, 600);
    if (f->type == FIGURE_ARROW || f->type == FIGURE_SPEAR || f->type == FIGURE_JAVELIN || f->type == FIGURE_BOLT) {
        f->building_id = F(f->building_id); // missiles keep the id of the figure that shot them
    } else {
        f->building_id = B(f->building_id);
    }
    f->immigrant_building_id = B(f->immigrant_building_id);
    f->destination_building_id = B(f->destination_building_id);
    f->formation_id = player_clone_id(c, f->formation_id, MAX_FORMATIONS);
    if (f->type != FIGURE_HIPPODROME_HORSES) { // horses count their laps in this field
        f->leading_figure_id = F(f->leading_figure_id);
    }
    f->target_figure_id = F(f->target_figure_id);
    f->targeted_by_figure_id = F(f->targeted_by_figure_id);
    f->opponent_id = F(f->opponent_id);
    f->attacker_id1 = F(f->attacker_id1);
    f->attacker_id2 = F(f->attacker_id2);
#undef B
#undef F
}

void figure_clone_player(const player_clone *c)
{
    for (int i = 0; i < MAX_FIGURES; i++) {
        figure source = data.figures[c->from * MAX_FIGURES + i];
        figure_clone_record(&data.figures[c->to * MAX_FIGURES + i], &source, c->to * MAX_FIGURES + i, c);
    }
}

void figure_save_record(buffer *buf, const figure *f)
{
    figure_save(buf, f);
}
