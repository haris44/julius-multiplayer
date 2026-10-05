#ifndef PLATFORM_SCREEN_H
#define PLATFORM_SCREEN_H

#include "graphics/color.h"
#include "input/cursor.h"

int platform_screen_create(const char *title, int dispay_scale_percentage, int display_id);
void platform_screen_destroy(void);

int platform_screen_resize(int pixel_width, int pixel_height);
void platform_screen_move(int x, int y);

int platform_screen_get_scale(void);

/**
 * Where the system says the cursor is, in the coordinates of the game, clamped to the window
 * @param inside Set to whether the cursor is really over the window
 * @return 0 when the system does not tell (no window system, window without the focus)
 */
int platform_screen_get_system_mouse_position(int *x, int *y, int *inside);

void platform_screen_set_fullscreen(void);
void platform_screen_set_windowed(void);
void platform_screen_set_window_size(int logical_width, int logical_height);
void platform_screen_center_window(void);

#ifdef _WIN32
void platform_screen_recreate_texture(void);
#endif

void platform_screen_clear(void);
void platform_screen_update(void);
void platform_screen_render(void);

void platform_screen_generate_mouse_cursor_texture(int cursor_id, cursor_scale scale, const color_t *cursor_colors);

#endif // PLATFORM_SCREEN_H
