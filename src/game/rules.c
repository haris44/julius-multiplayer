#include "rules.h"

#include "core/config.h"
#include "game/settings.h"

static struct {
    game_mode mode;
    game_rules_settings multiplayer;
} data;

void game_rules_set_classic(void)
{
    data.mode = GAME_MODE_CLASSIC;
}

void game_rules_set_multiplayer(const game_rules_settings *settings)
{
    data.mode = GAME_MODE_MULTIPLAYER;
    data.multiplayer = *settings;
}

void game_rules_default_multiplayer_settings(game_rules_settings *settings)
{
    settings->difficulty = DIFFICULTY_HARD;
    settings->gods_enabled = 1;
    settings->fix_immigration_bug = 0;
    settings->fix_100_year_ghosts = 0;
    settings->ai_invasions = 1;
    settings->end_condition = GAME_END_NONE;
    settings->score_years = 10;
    settings->territories = 0; // on with the prepared maps; the tests on copied cities build anywhere
    settings->fog_of_war = 1;
}

const game_rules_settings *game_rules_multiplayer_settings(void)
{
    return &data.multiplayer;
}

game_mode game_rules_mode(void)
{
    return data.mode;
}

int game_rules_is_multiplayer(void)
{
    return data.mode == GAME_MODE_MULTIPLAYER;
}

int game_rules_difficulty(void)
{
    return data.mode == GAME_MODE_MULTIPLAYER ? data.multiplayer.difficulty : setting_difficulty();
}

int game_rules_gods_enabled(void)
{
    return data.mode == GAME_MODE_MULTIPLAYER ? data.multiplayer.gods_enabled : setting_gods_enabled();
}

int game_rules_fix_immigration_bug(void)
{
    return data.mode == GAME_MODE_MULTIPLAYER ?
        data.multiplayer.fix_immigration_bug : config_get(CONFIG_GP_FIX_IMMIGRATION_BUG);
}

int game_rules_fix_100_year_ghosts(void)
{
    return data.mode == GAME_MODE_MULTIPLAYER ?
        data.multiplayer.fix_100_year_ghosts : config_get(CONFIG_GP_FIX_100_YEAR_GHOSTS);
}

int game_rules_ai_invasions(void)
{
    return data.mode == GAME_MODE_MULTIPLAYER ? data.multiplayer.ai_invasions : 1;
}

game_end_condition game_rules_end_condition(void)
{
    return data.mode == GAME_MODE_MULTIPLAYER ? data.multiplayer.end_condition : GAME_END_NONE;
}

int game_rules_territories(void)
{
    return data.mode == GAME_MODE_MULTIPLAYER ? data.multiplayer.territories : 0;
}

int game_rules_fog_of_war(void)
{
    return data.mode == GAME_MODE_MULTIPLAYER ? data.multiplayer.fog_of_war : 0;
}

int game_rules_score_years(void)
{
    return data.multiplayer.score_years;
}

void game_rules_save_state(buffer *buf)
{
    buffer_write_i32(buf, data.mode);
    buffer_write_i32(buf, data.multiplayer.difficulty);
    buffer_write_i32(buf, data.multiplayer.gods_enabled);
    buffer_write_i32(buf, data.multiplayer.fix_immigration_bug);
    buffer_write_i32(buf, data.multiplayer.fix_100_year_ghosts);
    buffer_write_i32(buf, data.multiplayer.ai_invasions);
    buffer_write_i32(buf, data.multiplayer.end_condition);
    buffer_write_i32(buf, data.multiplayer.score_years);
    buffer_write_i32(buf, data.multiplayer.territories);
    buffer_write_i32(buf, data.multiplayer.fog_of_war);
}

void game_rules_load_state(buffer *buf)
{
    data.mode = buffer_read_i32(buf);
    data.multiplayer.difficulty = buffer_read_i32(buf);
    data.multiplayer.gods_enabled = buffer_read_i32(buf);
    data.multiplayer.fix_immigration_bug = buffer_read_i32(buf);
    data.multiplayer.fix_100_year_ghosts = buffer_read_i32(buf);
    data.multiplayer.ai_invasions = buffer_read_i32(buf);
    data.multiplayer.end_condition = buffer_read_i32(buf);
    data.multiplayer.score_years = buffer_read_i32(buf);
    data.multiplayer.territories = buffer_read_i32(buf); // 0 in games saved before territories
    data.multiplayer.fog_of_war = buffer_read_i32(buf);
}
