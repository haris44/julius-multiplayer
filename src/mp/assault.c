#include "assault.h"

#include "building/building.h"
#include "core/calc.h"
#include "figure/figure.h"
#include "figure/formation.h"
#include "figure/formation_legion.h"
#include "game/player_context.h"
#include "game/rules.h"
#include "map/building.h"
#include "map/grid.h"
#include "map/owner.h"
#include "map/routing.h"
#include "mp/territory.h"
#include "mp/war.h"

#include <string.h>

#define STATE_VERSION 1
#define MAX_ORDERS FORMATION_ARRAY_SIZE
#define MAX_PLAYERS PLAYER_CONTEXT_MAX_PLAYERS

// Ticks a legion may stand by the building it goes to with nothing of the enemy to pull down around its soldiers
// before it gives up on that building: none of them can stand next to it from there (two days)
#define MAX_STALL_TICKS 100

// The order of each legion, by formation id (legions of every player)
typedef struct {
    uint8_t active;
    int8_t enemy;          // the player attacked
    int32_t x;             // the target picked: the centre of the radius
    int32_t y;
    int32_t building_id;   // the building the legion goes to, or whose fall it waits for
    int32_t stall_ticks;   // ticks standing by it with nothing to pull down
} assault_order;

static assault_order orders[MAX_ORDERS];

static int is_order_id(int formation_id)
{
    return formation_id > 0 && formation_id < MAX_ORDERS;
}

static void clear_order(assault_order *o)
{
    memset(o, 0, sizeof(assault_order));
    o->enemy = -1;
}

void mp_assault_reset(void)
{
    for (int i = 0; i < MAX_ORDERS; i++) {
        clear_order(&orders[i]);
    }
}

int mp_assault_is_active(int formation_id)
{
    return is_order_id(formation_id) && orders[formation_id].active;
}

int mp_assault_enemy(int formation_id)
{
    return mp_assault_is_active(formation_id) ? orders[formation_id].enemy : -1;
}

int mp_assault_building(int formation_id)
{
    return mp_assault_is_active(formation_id) ? orders[formation_id].building_id : 0;
}

void mp_assault_cancel(int formation_id)
{
    if (is_order_id(formation_id)) {
        clear_order(&orders[formation_id]);
    }
}

// ---------- targets ----------

// the player owning the tile: the owner of the building on it, else of the land (his zone), else of what he built on
// it (a road); -1 for nobody, Caesar, or a building not in use
static int owner_of_tile(int grid_offset)
{
    int building_id = map_building_at(grid_offset);
    if (building_id > 0) {
        if (BUILDING_IS_CAESAR(building_id) || building_get(building_id)->state != BUILDING_STATE_IN_USE) {
            return -1;
        }
        return BUILDING_OWNER(building_id);
    }
    int owner = mp_territory_owner(grid_offset);
    if (owner >= 0) {
        return owner;
    }
    owner = map_owner_get_claimed(grid_offset);
    return owner == MAP_OWNER_NONE || owner == MAP_OWNER_CAESAR ? -1 : owner;
}

// the larger of the two distances from the tile to the nearest tile of the building
static int distance_to_building(const building *b, int x, int y)
{
    int size = b->size > 0 ? b->size : 1;
    int dx = x < b->x ? b->x - x : (x >= b->x + size ? x - (b->x + size - 1) : 0);
    int dy = y < b->y ? b->y - y : (y >= b->y + size ? y - (b->y + size - 1) : 0);
    return dx > dy ? dx : dy;
}

static int is_target(int enemy, int building_id, int x, int y)
{
    const building *b = building_get(building_id);
    return b->state == BUILDING_STATE_IN_USE && BUILDING_OWNER(building_id) == enemy &&
        distance_to_building(b, x, y) <= MP_ASSAULT_RADIUS;
}

static int has_target_within_radius(int enemy, int x, int y)
{
    for (int id = enemy * MAX_BUILDINGS + 1; id < (enemy + 1) * MAX_BUILDINGS; id++) {
        if (is_target(enemy, id, x, y)) {
            return 1;
        }
    }
    return 0;
}

int mp_assault_enemy_at(int player_id, int x, int y)
{
    if (!game_rules_is_multiplayer() || !mp_war_any_fighting() || !map_grid_is_inside(x, y, 1)) {
        return -1;
    }
    int enemy = owner_of_tile(map_grid_offset(x, y));
    // one's own city too: a player never fights himself
    if (enemy < 0 || enemy >= MAX_PLAYERS || !mp_war_is_fighting(player_id, enemy)) {
        return -1;
    }
    return has_target_within_radius(enemy, x, y) ? enemy : -1;
}

