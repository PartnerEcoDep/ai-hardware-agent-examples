/**
 * @file convai_config_file.c
 * @brief Configuration file reader implementation.
 *
 * Reads a simple key=value config file from the executable's directory.
 * Supports both space-separated and newline-separated entries.
 *
 * On WS63 this feature is intentionally compiled out: the platform has no
 * filesystem path to the executable (LiteOS has no /proc/self/exe), so init()
 * always fails and the ~10KB static entry table would be dead BSS. The public
 * API is still defined (returning NULL / -1) so convai_bridge_defaults.c's
 * cfg_or() fallback to hardcoded defaults keeps working unchanged.
 */

#include "convai_config_file.h"
#include "convai_memory_budget.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef PLATFORM_TYPE_WS63

/* ---- WS63: no-op stubs (no static entry table -> 0 BSS) ---- */

const char *convai_config_file_get(const char *key)
{
    (void)key;
    return NULL;
}

int convai_config_file_init_path(const char *path)
{
    (void)path;
    return -1;
}

int convai_config_file_init(void)
{
    return -1;
}

void convai_config_file_deinit(void)
{
}

#else /* !PLATFORM_TYPE_WS63 — full filesystem-backed implementation */

/* ---- platform-specific executable directory ---- */

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

/* ---- internal constants ---- */

#define CONVAI_CONFIG_FILENAME      "convai.cfg"
#define CONVAI_CONFIG_MAX_LINE      CONVAI_BUDGET_CONFIG_LINE_BYTES
#define CONVAI_CONFIG_MAX_ENTRIES   CONVAI_BUDGET_CONFIG_ENTRY_COUNT
#define CONVAI_CONFIG_KEY_MAX       CONVAI_BUDGET_CONFIG_KEY_BYTES
#define CONVAI_CONFIG_VALUE_MAX     CONVAI_BUDGET_CONFIG_VALUE_BYTES

/* ---- internal structures ---- */

typedef struct {
    char key[CONVAI_CONFIG_KEY_MAX];
    char value[CONVAI_CONFIG_VALUE_MAX];
} config_entry_t;

static config_entry_t g_entries[CONVAI_CONFIG_MAX_ENTRIES];
static int            g_entry_count = 0;

/* ---- helpers ---- */

/**
 * Trim leading and trailing whitespace (including CR/LF) in-place.
 * Returns the trimmed string.
 */
static char *trim(char *s)
{
    if (s == NULL) return NULL;

    /* trim leading */
    while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n') s++;

    /* trim trailing */
    char *end = s + strlen(s);
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' ||
                       end[-1] == '\r' || end[-1] == '\n')) {
        end--;
    }
    *end = '\0';
    return s;
}

/**
 * Get the directory containing the current executable.
 * Returns the byte count written to buf (excluding null terminator),
 * or -1 on failure.
 */
static int get_exe_dir(char *buf, size_t size)
{
    if (buf == NULL || size == 0) return -1;

#ifdef _WIN32
    char exe_path[CONVAI_CONFIG_MAX_LINE];
    DWORD len = GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
    if (len == 0 || len >= sizeof(exe_path)) return -1;

    /* find last backslash or forward slash */
    char *last = strrchr(exe_path, '\\');
    if (last == NULL) last = strrchr(exe_path, '/');
    if (last == NULL) return -1;

    size_t dir_len = (size_t)(last - exe_path);
    if (dir_len >= size) return -1;

    memcpy(buf, exe_path, dir_len);
    buf[dir_len] = '\0';
    return (int)dir_len;
#else
    const char *proc_path = "/proc/self/exe";
    char exe_path[CONVAI_CONFIG_MAX_LINE];
    ssize_t len = readlink(proc_path, exe_path, sizeof(exe_path) - 1);
    if (len <= 0 || (size_t)len >= sizeof(exe_path)) return -1;
    exe_path[len] = '\0';

    /* find last slash */
    char *last = strrchr(exe_path, '/');
    if (last == NULL) return -1;

    size_t dir_len = (size_t)(last - exe_path);
    if (dir_len >= size) return -1;

    memcpy(buf, exe_path, dir_len);
    buf[dir_len] = '\0';
    return (int)dir_len;
#endif
}

/**
 * Parse a single line for key=value pairs.
 * Supports space-separated and newline-separated entries.
 * Lines whose first non-blank character is '#' or ';' are comments.
 * Parses in-place (modifies the input buffer).
 */
static int parse_config_line(char *line)
{
    char *p = line;

    /* skip full-line comments and blank lines */
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '#' || *p == ';' || *p == '\0' || *p == '\r' || *p == '\n') {
        return 0;
    }

    while (*p != '\0') {
        /* skip whitespace between tokens */
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (*p == '\0') break;

        /* mark start of token */
        char *token_start = p;

        /* advance to end of token (whitespace or null) */
        while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') p++;

        /* null-terminate this token */
        char token_end = *p;
        *p = '\0';

        /* look for '=' separator within the token */
        char *eq = strchr(token_start, '=');
        if (eq != NULL) {
            *eq = '\0';
            char *key = trim(token_start);
            char *val = trim(eq + 1);

            if (*key != '\0' && g_entry_count < CONVAI_CONFIG_MAX_ENTRIES) {
                size_t klen = strlen(key);
                size_t vlen = strlen(val);
                if (klen >= CONVAI_CONFIG_KEY_MAX)  klen = CONVAI_CONFIG_KEY_MAX - 1;
                if (vlen >= CONVAI_CONFIG_VALUE_MAX) vlen = CONVAI_CONFIG_VALUE_MAX - 1;

                memcpy(g_entries[g_entry_count].key, key, klen);
                g_entries[g_entry_count].key[klen] = '\0';
                memcpy(g_entries[g_entry_count].value, val, vlen);
                g_entries[g_entry_count].value[vlen] = '\0';
                g_entry_count++;
            }
        }

        /* restore the separator char and advance */
        *p = token_end;
        if (*p != '\0') p++;
    }

    return 0;
}

/* ---- public API ---- */

const char *convai_config_file_get(const char *key)
{
    if (key == NULL) return NULL;

    for (int i = 0; i < g_entry_count; i++) {
        if (strcmp(g_entries[i].key, key) == 0) {
            return g_entries[i].value;
        }
    }
    return NULL;
}

int convai_config_file_init_path(const char *path)
{
    if (path == NULL) return -1;

    FILE *fp = fopen(path, "r");
    if (fp == NULL) return -1;

    char line[CONVAI_CONFIG_MAX_LINE];
    int  ret = 0;

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (parse_config_line(line) != 0) {
            ret = -1;
            break;
        }
    }

    fclose(fp);
    return ret;
}

int convai_config_file_init(void)
{
    char dir[CONVAI_CONFIG_MAX_LINE];
    if (get_exe_dir(dir, sizeof(dir)) < 0) return -1;

    char path[CONVAI_CONFIG_MAX_LINE];
    int written = snprintf(path, sizeof(path), "%s/%s",
                           dir, CONVAI_CONFIG_FILENAME);
    if (written < 0 || (size_t)written >= sizeof(path)) return -1;

    return convai_config_file_init_path(path);
}

void convai_config_file_deinit(void)
{
    g_entry_count = 0;
    memset(g_entries, 0, sizeof(g_entries));
}

#endif /* PLATFORM_TYPE_WS63 */
