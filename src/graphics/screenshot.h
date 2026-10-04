#ifndef GRAPHICS_SCREENSHOT_H
#define GRAPHICS_SCREENSHOT_H

void graphics_save_screenshot(int full_city);

/**
 * Saves a screenshot to the given file, without showing an in-game notice
 * @param filename PNG file to write
 * @param full_city Whether to render the whole city instead of the current screen
 * @return 1 on success, 0 on failure
 */
int graphics_save_screenshot_to_file(const char *filename, int full_city);

#endif // GRAPHICS_SCREENSHOT_H
