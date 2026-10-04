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

void game_rules_save_state(buffer *buf)
{
    buffer_write_i32(buf, data.mode);
    buffer_write_i32(buf, data.multiplayer.difficulty);
    buffer_write_i32(buf, data.multiplayer.gods_enabled);
    buffer_write_i32(buf, data.multiplayer.fix_immigration_bug);
    buffer_write_i32(buf, data.multiplayer.fix_100_year_ghosts);
}

void game_rules_load_state(buffer *buf)
{
    data.mode = buffer_read_i32(buf);
    data.multiplayer.difficulty = buffer_read_i32(buf);
    data.multiplayer.gods_enabled = buffer_read_i32(buf);
    data.multiplayer.fix_immigration_bug = buffer_read_i32(buf);
    data.multiplayer.fix_100_year_ghosts = buffer_read_i32(buf);
}