// The tile next to the building where the standard goes (its first soldier stands there, next to the building): one
// the legion reaches from where it stands (the routing distances are computed from there), with no building on it,
// the nearest to the legion; ties go to the first in the order of the scan
static int standing_tile(const formation *m, const building *b, int *x, int *y)
{
    int size = b->size > 0 ? b->size : 1;
    int found = 0;
    int best_distance = 0;
    for (int yy = b->y - 1; yy <= b->y + size; yy++) {
        for (int xx = b->x - 1; xx <= b->x + size; xx++) {
            if (xx >= b->x && xx < b->x + size && yy >= b->y && yy < b->y + size) {
                continue; // the building itself
            }
            if (!map_grid_is_inside(xx, yy, 1)) {
                continue;
            }
            int offset = map_grid_offset(xx, yy);
            if (map_building_at(offset) > 0 || map_routing_distance(offset) <= 0) {
                continue;
            }
            if (xx == m->x_home && yy == m->y_home && (xx != m->standard_x || yy != m->standard_y)) {
                continue; // formation_legion_move_to refuses the tile where the first soldier stands
            }
            int d = calc_maximum_distance(m->x_home, m->y_home, xx, yy);
            if (!found || d < best_distance) {
                found = 1;
                best_distance = d;
                *x = xx;
                *y = yy;
            }
        }
    }
    return found;
}

// The next building to attack: a building of the enemy within the radius of the target picked, the nearest to (x, y)
// among those the legion can stand next to, other than `excluded`; ties go to the lowest id. Its standing tile in
// (*tx, *ty). As the invaders of the original game (figure/formation_enemy.c), the legion goes to the nearest of the
// buildings it may attack, but the player chose the quarter: the nearest one, of any kind.
static int next_building(const formation *m, const assault_order *o, int x, int y, int excluded, int *tx, int *ty)
{
    map_routing_calculate_distances(m->x_home, m->y_home);
    int best_id = 0;
    int best_distance = 0;
    for (int id = o->enemy * MAX_BUILDINGS + 1; id < (o->enemy + 1) * MAX_BUILDINGS; id++) {
        if (id == excluded || !is_target(o->enemy, id, o->x, o->y)) {
            continue;
        }
        const building *b = building_get(id);
        int d = distance_to_building(b, x, y);
        if (best_id && d >= best_distance) {
            continue;
        }
        int sx, sy;
        if (standing_tile(m, b, &sx, &sy)) {
            best_id = id;
            best_distance = d;
            *tx = sx;
            *ty = sy;
        }
    }
    return best_id;
}

// the legion goes to the tile; 0 when it cannot (the move order refuses it)
static int march(formation *m, int x, int y)
{
    if (m->standard_x == x && m->standard_y == y && !m->is_at_fort) {
        return 1; // already there, or on its way
    }
    formation_legion_move_to(m, x, y);
    return m->standard_x == x && m->standard_y == y && !m->is_at_fort;
}

// the legion of the current player that may receive the order, else 0
static formation *legion_able_to_attack(int formation_id)
{
    if (formation_id < FORMATION_FIRST || formation_id >= FORMATION_END) {
        return 0; // only a legion of the player who gives the order
    }
    formation *m = formation_get(formation_id);
    if (!m->in_use || !m->is_legion || m->in_distant_battle || m->cursed_by_mars || m->num_figures <= 0 ||
        formation_has_low_morale(m)) {
        return 0;
    }
    return m;
}

int mp_assault_is_offered(int formation_id)
{
    if (!game_rules_is_multiplayer() || !mp_war_any_fighting() || !legion_able_to_attack(formation_id)) {
        return 0;
    }
    for (int p = 0; p < player_context_num_players(); p++) {
        if (mp_war_is_fighting(player_context_current_player, p)) {
            return 1;
        }
    }
    return 0;
}

int mp_assault_order(int formation_id, int x, int y)
{
    int self = player_context_current_player;
    formation *m = legion_able_to_attack(formation_id);
    int enemy = m ? mp_assault_enemy_at(self, x, y) : -1;
    if (enemy < 0) {
        return 0;
    }
    assault_order o;
    clear_order(&o);
    o.active = 1;
    o.enemy = (int8_t) enemy;
    o.x = x;
    o.y = y;
    // first the building picked, or the nearest one to the tile picked
    int tx, ty;
    o.building_id = next_building(m, &o, x, y, 0, &tx, &ty);
    if (!o.building_id) {
        return 0; // the legion cannot stand next to any of them
    }
    formation_legion_move_to(m, tx, ty);
    if (m->standard_x != tx || m->standard_y != ty) {
        return 0;
    }
    orders[formation_id] = o;
    return 1;
}

// ---------- every tick ----------

typedef struct {
    int deployed;   // soldiers going to the standard, standing with it or mopping up
    int fighting;   // soldiers in a melee
    int busy;       // soldiers going to the standard, mopping up or in a melee
    int razing;     // soldiers standing with the standard with something of the enemy next to them
} legion_status;

