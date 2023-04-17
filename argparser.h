#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef void (*ArgCallback_t)(void *value);

enum ArgumentType
{
    ARG_STRING = 0,
    ARG_INT,
    ARG_FLOAT,
    ARG_STORE_TRUE,
    ARG_HELP,
};

struct Argument
{
    char name[50];
    char name_short;
    enum ArgumentType type;
    bool optional;
    bool positional;
    const char *help;
    void *result;
    ArgCallback_t callback;
};

typedef struct ArgParser {
    struct Argument *args;
    int positional_no;
    int positional_req;
    const char *name;
} ArgParser_t;

ArgParser_t *argparser_create(const char *name);
void argparser_free(ArgParser_t *parser);
void argparser_add_arg(
    ArgParser_t *parser, const char *name, char name_short,
    enum ArgumentType type, bool force_optional, const char *help);
int argparser_parse(ArgParser_t *parser, int argc, char *argv[]);
void *argparser_get(ArgParser_t *parser, const char *arg);
