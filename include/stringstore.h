//
// Created by David Nugent on 16/08/2016.
// Simple and efficient ordered string container

#ifndef GENERIC_STRSTORE_H
#define GENERIC_STRSTORE_H

#include <sys/types.h>

typedef struct stringstore_s stringstore_t;
typedef struct stringstore_iterator_s stringstore_iterator_t;

struct stringstore_s {
    int alloc;
    size_t ss_size,
           ss_used,
           ss_inc;
    void * ss_buff;
};

struct stringstore_iterator_s {
    stringstore_t *store;
    size_t offset;
};

enum stringstore_pos {
    SS_START    = (size_t)0,
    SS_END      = (size_t)-1,
    SS_ALL      = (size_t)-1,
};

extern stringstore_t *stringstore_init(stringstore_t *pstore);
extern stringstore_t *stringstore_init_n(stringstore_t *pstore, size_t size);
extern void stringstore_free(stringstore_t *pstore);

extern void *stringstore_buffer(stringstore_t *pstore);
extern size_t stringstore_capacity(stringstore_t *pstore);
extern size_t stringstore_length(stringstore_t *pstore);

// storage
extern size_t stringstore_storestr(stringstore_t *pstore, char const *value);
extern size_t stringstore_storestr_at(stringstore_t *pstore, char const *value, size_t at);
extern size_t stringstore_store(stringstore_t *pstore, void const *value, size_t length, size_t at);
extern size_t stringstore_append(stringstore_t *pstore, void const *value, size_t length);

// iterator & related functions
extern stringstore_iterator_t stringstore_iterator(stringstore_t *pstore);
extern void const *stringstore_at(stringstore_iterator_t *iter);
extern void const *stringstore_ptr(stringstore_t *pstore, size_t offset);
extern void const *stringstore_next(stringstore_iterator_t *piter, size_t *plength, char const *delim);
extern char const *stringstore_nextstr(stringstore_iterator_t *piter, size_t *plength);
extern size_t stringstore_remaining(stringstore_iterator_t *piter);

// NUL split strings in buffer
extern int stringstore_split(stringstore_t *pstore, char const *delim);

// consume (remove) bytes from buffer
extern void stringstore_consume(stringstore_t *pstore, size_t bytes);
extern void stringstore_clear(stringstore_t *pstore);

#endif //GENERIC_STRSTORE_H
