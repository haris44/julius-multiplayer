#include "display_options.h"

#include "game/settings.h"
#include "game/system.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "core/encoding.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"

#include <stdio.h>

static void button_fullscreen(int param1, int param2);
static void button_set_resolution(int id, int param2);
static void button_cancel(int param1, int param2);

static generic_button buttons[] = {
    {128, 136, 224, 20, button_fullscreen, button_none, 1, 0},
    {128, 160, 224, 20, button_set_resolution, button_none, 1, 0},
    {128, 184, 224, 20, button_set_resolution, button_none, 2, 0},
    {128, 208, 224, 20, button_set_resolution, button_none, 3, 0},
    {128, 232, 224, 20, button_set_resolution, button_none, 4, 0},
    {128, 256, 224, 20, button_cancel, button_none, 1, 0},
};

#define NUM_BUTTONS (sizeof(buttons) / sizeof(generic_button))

static struct {
    int focus_button_id;
    void (*close_callback)(void);
    int screen_width; // after the three sizes of the original game, the largest window on this screen
    int screen_height;
    uint8_t screen_label[64];
} data;

static void init(void (*close_callback)(void))
{
    data.focus_button_id = 0;
    data.close_callback = close_callback;
    data.screen_label[0] = 0;
    if (system_get_max_window_size(&data.screen_width, &data.screen_height)) {
        char label[64];
        snprintf(label, sizeof(label), "%d par %d (écran)", data.screen_width, data.screen_height);
        encoding_from_utf8(label, data.screen_label, sizeof(data.screen_label));
    }
}

static void draw_foreground(void)
{
    graphics_in_dialog();

    outer_panel_draw(96, 80, 18, 14);

    for (unsigned int i = 0; i < NUM_BUTTONS; i++) {
        label_draw(128, buttons[i].y, 14, data.focus_button_id == (int) i + 1 ? 1 : 2);
    }

    lang_text_draw_centered(42, 0, 128, 94, 224, FONT_LARGE_BLACK);

    lang_text_draw_centered(42, setting_fullscreen() ? 2 : 1, 128, 140, 224, FONT_NORMAL_GREEN);

    lang_text_draw_centered(42, 3, 128, 164, 224, FONT_NORMAL_GREEN);
    lang_text_draw_centered(42, 4, 128, 188, 224, FONT_NORMAL_GREEN);
    lang_text_draw_centered(42, 5, 128, 212, 224, FONT_NORMAL_GREEN);
    text_draw_centered(data.screen_label, 128, 236, 224, FONT_NORMAL_GREEN, 0);
    lang_text_draw_centered(42, 6, 128, 260, 224, FONT_NORMAL_GREEN);

    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    if (generic_buttons_handle_mouse(mouse_in_dialog(m), 0, 0, buttons, NUM_BUTTONS, &data.focus_button_id)) {
        return;
    }
    if (input_go_back_requested(m, h)) {
        data.close_callback();
    }
}

static void button_fullscreen(int param1, int param2)
{
    system_set_fullscreen(!setting_fullscreen());
    data.close_callback();
}

static void button_set_resolution(int id, int param2)
{
    switch (id) {
        case 1: system_resize(640, 480); break;
        case 2: system_resize(800, 600); break;
        case 3: system_resize(1024, 768); break;
        case 4:
            if (data.screen_label[0]) {
                system_resize(data.screen_width, data.screen_height);
            }
            break;
    }
    data.close_callback();
}

static void button_cancel(int param1, int param2)
{
    data.close_callback();
}

void window_display_options_show(void (*close_callback)(void))
{
    window_type window = {
        WINDOW_DISPLAY_OPTIONS,
        window_draw_underlying_window,
        draw_foreground,
        handle_input
    };
    init(close_callback);
    window_show(&window);
}
