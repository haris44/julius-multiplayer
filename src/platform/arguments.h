#ifndef PLATFORM_ARGUMENTS_H
#define PLATFORM_ARGUMENTS_H

typedef struct {
    const char *data_directory;
    int display_scale_percentage;
    int cursor_scale_percentage;
    int force_windowed;
    int force_fullscreen;
    int display_id;
    const char *automation_script;
    const char *mp_host_save;   /**< --mp-host SAVE: hosts a network game starting from this saved game */
    int mp_players;             /**< --mp-players N: number of players of the hosted game */
    int mp_shared_city;         /**< --mp-shared-city: all players build in the same city */
    int mp_score_years;         /**< --mp-score-years N: the game ends by score after N years */
    const char *mp_join;        /**< --mp-join ADDRESS[:PORT]: joins a network game */
    int mp_port;                /**< --mp-port PORT */
} julius_args;

int platform_parse_arguments(int argc, char **argv, julius_args *output_args);

#endif // PLATFORM_ARGUMENTS_H
