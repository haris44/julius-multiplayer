#include "automation.h"

#include "game/file.h"
#include "game/settings.h"
#include "game/tick.h"
#include "game/time.h"
#include "graphics/screen.h"
#include "graphics/screenshot.h"
#include "input/mouse.h"
#include "window/city.h"

#include "SDL.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif

#define MAX_PATH_LENGTH 1024
#define MILLIS_PER_FRAME 16
#define START_TIME 1000
// Frames allowed per requested tick for the "run" command: the slowest game speed needs ~44 frames per tick
#define RUN_FRAMES_PER_TICK 60
#define RUN_EXTRA_FRAMES 600

typedef enum {
    PENDING_NONE = 0,
    PENDING_MOUSE_UP,
    PENDING_DRAG_MOVE,
    PENDING_KEY_UP
} pending_action;

static struct {
    int active;
    int finished;
    int failed;
    int mouse_initialized;
    struct {
        int active;
        int target_tick;
        int frames_left;
    } run;
    char *script;
    char **lines;
    int num_lines;
    int current_line;
    int wait_frames;
    time_millis time;
    char base_dir[MAX_PATH_LENGTH];
    struct {
        pending_action action;
        int x;
        int y;
        int button;
        SDL_Scancode scancode;
        Uint16 modifiers;
    } pending;
    struct {
        int requested;
        int full_city;
        char filename[MAX_PATH_LENGTH];
    } screenshot;
} data;

static void log_message(const char *message, const char *value)
{
    SDL_Log("[automation] line %d: %s %s", data.current_line, message, value ? value : "");
}

static void fail(const char *message, const char *value)
{
    log_message(message, value);
    data.failed = 1;
    data.finished = 1;
    SDL_Event event = { .type = SDL_QUIT };
    SDL_PushEvent(&event);
}

static int is_absolute_path(const char *path)
{
#ifdef _WIN32
    return path[0] == '\\' || path[0] == '/' || (path[0] && path[1] == ':');
#else
    return path[0] == '/';
#endif
}

static const char *resolve_path(const char *path)
{
    static char resolved[MAX_PATH_LENGTH];
    if (is_absolute_path(path)) {
        snprintf(resolved, MAX_PATH_LENGTH, "%s", path);
    } else {
        snprintf(resolved, MAX_PATH_LENGTH, "%s/%s", data.base_dir, path);
    }
    return resolved;
}

static int split_lines(char *script)
{
    int capacity = 64;
    data.lines = malloc(capacity * sizeof(char *));
    if (!data.lines) {
        return 0;
    }
    // Every line is kept, including empty ones, so that line numbers in logs match the file
    char *line = script;
    while (*line) {
        char *end = strchr(line, '\n');
        if (end) {
            *end = 0;
        }
        size_t length = strlen(line);
        if (length && line[length - 1] == '\r') {
            line[length - 1] = 0;
        }
        if (data.num_lines == capacity) {
            capacity *= 2;
            char **lines = realloc(data.lines, capacity * sizeof(char *));
            if (!lines) {
                return 0;
            }
            data.lines = lines;
        }
        data.lines[data.num_lines++] = line;
        if (!end) {
            break;
        }
        line = end + 1;
    }
    return 1;
}

int platform_automation_init(const char *script_file)
{
    memset(&data, 0, sizeof(data));
    if (!getcwd(data.base_dir, MAX_PATH_LENGTH)) {
        SDL_Log("[automation] unable to determine current directory");
        return 0;
    }
    FILE *fp = fopen(script_file, "rb");
    if (!fp) {
        SDL_Log("[automation] unable to open script %s", script_file);
        return 0;
    }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    data.script = malloc(size + 1);
    if (!data.script || fread(data.script, 1, size, fp) != (size_t) size) {
        fclose(fp);
        SDL_Log("[automation] unable to read script %s", script_file);
        return 0;
    }
    fclose(fp);
    data.script[size] = 0;
    if (!split_lines(data.script)) {
        SDL_Log("[automation] out of memory");
        return 0;
    }
    data.time = START_TIME;
    data.active = 1;
    SDL_Log("[automation] loaded %s (%d lines)", script_file, data.num_lines);
    return 1;
}

int platform_automation_is_active(void)
{
    return data.active;
}

int platform_automation_exit_code(void)
{
    return data.failed ? 3 : 0;
}

time_millis platform_automation_time(void)
{
    return data.time;
}

static void push_mouse_motion(int x, int y)
{
    SDL_Event event = { .type = SDL_MOUSEMOTION };
    event.motion.x = x;
    event.motion.y = y;
    SDL_PushEvent(&event);
}

