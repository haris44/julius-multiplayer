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

#endif // GAME_FILE_IO_H
