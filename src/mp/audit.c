#include "audit.h"

#include "building/building.h"
#include "game/player_context.h"

static struct {
    int violations;
    const char *first_kind;
} data;

void mp_audit_effect(int building_id, const char *kind)
{
    if (building_id > 0 && BUILDING_OWNER(building_id) != player_context_current_player) {
        if (!data.violations) {
            data.first_kind = kind;
        }
        data.violations++;
    }
}

void mp_audit_reset(void)
{
    data.violations = 0;
    data.first_kind = 0;
}

int mp_audit_violations(void)
{
    return data.violations;
}

const char *mp_audit_first_kind(void)
{
    return data.first_kind;
}
