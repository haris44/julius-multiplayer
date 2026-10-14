#ifndef WIDGET_CITY_H
#define WIDGET_CITY_H

#include "graphics/tooltip.h"
#include "input/hotkey.h"
#include "input/mouse.h"

typedef struct {
    int x;
    int y;
} pixel_coordinate;

void widget_city_draw(void);
void widget_city_draw_for_figure(int figure_id, pixel_coordinate *coord);

void widget_city_draw_construction_cost_and_size(void);
void widget_city_draw_touch_buttons(void);

int widget_city_has_input(void);
void widget_city_handle_input(const mouse *m, const hotkeys *h);
void widget_city_handle_input_military(const mouse *m, const hotkeys *h, int legion_formation_id);

/**
 * The order "attack" of a legion (multiplayer, D-087): the next click on the city, with the legion selected, picks its
 * target instead of moving it
 */
void widget_city_pick_legion_attack(int legion_formation_id);

/**
 * The prompt of the order "attack" while it waits for its target (drawn by the military view of the city)
 */
void widget_city_draw_legion_attack_prompt(void);

void widget_city_get_tooltip(tooltip_context *c);

void widget_city_clear_current_tile(void);

int widget_city_current_grid_offset(void);

#endif // WIDGET_CITY_H
