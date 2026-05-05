#include "file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>
#include "szx_file.h"
#include "log.h"
#include <physfs.h>
#include "sleepdart_info.h"
#include "sleepdart_assets.h"
#include "unicode.h"

static const char *file_pref_dir = NULL;

static const char *physfs_error()
{
    PHYSFS_ErrorCode err = PHYSFS_getLastErrorCode();
    return (err) ? PHYSFS_getErrorByCode(err) : "unknown error";
}

bool file_init(const char *argv0)
{
    if (!PHYSFS_init(argv0)) {
        dlog(LOG_ERR, "Failed to init PhysicsFS: %s", physfs_error());
        return false;
    }

    bool portable_mode = false;
    char *path = file_path_append(NULL, file_get_basedir(), "config.ini");
    if (path && file_exists(path)) {
        dlog(LOG_INFO, "Config file found in program directory, running in portable mode.");
        portable_mode = true;
    }
    free(path);

    file_pref_dir = portable_mode ? file_get_basedir() : PHYSFS_getPrefDir(SLEEPDART_ORG, SLEEPDART_NAME);
    PHYSFS_setWriteDir(file_pref_dir);
    PHYSFS_mountMemory(sleepdart_assets, sizeof(sleepdart_assets), NULL, "__assets.zip", NULL, 0);
    PHYSFS_mount("./", NULL, 0);
    PHYSFS_mount(file_pref_dir, NULL, 0);

    return true;
}

bool file_exists(const char *path)
{
    if (!path) return false;

    struct stat s;
    if (stat(path, &s)) {
        return false;
    }

    return true;
}

int64_t file_get_size(const char *path)
{
    if (path == NULL) return -1;

    struct stat s;
    if (stat(path, &s)) {
        return -2;
    }

    return s.st_size;
}

bool file_is_directory_separator(char c)
{
#ifdef _WIN32
    if (c == '/' || c == '\\') return true;
#else
    if (c == '/') return true;
#endif
    return false;
}

/* returns a pointer to the file extension in the path (w/o the dot),
 * or NULL if there is no valid extension present or on an error */
const char *file_get_extension(const char *path)
{
    if (path == NULL) {
        return NULL;
    }

    const char *p = path;
    while (*p != 0) {
        p++;
    }

    p--;
    const char *last = p;

    while (*p != '.') {
        if (file_is_directory_separator(*p)) {
            return NULL;
        }

        p--;

        if (p <= path || *p == 0) {
            return NULL;
        }
    }

    // no sane file ends with a dot.
    // windows makes it very difficult to create such files.
    // but let's handle it anyway just in case
    if (p >= last) return NULL;

    return p + 1;
}

const char *file_get_basedir()
{
    return SDL_GetBasePath();
}

const char *file_get_prefdir()
{
    return file_pref_dir;
}

char *file_path_append(char *dst, const char *a, const char *b)
{
    if (!a || !b) {
        return NULL;
    }

    size_t len_a = strlen(a);
    size_t len_b = strlen(b);

    bool has_dir_separator = file_is_directory_separator(a[len_a - 1]);

    size_t len_dst = len_a + len_b + 1;
    if (!has_dir_separator) {
        // account for extra dir separator.
        len_dst += 1;
    }

    bool append_in_place = (dst == a);
    dst = realloc(dst, len_dst);
    if (!dst) {
        return NULL;
    }

    if (!append_in_place) {
        strncpy(dst, a, len_a);
    }

    if (has_dir_separator) {
        strncpy(&dst[len_a], b, len_b + 1);
    } else {
        dst[len_a] = '/';
        strncpy(&dst[len_a+1], b, len_b + 1);
    }

#ifdef _WIN32
    char *dp = dst;
    while (*dp != 0) {
        if (*dp == '/') {
            *dp = '\\';
        }
        dp++;
    }
#endif
    return dst;
}

enum FileType file_detect_type(const char *path)
{
    const char *ext = file_get_extension(path);

    if (ext != NULL) {
        // .tap files aren't reliably possible to detect by binary data alone
        if (SDL_strncasecmp(ext, "tap", 3) == 0) {
            return FTYPE_TAP;
        }

        // ditto.
        if (SDL_strncasecmp(ext, "sna", 3) == 0) {
            return FTYPE_SNA;
        }
    }

    if (szx_is_valid_file(path)) {
        return FTYPE_SZX;
    }

    return FTYPE_UNKNOWN;
}

/* returns a pointer to a line string (without newline character),
 * or null on error or if there is none left.
 * user is expected to free the returned pointer */
char *file_read_line(FILE *f)
{
    char buf[256];
    char *str = NULL;
    size_t str_len = 0;
    size_t offset = 0;
    while (fgets(buf, sizeof(buf), f)) {
        size_t len = strcspn(buf, "\r\n");
        str_len += len;
        char *str_new = realloc(str, str_len+1);
        if (str_new == NULL) {
            dlog(LOG_ERRSILENT, "%s: realloc fail", __func__);
            free(str);
            return NULL;
        }
        str = str_new;
        strncpy(&str[offset], buf, len);
        offset = str_len;
        str[str_len] = 0;
        if (len < sizeof(buf)-1) {
            break;
        }
    }

    return str;
}

// Lifted and adapted from https://github.com/Photosounder/fopen_utf8
// "[...] just put it in your code, who cares where it came from". I do :')
#ifdef _WIN32
FILE *fopen_utf8_win32(const char *path, const wchar_t *mode)
{
    wchar_t *wpath;
    FILE *file;

    wpath = utf8_to_utf16(path);
    if (wpath == NULL)
        return NULL;

    file = _wfopen(wpath, mode);
    SDL_free(wpath);
    return file;
}
#endif
