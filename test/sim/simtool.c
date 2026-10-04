// Headless simulation tool: loads saved games, runs ticks exactly like the autopilot and
// prints state checksums. See doc/mp/TESTING.md.
#include "core/time.h"
#include "game/file.h"
#include "game/game.h"
#include "game/settings.h"
#include "mp/checksum.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SAMPLES 100000

static time_millis clock_millis;

static int usage(void)
{
    printf("Usage:\n");
    printf("  simtool checksum SAVE                  checksum of a loaded saved game\n");
    printf("  simtool trace SAVE TICKS [STEP]        checksum every STEP ticks (default 1)\n");
    printf("  simtool pieces SAVE [TICKS]            checksum of every saved game piece after TICKS\n");
    printf("  simtool idempotence SAVE TICKS [STEP]  loads and runs SAVE twice in one process,\n");
    printf("                                         fails if the two traces differ\n");
    return 2;
}

static int load(const char *file)
{
    if (!game_file_load_saved_game(file)) {
        printf("Unable to load saved game %s\n", file);
        return 0;
    }
    return 1;
}

// Same stepping as test/sav/run.c: 2 ms per call at speed 500 gives exactly one tick per game_run().
// The clock never goes back, so several runs can follow each other in one process.
static void run_one_tick(void)
{
    clock_millis += 2;
    time_set_millis(clock_millis);
    game_run();
}

static int run_trace(int ticks, int step, uint64_t *samples, FILE *out)
{
    int num_samples = 0;
    setting_reset_speeds(500, setting_scroll_speed());
    uint64_t checksum = mp_checksum_state();
    if (out) {
        fprintf(out, "%d %016" PRIx64 "\n", 0, checksum);
    }
    if (samples) {
        samples[num_samples] = checksum;
    }
    num_samples++;
    for (int tick = 1; tick <= ticks; tick++) {
        run_one_tick();
        if (tick % step == 0 || tick == ticks) {
            checksum = mp_checksum_state();
            if (out) {
                fprintf(out, "%d %016" PRIx64 "\n", tick, checksum);
            }
            if (samples && num_samples < MAX_SAMPLES) {
                samples[num_samples] = checksum;
            }
            num_samples++;
        }
    }
    return num_samples;
}

static void print_piece(const char *name, uint64_t checksum, void *userdata)
{
    printf("%-34s %016" PRIx64 "\n", name, checksum);
}

static int command_checksum(const char *file)
{
    if (!load(file)) {
        return 2;
    }
    printf("%016" PRIx64 "\n", mp_checksum_state());
    return 0;
}

static int command_trace(const char *file, int ticks, int step)
{
    if (!load(file)) {
        return 2;
    }
    run_trace(ticks, step, 0, stdout);
    return 0;
}

static int command_pieces(const char *file, int ticks)
{
    if (!load(file)) {
        return 2;
    }
    if (ticks > 0) {
        run_trace(ticks, ticks, 0, 0);
    }
    uint64_t total = mp_checksum_state_pieces(print_piece, 0);
    printf("%-34s %016" PRIx64 "\n", "TOTAL", total);
    return 0;
}

static int command_idempotence(const char *file, int ticks, int step)
{
    static uint64_t first[MAX_SAMPLES];
    static uint64_t second[MAX_SAMPLES];
    if (!load(file)) {
        return 2;
    }
    int n = run_trace(ticks, step, first, 0);
    if (!load(file)) {
        return 2;
    }
    run_trace(ticks, step, second, 0);
    if (n > MAX_SAMPLES) {
        n = MAX_SAMPLES;
    }
    for (int i = 0; i < n; i++) {
        if (first[i] != second[i]) {
            int tick = i == n - 1 ? ticks : i * step;
            printf("Second run of %s diverges at tick %d (sample %d): %016" PRIx64 " != %016" PRIx64 "\n",
                file, tick, i, first[i], second[i]);
            return 1;
        }
    }
    printf("Both runs of %s are identical over %d ticks (%d samples)\n", file, ticks, n);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        return usage();
    }
    const char *command = argv[1];
    const char *file = argv[2];
    int ticks = argc > 3 ? atoi(argv[3]) : 0;
    int step = argc > 4 ? atoi(argv[4]) : 1;
    if (step <= 0) {
        step = 1;
    }

    if (!game_pre_init() || !game_init()) {
        printf("Unable to initialize the game\n");
        return 2;
    }

    int result;
    if (strcmp(command, "checksum") == 0) {
        result = command_checksum(file);
    } else if (strcmp(command, "trace") == 0 && argc > 3) {
        result = command_trace(file, ticks, step);
    } else if (strcmp(command, "pieces") == 0) {
        result = command_pieces(file, ticks);
    } else if (strcmp(command, "idempotence") == 0 && argc > 3) {
        result = command_idempotence(file, ticks, step);
    } else {
        result = usage();
    }
    game_exit();
    return result;
}
