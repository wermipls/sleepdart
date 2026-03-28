#include "file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <SDL3/SDL_filesystem.h>
#include "szx_file.h"
#include "vector.h"
#include "log.h"
#include "unicode.h"

int64_t file_get_size(const char *path)
{
    if (path == NULL) return -1;

    struct stat s;
    if (stat(path, &s)) {
        return -2;
    }

    return s.st_size;
}

int file_is_regular_file(char *path)
{
    if (path == NULL) return -1;

    struct stat s;
    if (stat(path, &s)) {
        return -2;
    }

    if (S_ISREG(s.st_mode)) {
        return true;
    } else {
        return false;
    }
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
char *file_get_extension(char *path)
{
    if (path == NULL) {
        return NULL;
    }

    if (!file_is_regular_file(path)) {
        return NULL;
    }

    char *p = path;
    while (*p != 0) {
        p++;
    }

    p--;
    char *last = p;

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

int file_path_append(char *dst, const char *a, const char *b, size_t len)
{
    if (!dst || !a || !b || !len) {
        return -1;
    }

    const char *p = a;
    bool has_dir_separator = false;
    while (*p) {
        p++;
    }
    if (p > a) {
        p--;
        has_dir_separator = file_is_directory_separator(*p);
    }

    // FIXME: should use PathAppend on win32, probably?
    if (has_dir_separator) {
        snprintf(dst, len, "%s%s", a, b);
    } else {
        snprintf(dst, len, "%s/%s", a, b);
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
    return 0;
}

void file_free_list(char **list)
{
    if (list == NULL) {
        return;
    }

    for (size_t i = 0; i < vector_len(list); i++) {
        if (list[i] != NULL) {
            free(list[i]);
        }
    }

    vector_free(list);
}

static SDL_EnumerationResult list_directory_files_cb(void *userdata, const char *dirname, const char *fname)
{
    char ***pvec = userdata;
    char **files = *pvec;

    size_t len_fname = strlen(fname);
    size_t len_dirname = strlen(dirname);

    size_t fullpath_len = len_dirname + len_fname + 1;
    char *fullpath = malloc(fullpath_len);
    if (!fullpath) {
        return SDL_ENUM_FAILURE;
    }

    int err = file_path_append(fullpath, dirname, fname, fullpath_len);
    if (err) {
        free(fullpath);
        return SDL_ENUM_FAILURE;
    }

    if (!file_is_regular_file(fullpath)) {
        free(fullpath);
        return SDL_ENUM_CONTINUE;
    }

    free(fullpath);

    char *p = malloc(len_fname+1);
    if (!p) {
        return SDL_ENUM_FAILURE;
    }
    strncpy(p, fname, len_fname+1);
    vector_add(files, p);
    // vector ptr may have changed because of reallocation.
    *pvec = files;

    return SDL_ENUM_CONTINUE;
}

char **file_list_directory_files(char *path)
{
    char **files = vector_create();
    if (!SDL_EnumerateDirectory(path, list_directory_files_cb, &files)) {
        dlog(LOG_ERRSILENT, "failed to enumerate files in directory: %s", SDL_GetError());
        vector_free(files);
        return NULL;
    }
    return files;
}

enum FileType file_detect_type(char *path)
{
    char *ext = file_get_extension(path);

    if (ext != NULL) {
        // detect by extension
        size_t len = strlen(ext);
        char ext_lower[len+1];
        for (size_t i = 0; i < len; i++) {
            ext_lower[i] = tolower(ext[i]);
        }

        // .tap files aren't reliably possible to detect by binary data alone
        if (strncmp(ext_lower, "tap", 3) == 0) {
            return FTYPE_TAP;
        }

        if (strncmp(ext_lower, "sna", 3) == 0) {
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

    wpath = utf8_to_utf16(path, NULL);
    if (wpath == NULL)
        return NULL;

    file = _wfopen(wpath, mode);
    free(wpath);
    return file;
}
#endif
