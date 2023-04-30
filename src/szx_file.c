#include "szx_file.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define U32_FROM_STR(str) (*((uint32_t *)(str)))

const char expected_magic[4] = "ZXST";

bool szx_is_valid_file(char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return false;
    }

    uint32_t magic;
    size_t bytes = fread(&magic, 1, sizeof(magic), f);
    if (bytes < sizeof(magic)) {
        fclose(f);
        return false;
    }

    fclose(f);

    if (U32_FROM_STR(expected_magic) != magic) {
        return false;
    }

    return true;
}

void szx_free(SZX_t *szx) {
    if (szx == NULL) return;
    
    if (szx->block != NULL) {
        for (size_t i = 0; i < szx->blocks; i++) {
            if (szx->block[i].data == NULL) {
                break;
            }
            free(szx->block[i].data);
        }
        free(szx->block);
    }
    free(szx);
}

SZX_t *szx_load_file(char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        return NULL;
    }

    struct SZXHeader header;
    size_t bytes = fread(&header, 1, sizeof(header), f);
    if (bytes < sizeof(header)) {
        fclose(f);
        return NULL;
    }

    if (header.magic != U32_FROM_STR(expected_magic)) {
        fclose(f);
        return NULL;
    }

    size_t block_count = 0;

    while (!feof(f)) {
        struct SZXBlockHeader block_header;
        bytes = fread(&block_header, 1, sizeof(block_header), f);
        if (bytes < sizeof(block_header)) {
            if (feof(f) && bytes == 0) {
                break;
            }
            fclose(f);
            return NULL;
        }

        if (block_header.size == 0) {
            fclose(f);
            return NULL;
        }

        block_count++;

        fseek(f, block_header.size, SEEK_CUR);
    }

    SZX_t *szx = calloc(sizeof(SZX_t), 1);
    if (szx == NULL) {
        return NULL;
    }

    szx->header = header;
    szx->blocks = block_count;

    szx->block = calloc(sizeof(*szx->block), block_count);
    if (szx->block == NULL) {
        szx_free(szx);
        fclose(f);
        return NULL;
    }
    
    fseek(f, sizeof(header), SEEK_SET);

    size_t current_block = 0;

    while (!feof(f)) {
        struct SZXBlockHeader block_header;
        bytes = fread(&block_header, 1, sizeof(block_header), f);
        if (bytes < sizeof(block_header)) {
            if (feof(f) && bytes == 0) {
                break;
            }
            fclose(f);
            return NULL;
        }

        if (block_header.size == 0) {
            fclose(f);
            return NULL;
        }

        uint8_t *data = malloc(block_header.size);
        if (data == NULL) {
            szx_free(szx);
            fclose(f);
            return NULL;
        }

        bytes = fread(data, 1, block_header.size, f);
        if (bytes < block_header.size) {
            free(data);
            szx_free(szx);
            fclose(f);
            return NULL;
        }

        szx->block[current_block].header = block_header;
        szx->block[current_block].data = data;

        current_block++;
    }

    fclose(f);
    return szx;
}
