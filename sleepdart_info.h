#pragma once

#define SLEEPDART_NAME "sleepdart3"
#define SLEEPDART_VERSION "0.0.1"
#define SLEEPDART_DESCRIPTION "A half-hearted attempt at a reasonably accurate ZX Spectrum emulator"
#define SLEEPDART_REPO "https://github.com/wermipls/sleepdart3"
#ifdef GIT_DESCRIBE
    #undef SLEEPDART_VERSION
    #define SLEEPDART_VERSION GIT_DESCRIBE
#endif