static void push_mouse_button(int x, int y, int button, int down)
{
    SDL_Event event = { .type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP };
    event.button.x = x;
    event.button.y = y;
    event.button.button = button;
    event.button.state = down ? SDL_PRESSED : SDL_RELEASED;
    event.button.clicks = 1;
    SDL_PushEvent(&event);
}

static void push_key(SDL_Scancode scancode, Uint16 modifiers, int down)
{
    SDL_Event event = { .type = down ? SDL_KEYDOWN : SDL_KEYUP };
    event.key.state = down ? SDL_PRESSED : SDL_RELEASED;
    event.key.keysym.scancode = scancode;
    event.key.keysym.sym = SDL_GetKeyFromScancode(scancode);
    event.key.keysym.mod = modifiers;
    SDL_PushEvent(&event);
}

static int parse_key(char *name, SDL_Scancode *scancode, Uint16 *modifiers)
{
    *modifiers = KMOD_NONE;
    char *plus;
    while ((plus = strchr(name, '+')) != 0 && plus[1]) {
        *plus = 0;
        if (SDL_strcasecmp(name, "ctrl") == 0) {
            *modifiers |= KMOD_LCTRL;
        } else if (SDL_strcasecmp(name, "shift") == 0) {
            *modifiers |= KMOD_LSHIFT;
        } else if (SDL_strcasecmp(name, "alt") == 0) {
            *modifiers |= KMOD_LALT;
        } else if (SDL_strcasecmp(name, "gui") == 0 || SDL_strcasecmp(name, "cmd") == 0) {
            *modifiers |= KMOD_LGUI;
        } else {
            return 0;
        }
        name = plus + 1;
    }
    *scancode = SDL_GetScancodeFromName(name);
    return *scancode != SDL_SCANCODE_UNKNOWN;
}

static int handle_pending(void)
{
    switch (data.pending.action) {
        case PENDING_MOUSE_UP:
            push_mouse_button(data.pending.x, data.pending.y, data.pending.button, 0);
            break;
        case PENDING_DRAG_MOVE:
            push_mouse_motion(data.pending.x, data.pending.y);
            data.pending.action = PENDING_MOUSE_UP;
            return 1;
        case PENDING_KEY_UP:
            push_key(data.pending.scancode, data.pending.modifiers, 0);
            break;
        default:
            return 0;
    }
    data.pending.action = PENDING_NONE;
    return 1;
}

// Monotonic tick counter derived from the game date: 50 ticks per day, 16 days per month, 12 months per year
static int total_ticks(void)
{
    return ((game_time_year() * 12 + game_time_month()) * 16 + game_time_day()) * 50 + game_time_tick();
}

static void request_screenshot(const char *filename, int full_city)
{
    data.screenshot.requested = 1;
    data.screenshot.full_city = full_city;
    snprintf(data.screenshot.filename, MAX_PATH_LENGTH, "%s", resolve_path(filename));
}

/**
 * Executes one script line.
 * @return 1 when the command needs the current frame to be processed before the next command
 */
