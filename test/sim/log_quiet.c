// Log stub for simtool: stdout only carries data, errors go to stderr, info is dropped
#include "core/log.h"

#include <stdio.h>

void log_info(const char *msg, const char *param_str, int param_int)
{
}

void log_error(const char *msg, const char *param_str, int param_int)
{
    fprintf(stderr, "ERROR: %s", msg);
    if (param_str) {
        fprintf(stderr, "  %s", param_str);
    }
    if (param_int) {
        fprintf(stderr, "  %d", param_int);
    }
    fprintf(stderr, "\n");
}
