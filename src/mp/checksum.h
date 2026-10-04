#ifndef MP_CHECKSUM_H
#define MP_CHECKSUM_H

#include <stdint.h>

/**
 * @file
 * Checksum of the simulation state, used to detect desynchronisations between players
 * and to find the first diverging tick in tests.
 *
 * It is a 64-bit FNV-1a hash of the game state serialized exactly as a saved game (explicit
 * little-endian, so identical on every platform). Parts written by the user interface are left
 * out: camera, view orientation, sounds, animations, construction previews, messages, figure
 * phrases, overlay flags and statistics. See doc/mp/DESIGN.md §3.8.
 */

typedef void (*mp_checksum_piece_callback)(const char *name, uint64_t checksum, void *userdata);

/**
 * @return Checksum of the current simulation state
 */
uint64_t mp_checksum_state(void);

/**
 * Computes the checksum of every saved game piece taken into account, for diagnostics
 * @param callback Called for each included piece, in file order
 * @param userdata Passed to the callback
 * @return Checksum of the whole state, same as mp_checksum_state()
 */
uint64_t mp_checksum_state_pieces(mp_checksum_piece_callback callback, void *userdata);

#endif // MP_CHECKSUM_H
