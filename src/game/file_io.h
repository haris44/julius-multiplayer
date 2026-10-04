#ifndef GAME_FILE_IO_H
#define GAME_FILE_IO_H

int game_file_io_read_scenario(const char *filename);

int game_file_io_write_scenario(const char *filename);

int game_file_io_read_saved_game(const char *filename, int offset);

int game_file_io_write_saved_game(const char *filename);

int game_file_io_delete_saved_game(const char *filename);

typedef void (*game_file_io_piece_visitor)(const char *name, const unsigned char *data, int size, void *userdata);

/**
 * Serializes the current game state in memory, exactly as a saved game but uncompressed,
 * and passes every piece to the visitor in file order. Buffers are zeroed first, so the
 * result does not depend on previously loaded games.
 * @param visitor Function called for every piece
 * @param userdata Passed to the visitor
 */
void game_file_io_visit_saved_game(game_file_io_piece_visitor visitor, void *userdata);

/**
 * Serializes the state of the current player with wide fields (multiplayer saved games, checksums),
 * passing the bytes actually written for every piece
 * @param include_grids Whether to include the map grids, shared by all players
 */
void game_file_io_visit_wide_state(game_file_io_piece_visitor visitor, void *userdata, int include_grids);

/**
 * Fills a piece of a wide state when loading
 * @return 1 when the piece was found
 */
typedef int (*game_file_io_piece_provider)(const char *name, unsigned char *data, int capacity, void *userdata);

/**
 * Loads the state of the current player from wide pieces
 */
int game_file_io_load_wide_state(game_file_io_piece_provider provider, void *userdata);

#endif // GAME_FILE_IO_H
