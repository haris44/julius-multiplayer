#ifndef GAME_SAVE_FORMAT_H
#define GAME_SAVE_FORMAT_H

#include "core/buffer.h"

/**
 * @file
 * Width of the fields that Caesar III saved games store too narrowly for large multiplayer maps
 * (coordinates, grid offsets, storage and road network numbers): narrow in classic saved games, wide
 * in multiplayer saved games and state checksums (doc/mp/DECISIONS.md D-024).
 */

extern int save_format_wide;

static inline void save_write_coord(buffer *buf, int value)
{
    if (save_format_wide) {
        buffer_write_u16(buf, (uint16_t) value);
    } else {
        buffer_write_u8(buf, (uint8_t) value);
    }
}

static inline int save_read_coord(buffer *buf)
{
    return save_format_wide ? buffer_read_u16(buf) : buffer_read_u8(buf);
}

static inline void save_write_offset(buffer *buf, int value)
{
    if (save_format_wide) {
        buffer_write_i32(buf, value);
    } else {
        buffer_write_i16(buf, (int16_t) value);
    }
}

static inline int save_read_offset(buffer *buf)
{
    return save_format_wide ? buffer_read_i32(buf) : buffer_read_i16(buf);
}

/** Small numbers (storage, road network) */
static inline void save_write_small_id(buffer *buf, int value)
{
    save_write_coord(buf, value);
}

static inline int save_read_small_id(buffer *buf)
{
    return save_read_coord(buf);
}

#endif // GAME_SAVE_FORMAT_H