static int execute(char *line)
{
    while (*line == ' ' || *line == '\t') {
        line++;
    }
    if (!*line || *line == '#') {
        return 0;
    }
    char command[32] = { 0 };
    char arg[MAX_PATH_LENGTH] = { 0 };
    int x = 0, y = 0, x2 = 0, y2 = 0, n = 0;
    sscanf(line, "%31s", command);
    const char *rest = line + strlen(command);
    while (*rest == ' ' || *rest == '\t') {
        rest++;
    }

    if (strcmp(command, "wait") == 0) {
        if (sscanf(rest, "%d", &n) != 1 || n < 0) {
            fail("invalid wait:", rest);
            return 1;
        }
        if (n == 0) {
            return 0;
        }
        data.wait_frames = n - 1;
        return 1;
    } else if (strcmp(command, "mouse") == 0) {
        if (sscanf(rest, "%d %d", &x, &y) != 2) {
            fail("invalid mouse:", rest);
            return 1;
        }
        push_mouse_motion(x, y);
        return 1;
    } else if (strcmp(command, "click") == 0 || strcmp(command, "rclick") == 0) {
        if (sscanf(rest, "%d %d", &x, &y) != 2) {
            fail("invalid click:", rest);
            return 1;
        }
        int button = command[0] == 'r' ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT;
        push_mouse_motion(x, y);
        push_mouse_button(x, y, button, 1);
        data.pending.action = PENDING_MOUSE_UP;
        data.pending.x = x;
        data.pending.y = y;
        data.pending.button = button;
        return 1;
    } else if (strcmp(command, "drag") == 0) {
        if (sscanf(rest, "%d %d %d %d", &x, &y, &x2, &y2) != 4) {
            fail("invalid drag:", rest);
            return 1;
        }
        push_mouse_motion(x, y);
        push_mouse_button(x, y, SDL_BUTTON_LEFT, 1);
        data.pending.action = PENDING_DRAG_MOVE;
        data.pending.x = x2;
        data.pending.y = y2;
        data.pending.button = SDL_BUTTON_LEFT;
        return 1;
    } else if (strcmp(command, "key") == 0) {
        SDL_Scancode scancode;
        Uint16 modifiers;
        snprintf(arg, MAX_PATH_LENGTH, "%s", rest);
        if (!parse_key(arg, &scancode, &modifiers)) {
            fail("unknown key:", rest);
            return 1;
        }
        push_key(scancode, modifiers, 1);
        data.pending.action = PENDING_KEY_UP;
        data.pending.scancode = scancode;
        data.pending.modifiers = modifiers;
        return 1;
    } else if (strcmp(command, "text") == 0) {
        SDL_Event event = { .type = SDL_TEXTINPUT };
        snprintf(event.text.text, sizeof(event.text.text), "%s", rest);
        SDL_PushEvent(&event);
        return 1;
    } else if (strcmp(command, "load") == 0) {
        const char *path = resolve_path(rest);
        if (!game_file_load_saved_game(path)) {
            fail("unable to load saved game:", path);
            return 1;
        }
        window_city_show();
        log_message("loaded", path);
        return 1;
    } else if (strcmp(command, "save") == 0) {
        const char *path = resolve_path(rest);
        if (!game_file_write_saved_game(path)) {
            fail("unable to save game:", path);
            return 1;
        }
        log_message("saved", path);
        return 0;
    } else if (strcmp(command, "ticks") == 0) {
        if (sscanf(rest, "%d", &n) != 1 || n < 0) {
            fail("invalid ticks:", rest);
            return 1;
        }
        for (int i = 0; i < n; i++) {
            game_tick_run();
        }
        return 1;
    } else if (strcmp(command, "run") == 0) {
        // Unlike "ticks", lets the normal game loop advance the simulation, including UI pauses
        if (sscanf(rest, "%d", &n) != 1 || n <= 0) {
            fail("invalid run:", rest);
            return 1;
        }
        data.run.active = 1;
        data.run.target_tick = total_ticks() + n;
        data.run.frames_left = n * RUN_FRAMES_PER_TICK + RUN_EXTRA_FRAMES;
        return 1;
    } else if (strcmp(command, "speed") == 0) {
        if (sscanf(rest, "%d", &n) != 1 || n < 10 || n > 500) {
            fail("invalid speed (10-500):", rest);
            return 1;
        }
        setting_reset_speeds(n, setting_scroll_speed());
        return 0;
    } else if (strcmp(command, "screenshot") == 0 || strcmp(command, "cityshot") == 0) {
        if (!*rest) {
            fail("missing file name for", command);
            return 1;
        }
        request_screenshot(rest, command[0] == 'c');
        return 1;
    } else if (strcmp(command, "log") == 0) {
        log_message("log:", rest);
        return 0;
    } else if (strcmp(command, "quit") == 0) {
        data.finished = 1;
        SDL_Event event = { .type = SDL_QUIT };
        SDL_PushEvent(&event);
        return 1;
    }
    fail("unknown command:", command);
    return 1;
}

void platform_automation_before_frame(void)
{
    if (!data.active || data.finished) {
        return;
    }
    data.time += MILLIS_PER_FRAME;
    if (!data.mouse_initialized) {
        // Start with the mouse in the middle of the screen: at (0,0) the city view would scroll on its own
        mouse_set_position(screen_width() / 2, screen_height() / 2);
        data.mouse_initialized = 1;
    }
    if (handle_pending()) {
        return;
    }
    if (data.run.active) {
        if (total_ticks() < data.run.target_tick) {
            if (--data.run.frames_left <= 0) {
                fail("run timed out: is the game paused or not showing the city?", 0);
            }
            return;
        }
        data.run.active = 0;
    }
    if (data.wait_frames > 0) {
        data.wait_frames--;
        return;
    }
    while (data.current_line < data.num_lines && !data.finished) {
        char *line = data.lines[data.current_line++];
        if (execute(line)) {
            return;
        }
    }
    if (!data.finished) {
        SDL_Log("[automation] end of script, quitting");
        data.finished = 1;
        SDL_Event event = { .type = SDL_QUIT };
        SDL_PushEvent(&event);
    }
}

void platform_automation_after_frame(void)
{
    if (!data.active || !data.screenshot.requested) {
        return;
    }
    data.screenshot.requested = 0;
    if (graphics_save_screenshot_to_file(data.screenshot.filename, data.screenshot.full_city)) {
        log_message("screenshot saved:", data.screenshot.filename);
    } else {
        fail("unable to save screenshot:", data.screenshot.filename);
    }
}
