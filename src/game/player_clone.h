#ifndef GAME_PLAYER_CLONE_H
#define GAME_PLAYER_CLONE_H

/**
 * @file
 * Copying the city of one player into the slices of another player, at another place of the map.
 * Used to build twin cities for the isolation test (doc/mp/ROADMAP.md M3.7) and, later, to start
 * every player from the same city.
 */

typedef struct {
    int from;      /**< Player whose city is copied */
    int to;        /**< Player receiving the copy */
    int dx;        /**< Shift of the copy on the map, in tiles */
    int dy;
    int grid_delta; /**< Same shift as a grid offset */
} player_clone;

/** Id in the slice of the copied player, moved to the same place in the slice of the receiving player */
static inline int player_clone_id(const player_clone *c, int id, int slice_size)
{
    if (id > 0 && id / slice_size == c->from) {
        return id + (c->to - c->from) * slice_size;
    }
    return id;
}

#endif // GAME_PLAYER_CLONE_H