static void get_status(const formation *m, legion_status *s)
{
    memset(s, 0, sizeof(legion_status));
    for (int i = 0; i < MAX_FORMATION_FIGURES; i++) {
        if (m->figures[i] <= 0) {
            continue;
        }
        const figure *f = figure_get(m->figures[i]);
        if (figure_is_dead(f) || f->formation_id != m->id) {
            continue;
        }
        switch (f->action_state) {
            case FIGURE_ACTION_150_ATTACK:
                s->fighting++;
                s->busy++;
                break;
            case FIGURE_ACTION_83_SOLDIER_GOING_TO_STANDARD:
            case FIGURE_ACTION_86_SOLDIER_MOPPING_UP:
                s->deployed++;
                s->busy++;
                break;
            case FIGURE_ACTION_84_SOLDIER_AT_STANDARD:
                s->deployed++;
                if (mp_war_soldier_has_target_next_to(f)) {
                    s->razing++;
                }
                break;
            default:
                break; // at rest, going to the fort, fleeing, to the barracks or to a distant battle
        }
    }
}

static int may_go_on(int formation_id, const assault_order *o, const legion_status *s)
{
    formation *m = formation_get(formation_id);
    if (!m->in_use || !m->is_legion || m->in_distant_battle || m->cursed_by_mars || formation_has_low_morale(m)) {
        return 0;
    }
    if (!mp_war_is_fighting(FORMATION_OWNER(formation_id), o->enemy)) {
        return 0; // peace, signed or imposed: the legion goes home as any other (mp/war.c)
    }
    // sent home by something else, or no soldier left
    return s->deployed > 0 || s->fighting > 0;
}

static void update_order(int formation_id, assault_order *o)
{
    legion_status s;
    formation *m = formation_get(formation_id);
    get_status(m, &s);
    if (!may_go_on(formation_id, o, &s)) {
        clear_order(o);
        return;
    }
    int excluded = 0;
    const building *b = building_get(o->building_id);
    if (o->building_id && b->state == BUILDING_STATE_IN_USE && BUILDING_OWNER(o->building_id) == o->enemy) {
        // the building still stands: the legion goes on marching or pulling down
        if (s.busy || s.razing) {
            o->stall_ticks = 0;
            return;
        }
        if (++o->stall_ticks < MAX_STALL_TICKS) {
            return;
        }
        excluded = o->building_id; // none of its soldiers could stand next to it: another one
    } else if (s.razing) {
        return; // it fell: what still stands next to the soldiers falls first
    }
    o->stall_ticks = 0;
    int tx, ty;
    int next = next_building(m, o, m->standard_x, m->standard_y, excluded, &tx, &ty);
    if (!next || !march(m, tx, ty)) {
        clear_order(o); // nothing left within the radius: the order is done, the legion stays there
        return;
    }
    o->building_id = next;
}

void mp_assault_update(void)
{
    if (!game_rules_is_multiplayer()) {
        return;
    }
    int previous = player_context_current();
    for (int i = 1; i < MAX_ORDERS; i++) {
        if (!orders[i].active) {
            continue;
        }
        if (FORMATION_OWNER(i) >= player_context_num_players()) {
            clear_order(&orders[i]);
            continue;
        }
        player_context_switch(FORMATION_OWNER(i)); // its warnings are those of its player
        update_order(i, &orders[i]);
    }
    player_context_switch(previous);
}

// ---------- saved game ----------

void mp_assault_save_state(buffer *buf)
{
    int count = 0;
    for (int i = 0; i < MAX_ORDERS; i++) {
        count += orders[i].active != 0;
    }
    buffer_write_i32(buf, STATE_VERSION);
    buffer_write_i32(buf, count);
    for (int i = 0; i < MAX_ORDERS; i++) {
        const assault_order *o = &orders[i];
        if (!o->active) {
            continue;
        }
        buffer_write_i32(buf, i);
        buffer_write_i8(buf, o->enemy);
        buffer_write_i32(buf, o->x);
        buffer_write_i32(buf, o->y);
        buffer_write_i32(buf, o->building_id);
        buffer_write_i32(buf, o->stall_ticks);
    }
}

void mp_assault_load_state(buffer *buf)
{
    mp_assault_reset();
    int version = buffer_read_i32(buf);
    int count = buffer_read_i32(buf);
    if (version < 1 || version > STATE_VERSION || count < 0 || count > MAX_ORDERS) {
        return;
    }
    for (int n = 0; n < count; n++) {
        int id = buffer_read_i32(buf);
        int enemy = buffer_read_i8(buf);
        int x = buffer_read_i32(buf);
        int y = buffer_read_i32(buf);
        int building_id = buffer_read_i32(buf);
        int stall_ticks = buffer_read_i32(buf);
        if (!is_order_id(id) || enemy < 0 || enemy >= MAX_PLAYERS ||
            (building_id != 0 && (building_id < 0 || BUILDING_OWNER(building_id) != enemy))) {
            continue; // nonsense: no order
        }
        assault_order *o = &orders[id];
        o->active = 1;
        o->enemy = (int8_t) enemy;
        o->x = x;
        o->y = y;
        o->building_id = building_id;
        o->stall_ticks = stall_ticks;
    }
    if (buf->overflow) {
        mp_assault_reset();
    }
}
