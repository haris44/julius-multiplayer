#ifndef MP_AUDIT_H
#define MP_AUDIT_H

/**
 * @file
 * Counts the effects a city has on the buildings of another player (services, workers, goods, fires...):
 * the "connected neighbours" test checks that there are none (doc/mp/ROADMAP.md M4.3, D-018).
 * Counting only: the simulation never reads it.
 */

/**
 * Called where a city acts on a building: counts the building if it belongs to another player
 * @param kind Short name of the effect, for the report
 */
void mp_audit_effect(int building_id, const char *kind);

void mp_audit_reset(void);

int mp_audit_violations(void);

/**
 * Kind of the first violation, or 0
 */
const char *mp_audit_first_kind(void);

#endif // MP_AUDIT_H
