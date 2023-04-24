/* This is a funny experiment in implementing a vector type
 * with a clean API in C. It has some function-like macros,
 * which work properly only assuming a single reference in the program.
 * It's very evil and probably extremely easy to be mishandled.
 *
 * The various vector macros rely on the vector pointer being cast
 * to the appropriate type to derive the element size. The vector itself
 * does not store any information about the type.
 *
 * Example of use:
 *
 *     int *vec = vector_create();
 *     vector_resize(a, 8);
 *
 *     size_t i;
 *     for (i = 0, i < vector_len(vec); i++) {
 *         // can be accessed like a normal C array
 *         vec[i] = i;
 *     }
 * 
 *     while (i < 16) {
 *         // can also manipulate elements using special macros
 *         // vector will resize automagically if there's not enough space
 *         vector_add(vec, i);
 *         i++;
 *     }
 * 
 *     for (i = 0; i < vector_len(vec); i++) {
 *        printf("%d ", vec[i]);
 *     }
 * 
 *     vector_free(vec);
 * 
 * The code snippet above should print out a sequence of numbers from 0 to 15. 
 * 
 * This code is inspired by a very similar library, based on similar principles:
 * https://github.com/Mashpoe/c-vector
 * 
 * I found it, looked at the API and thought "OK so this should be possible",
 * then wrote my own implementation without looking at its code. */

#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h> // FIXME: replace with assertf?

struct VectorHeader {
    size_t count;
    size_t limit;
};

#define VEC_HDR_SIZE_ (sizeof(struct VectorHeader))

#define vector_get_headerptr(vecptr) (void *)((char *)(vecptr) - VEC_HDR_SIZE_)
#define vector_get_dataptr(vechdrptr) (void *)((char *)(vechdrptr) + VEC_HDR_SIZE_)

/* Creates a vector with 0 elements.
 * Returns a pointer to the vector, or NULL on error.
 * User is expected to cast the pointer to the desired type.
 * User must call vector_free after use to deallocate the memory. */
static inline void *vector_create()
{
    void *vec = malloc(VEC_HDR_SIZE_);
    if (vec == NULL) {
        return NULL;
    }
    struct VectorHeader *vh = vec;
    vh->count = 0;
    vh->limit = 0;

    return vector_get_dataptr(vec);
}

/* Deallocates a vector. */
static inline void vector_free(void *vec)
{
    if (vec == NULL) {
        return;
    }

    vec = vector_get_headerptr(vec);
    free(vec);
}

static inline size_t vector_len(void *vec)
{
    if (vec == NULL) {
        return 0;
    }
    struct VectorHeader *vh = vector_get_headerptr(vec);

    return vh->count;
}

#define vector_resize(vecptr, new_count) \
{                                                                               \
    assert(vecptr);                                                             \
    assert(sizeof(vecptr) == sizeof(void *));                                   \
    void *tmp_newptr__ = malloc(sizeof(*vecptr) * (new_count) + VEC_HDR_SIZE_); \
    assert(tmp_newptr__);                                                       \
    struct VectorHeader *tmp_vh__ = tmp_newptr__;                               \
    tmp_vh__->count = (new_count);                                              \
    tmp_vh__->limit = (new_count) * sizeof(*vecptr);                            \
    struct VectorHeader *tmp_old_vh__ = vector_get_headerptr(vecptr);           \
    size_t tmp_size__ = tmp_old_vh__->limit;                                    \
    tmp_size__ = (tmp_size__ > tmp_vh__->limit) ? tmp_vh__->limit : tmp_size__; \
    memcpy(vector_get_dataptr(tmp_vh__), vecptr, tmp_size__);                   \
    free(tmp_old_vh__);                                                         \
    vecptr = vector_get_dataptr(tmp_vh__);                                      \
}

#define vector_add(vecptr, element) \
{                                                                               \
    assert(vecptr);                                                             \
    assert(sizeof(vecptr) == sizeof(void *));                                   \
    struct VectorHeader *tmp_vh__ = vector_get_headerptr(vecptr);               \
    if (tmp_vh__->limit < (tmp_vh__->count+1) * sizeof(*vecptr)) {              \
        size_t tmp_newsize__ = sizeof(struct VectorHeader)                      \
                             + (sizeof(*vecptr) * ((tmp_vh__->count+1)*2));     \
        void *tmp_newptr__ = malloc(tmp_newsize__);                             \
        assert(tmp_newptr__);                                                   \
        memcpy(tmp_newptr__, tmp_vh__, VEC_HDR_SIZE_+tmp_vh__->limit);          \
        free(tmp_vh__);                                                         \
        tmp_vh__ = tmp_newptr__;                                                \
        tmp_vh__->limit = tmp_newsize__ - VEC_HDR_SIZE_;                        \
        vecptr = vector_get_dataptr(tmp_vh__);                                  \
    }                                                                           \
    vecptr[tmp_vh__->count] = element;                                          \
    tmp_vh__->count += 1;                                                       \
}
