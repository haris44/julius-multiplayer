#include "lobby.h"

#include "game/settings.h"
#include "mp/lockstep.h"

// end of the game: Caesar makes his heir of the first city to one of these scores (D-053), or none (0)
static const int END_SCORES[] = { 500, 1000, 1500, 2000, 0 };
#define NUM_END_CHOICES 5
#define DEFAULT_END_CHOICE 1
#define NUM_DIFFICULTIES 5

static struct {
    int initialized;
    int difficulty;
    int gods;
    int end_choice;
    int ai_invasions;
    int fog_of_war;
    game_rules_settings shown;
} data;

void mp_lobby_rules_init(void)
{
    if (data.initialized) {
        return;
    }
    data.initialized = 1;
    game_rules_settings rules;
    game_rules_default_multiplayer_settings(&rules);
    // the lobby proposes an easy game (T4.4): the default rules of the tests stay hard
    data.difficulty = DIFFICULTY_EASY;
    data.gods = rules.gods_enabled;
    data.end_choice = DEFAULT_END_CHOICE;
    data.ai_invasions = rules.ai_invasions;
    data.fog_of_war = rules.fog_of_war;
}

int mp_lobby_rules_editable(void)
{
    // a game resumed from its saved game keeps its rules (D-073)
    return mp_lockstep_get_state() == MP_LOCKSTEP_OFF ||
        (mp_lockstep_is_host() && !mp_lockstep_rules_from_saved_game());
}

void mp_lobby_rules_settings(game_rules_settings *settings)
{
    mp_lobby_rules_init();
    game_rules_default_multiplayer_settings(settings);
    settings->difficulty = data.difficulty;
    settings->gods_enabled = data.gods;
    settings->ai_invasions = data.ai_invasions;
    settings->fog_of_war = data.fog_of_war;
    settings->end_condition = END_SCORES[data.end_choice] ? GAME_END_CAESAR : GAME_END_NONE;
    if (END_SCORES[data.end_choice]) {
        settings->caesar_score = END_SCORES[data.end_choice];
    }
}

void mp_lobby_change_rule(mp_lobby_rule rule)
{
    mp_lobby_rules_init();
    if (!mp_lobby_rules_editable()) {
        return; // the rules of the host
    }
    switch (rule) {
        case MP_LOBBY_RULE_DIFFICULTY:
            data.difficulty = (data.difficulty + 1) % NUM_DIFFICULTIES;
            break;
        case MP_LOBBY_RULE_GODS:
            data.gods = !data.gods;
            break;
        case MP_LOBBY_RULE_END:
            data.end_choice = (data.end_choice + 1) % NUM_END_CHOICES;
            break;
        case MP_LOBBY_RULE_INVASIONS:
            data.ai_invasions = !data.ai_invasions;
            break;
        case MP_LOBBY_RULE_FOG:
            data.fog_of_war = !data.fog_of_war;
            break;
    }
    if (mp_lockstep_get_state() == MP_LOCKSTEP_WAITING_FOR_PLAYERS && mp_lockstep_is_host()) {
        // hosting: the game and the players already there get the change now
        game_rules_settings rules;
        mp_lobby_rules_settings(&rules);
        mp_lockstep_set_rules(&rules);
    }
}

const game_rules_settings *mp_lobby_rules_shown(void)
{
    if (!mp_lobby_rules_editable() && mp_lockstep_lobby_rules()) {
        return mp_lockstep_lobby_rules();
    }
    mp_lobby_rules_settings(&data.shown);
    return &data.shown;
}

void mp_lobby_start_game(void)
{
    // the rules of the moment, whatever was chosen since "Host" (T4.11)
    game_rules_settings rules;
    mp_lobby_rules_settings(&rules);
    mp_lockstep_set_rules(&rules);
    mp_lockstep_start_game();
}
