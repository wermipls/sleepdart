#include "argparser.h"
#include <inttypes.h>
#include "vector.h"
#include "log.h"

ArgParser_t *argparser_create()
{
    ArgParser_t *parser = calloc(sizeof(ArgParser_t), 1);
    if (parser == NULL) {
        return NULL;
    }

    parser->args = vector_create();
    if (parser->args == NULL) {
        free(parser);
        return NULL;
    }

    return parser;
}

void argparser_free(ArgParser_t *parser) {
    if (parser) {
        if (parser->args) {
            for (size_t i = 0; i < vector_len(parser->args); i++) {
                if (parser->args[i].result) {
                    free(parser->args[i].result);
                }
            }
            vector_free(parser->args);
        }
        free(parser);
    }
}

int args_get_index_by_name(ArgParser_t *parser, const char *name)
{
    for (size_t i = 0; i < vector_len(parser->args); i++) {
        int result = strncmp(parser->args[i].name, name, sizeof(parser->args->name));
        if (result == 0) {
            return i;
        }
    }

    return -1;
}

int args_get_index_by_char(ArgParser_t *parser, const char name)
{
    if (name == 0) {
        return -1;
    }

    for (size_t i = 0; i < vector_len(parser->args); i++) {
        if (parser->args[i].name_short == name) {
            return i;
        }
    }

    return -1;
}

int args_get_index_positional(ArgParser_t *parser, int start_i)
{
    for (size_t i = start_i; i < vector_len(parser->args); i++) {
        if (parser->args[i].positional == true) {
            return i;
        }
    }

    return -1;
}

bool arg_requires_parameter(struct Argument *arg)
{
    if (arg->type == ARG_STORE_TRUE) {
        return false;
    } else {
        return true;
    }
}

void *arg_parse_parameter(struct Argument *arg, char *param)
{
    switch (arg->type) 
    {
    case ARG_STRING: ;
        size_t len = strlen(param) + 1;
        char *s = malloc(len);
        if (!s) break;
        memcpy(s, param, len);
        s[len-1] = 0;
        return s;
    case ARG_INT: ;
        int *i = malloc(sizeof(int));
        if (!i) break;
        *i = atoi(param);
        return i;
    case ARG_FLOAT: ;
        float *f = malloc(sizeof(float));
        if (!f) break;
        *f = atof(param);
        return f;
    case ARG_STORE_TRUE: ;
        bool *st = malloc(sizeof(bool));
        if (!st) break;
        *st = true;
        return st;
    default:
        return NULL;
    }

    return NULL;
}

void argparser_add_arg(
    ArgParser_t *parser, const char *name, char name_short,
    enum ArgumentType type, bool force_optional, const char *help)
{
    if (!name && name_short == 0) {
        dlog(LOG_ERRSILENT, "%s: Argument name cannot be empty", __func__);
        return;
    }

    if (name && name[0] == 0) {
        dlog(LOG_ERRSILENT, "%s: Argument name cannot be empty", __func__);
        return;
    }

    struct Argument arg = { 0 };

    arg.type = type;
    arg.help = help;

    if (name && name[0] == '-') {
        arg.optional = true;
        if (name[1] == '-') {
            strncpy(arg.name, name+2, sizeof(arg.name)-1);
        } else {
            name_short = name[1];
        }
    } else if (name) {
        strncpy(arg.name, name, sizeof(arg.name)-1);
    }

    arg.name[sizeof(arg.name)-1] = 0;

    if (name_short) {
        arg.optional = true;
        arg.name_short = name_short;
    }

    if (arg.name[0]) {
        int i = args_get_index_by_name(parser, arg.name);
        if (i >= 0) {
            dlog(LOG_ERRSILENT, "%s: Argument %s duplicates name of argument %d", __func__, arg.name, i);
            return;
        }
    }

    if (arg.name_short) {
        int i = args_get_index_by_char(parser, arg.name_short);
        if (i >= 0) {
            dlog(LOG_ERRSILENT, "%s: Argument -%c duplicates name of argument %d", __func__, i);
            return;
        }
    }

    if (!arg.optional) {
        arg.positional = true;
    }

    arg.optional |= force_optional;

    if (arg.positional) {
        if (!arg.optional && parser->positional_no != parser->positional_req) {
            dlog(LOG_ERRSILENT, "%s: Positional non-optional argument defined after other optional positional args", __func__);
            return;
        }
        parser->positional_no += 1;
        parser->positional_req += !arg.optional;
    }

    vector_add(parser->args, arg);
}

int argparser_parse(ArgParser_t *parser, int argc, char *argv[])
{
    if (argc == 0) {
        dlog(LOG_ERRSILENT, "%s: ¿como estas?", __func__);
        return -1;
    }

    int positional_last_idx = -1;
    int positional_handled = 0;

    for (int i = 1; i < argc; i++) {
        char *argstr = argv[i];
        int arg_idx = -1;

        if (argstr[0] == '-') {
            if (argstr[1] == '-') {
                arg_idx = args_get_index_by_name(parser, argstr+2);
            } else {
                arg_idx = args_get_index_by_char(parser, argstr[1]);
            }
        }

        if (arg_idx >= 0) {
            struct Argument *arg = &parser->args[arg_idx];
            i++;
            bool need_param = arg_requires_parameter(arg);
            if (need_param && i >= argc) {
                dlog(LOG_ERRSILENT, "Missing parameter for argument \"%s\"", argstr);
                return -2;
            }

            if (need_param) {
                arg->result = arg_parse_parameter(arg, argv[i]);
            } else {
                arg->result = arg_parse_parameter(arg, 0);
                i--;
            }
            continue;
        }

        if (positional_handled < parser->positional_no) {
            arg_idx = args_get_index_positional(parser, positional_last_idx + 1);
        }

        if (arg_idx >= 0) {
            struct Argument *arg = &parser->args[arg_idx];
            arg->result = arg_parse_parameter(arg, argv[i]);
            positional_handled += 1;
            positional_last_idx = arg_idx;
        }
    }

    if (positional_handled < parser->positional_req) {
        dlog(LOG_ERRSILENT, "Expected %d positional arguments, got %d",
                      parser->positional_req, positional_handled);
        return -3;
    }

    return 0;
}

void *argparser_get(ArgParser_t *parser, const char *arg)
{
    if (arg == NULL) {
        return NULL;
    }

    if (arg[0] == 0) {
        return NULL;
    }

    int idx;
    if (arg[1] == 0) {
        idx = args_get_index_by_char(parser, arg[0]);
    } else {
        idx = args_get_index_by_name(parser, arg);
    }

    if (idx < 0) {
        return NULL;
    } else {
        return parser->args[idx].result;
    }
}
