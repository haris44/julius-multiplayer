#ifndef MP_PERMISSIONS_H
#define MP_PERMISSIONS_H

/**
 * @file
 * Permissions to exploit resources, different for every arrival point (doc/mp/DECISIONS.md D-020): the
 * original rule (our city of the empire map produces the raw material, or an open route supplies it) is
 * evaluated in the context of each city. Until maps define them (M6), the raw materials the starting city
 * may produce are shared out between the players:
 * - food stays allowed for everyone;
 * - iron, hence weapons, goes to a single player;
 * - the other raw materials (clay, timber, olives, vines, marble) go to the players in turn.
 */

/**
 * Shares out the production permissions of the current game between its cities
 */
void mp_permissions_share_out(void);

/**
 * Whether the current city may build this farm or raw material building: the rule of the build menu, applied to the
 * build commands of the players (a farm of a food its plan refuses, such as a pig farm on the coast, T4.15)
 * @return 1 for every other building
 */
int mp_permissions_may_build(int building_type);

#endif // MP_PERMISSIONS_H
