#ifndef BUILDING_CONSTRUCTION_CLEAR_H
#define BUILDING_CONSTRUCTION_CLEAR_H

/**
 * Clears land
 * @param measure_only Whether to measure only
 * @param x_start Start X
 * @param y_start Start Y
 * @param x_end End X
 * @param y_end End Y
 * @return Number of tiles cleared
 */
int building_construction_clear_land(int measure_only, int x_start, int y_start, int x_end, int y_end);

/**
 * Tells whether clearing this area needs the player to confirm deleting a fort or a bridge.
 * Only one question is asked: the fort one when both apply.
 */
void building_construction_clear_land_needs_confirmation(int x_start, int y_start, int x_end, int y_end,
    int *fort, int *bridge);

/**
 * Answers given in advance to the fort/bridge questions (1 = yes, -1 = no), used when replaying a
 * command: no popup is shown, the result is the same as answering it
 */
void building_construction_clear_land_preset_answers(int fort_answer, int bridge_answer);

void building_construction_clear_land_reset_answers(void);

#endif // BUILDING_CONSTRUCTION_CLEAR_H
