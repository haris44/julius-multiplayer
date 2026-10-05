#include "effect.h"

#include "game/settings.h"
#include "mp/session.h"
#include "sound/channel.h"
#include "sound/device.h"

void sound_effect_set_volume(int percentage)
{
    for (int i = SOUND_CHANNEL_EFFECTS_MIN; i <= SOUND_CHANNEL_EFFECTS_MAX; i++) {
        sound_device_set_channel_volume(i, percentage);
    }
}

void sound_effect_play(int effect)
{
    // the fire, collapse or earthquake of another player's city is not heard here
    if (!setting_sound(SOUND_EFFECTS)->enabled || mp_session_is_other_players_city()) {
        return;
    }
    if (sound_device_is_channel_playing(effect)) {
        return;
    }
    sound_device_play_channel(effect, setting_sound(SOUND_EFFECTS)->volume);
}
