#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "at_helpers.h"

#include <lpac/utils.h>
#include <unistd.h>

inline void at_warning_message(void) {
    static char *message =
        "WARNING: AT driver is for demo purposes only.\n"
        "WARNING: AT driver strictly complies with \"ETSI TS 127 007\" specification.\n"
        "WARNING: Some operations (e.g: download, delete, etc.), may fail due to insufficient response time.\n";

    if (isatty(fileno(stdin))) {
        fprintf(stderr, "\033[0;31m%s\033[0m", message);
    } else {
        fprintf(stderr, "%s", message);
    }
}

char *at_channel_get(struct at_userdata *userdata, const int index) {
    if (index <= 0 || index > AT_MAX_LOGICAL_CHANNELS)
        return NULL;

    char **channels = userdata->channels;
    return channels[index];
}

int at_channel_set(struct at_userdata *userdata, const int index, const char *identifier) {
    if (index <= 0 || index > AT_MAX_LOGICAL_CHANNELS)
        return -1;

    char **channels = userdata->channels;

    if (channels[index]) {
        free(channels[index]);
    }

    channels[index] = identifier ? strdup(identifier) : NULL;
    return 0;
}

int at_channel_next_id(struct at_userdata *userdata) {
    int index = 1;
    char **channels = userdata->channels;

    while (index <= AT_MAX_LOGICAL_CHANNELS && channels[index] != NULL)
        index++;

    if (index > AT_MAX_LOGICAL_CHANNELS)
        return -1;

    return index;
}

int at_emit_command(struct at_userdata *userdata, const char *fmt, ...) {
    va_list args, args_length;
    va_start(args, fmt);

    va_copy(args_length, args);
    const int n = vsnprintf(NULL, 0, fmt, args_length);
    va_end(args_length);

    _cleanup_free_ char *formatted = calloc(n + 2 /* CR+LF */ + 1, 1);
    if (formatted == NULL) {
        va_end(args);
        return -1;
    }

    vsnprintf(formatted, n + 1, fmt, args);
    va_end(args);

    formatted[n + 0] = '\r'; // CR
    formatted[n + 1] = '\n'; // LF
    formatted[n + 2] = '\0'; // NUL

    AT_DEBUG_TX(formatted);

    int ret = at_write_command(userdata, formatted);
    return ret;
}

int at_run_init_cmds(struct at_userdata *userdata) {
    const char *init_cmds = getenv_str_or_default(ENV_AT_INIT_CMDS, NULL);
    if (init_cmds == NULL)
        return -1;

    while(init_cmds) {
        const char *cmd_end = strchr(init_cmds, ';');
        size_t cmd_len = cmd_end ? (cmd_end - init_cmds) : strlen(init_cmds);

        if (cmd_len > 0) {
            at_emit_command(userdata, "%.*s", cmd_len, init_cmds);
            if (at_expect(userdata, NULL, NULL) != 0) {
                fprintf(stderr, "AT init command \'%.*s\' failed\n", cmd_len, init_cmds);
                return -1;
            }
        }

        init_cmds = cmd_end ? (cmd_end + 1) : NULL;
    }

    return 0;
}
