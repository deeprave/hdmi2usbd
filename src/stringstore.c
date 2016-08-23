//
// Created by David Nugent on 16/08/2016.
//
// stringstore_t is an object used to store and line buffer strings
// It primarily supports a line-based protocol but can possibly be
// used to store arbitrary data as well (untested)

#include <string.h>
#include <stdlib.h>
#include "stringstore.h"


#define STRSTORE_ALLOC  0x5a1dc51
#define STRSTORE_SIZE   2048

stringstore_t *
stringstore_init(stringstore_t *pstore) {
    return stringstore_init_n(pstore, STRSTORE_SIZE);
}

// Initialise (possibly allocate) a stringstore_t
// If passed NULL as arg, allocates (and marks as allocated)
stringstore_t *
stringstore_init_n(stringstore_t *pstore, size_t size) {
    if (pstore != NULL)
        memset(pstore, '\0', sizeof(stringstore_t));
    else {
        pstore = calloc(1, sizeof(stringstore_t));
        pstore->alloc = STRSTORE_ALLOC;
    }
    pstore->ss_used = 0;
    pstore->ss_inc = pstore->ss_size = size ? size : STRSTORE_ALLOC;
    pstore->ss_buff = calloc(1, size);
    return pstore;
}

// Free a stringstore_t object
// Frees the buffer space as well
void
stringstore_free(stringstore_t *pstore) {
    if (pstore != NULL) {
        // Free the store if we originally allocated it
        free(pstore->ss_buff);
        pstore->ss_buff = NULL;
        if (pstore->alloc == STRSTORE_ALLOC) {
            pstore->alloc = 0; // prevent accidental re-free
            free(pstore);
        }
    }
}

// Get the non-const base buffer address
void *
stringstore_buffer(stringstore_t *pstore) {
    return pstore->ss_buff;
}

// Return a ptr at a particular offset

void const *
stringstore_ptr(stringstore_t *pstore, size_t offset) {
    return offset >= pstore->ss_used ? NULL : stringstore_buffer(pstore) + offset;
}

void const *
stringstore_at(stringstore_iterator_t *iter) {
    return stringstore_ptr(iter->store, iter->offset);
}

// Return current capacity of the store
size_t
stringstore_capacity(stringstore_t *pstore) {
    return pstore->ss_size;
}

// Return the amount of data in the store in bytes

size_t
stringstore_length(stringstore_t *pstore) {
    return pstore->ss_used;
}


// Resize the buffer

size_t
strstore_resize(stringstore_t *pstore, size_t to_length) {
    if (to_length < stringstore_length(pstore))
        to_length = stringstore_length(pstore);
    if (to_length != stringstore_capacity(pstore)) {
        void * buffer = pstore->ss_buff; // save old bufffer addr
        pstore->ss_buff = calloc(1, to_length);
        if (to_length > 0)
            memmove(pstore->ss_buff, buffer, pstore->ss_used);
        pstore->ss_size = to_length;
        free(buffer);
    }
    return to_length;
}

// Shortcut to resize to current used length

size_t
strstore_compact(stringstore_t *pstore) {
    return strstore_resize(pstore, 0);
}

// strstore_append: append/insert data into the store

size_t
stringstore_store(stringstore_t *pstore, void const *value, size_t length, size_t at) {
    if ((length + stringstore_length(pstore)) > stringstore_capacity(pstore)) {
        size_t increment = pstore->ss_inc;  // default growth increment
        while ((stringstore_length(pstore) + length) > (stringstore_capacity(pstore) + increment))
            increment += pstore->ss_inc;    // add increment blocks until we can fit the value
        strstore_resize(pstore, stringstore_capacity(pstore) + increment);
    }
    if (length == 0 || value == NULL)
        length = 0;
    else if (at == SS_END || at >= pstore->ss_used) // append
        memmove(pstore->ss_buff + pstore->ss_used, value, length);
    else {                                          // insert
        void *dest = pstore->ss_buff + at;
        memmove(dest + length, dest, pstore->ss_used - at);
        memmove(dest, value, length);
    }
    pstore->ss_used += length;
    return length;
}

// append data to a stringstore

size_t
stringstore_append(stringstore_t *pstore, void const *value, size_t length) {
    return stringstore_store(pstore, value, length, SS_END);
}

// stringstore_store: store a nul terminated string in the store

size_t
stringstore_storestr(stringstore_t *pstore, char const *value) {
    return stringstore_store(pstore, value, value == NULL ? 0 : strlen(value), SS_END);
}

size_t
stringstore_storestr_at(stringstore_t *pstore, char const *value, size_t at) {
    return stringstore_store(pstore, value, value == NULL ? 0 : strlen(value), at);
}

// stringstore_iterator: create an iterator for stringstore_t

stringstore_iterator_t
stringstore_iterator(stringstore_t *pstore) {
    return (stringstore_iterator_t){pstore, 0 };
}

// stringstore_next: return next string (and length) via iterator, NULL at end
// string length returned by plength
// if delim is NULL, strings are terminated by NUL or end of buffer
// if delim is NULL, strings are terminated by any char in delim

void const *
stringstore_next(stringstore_iterator_t *piter, size_t *plength, char const *delim) {
    stringstore_t *pstore = piter->store;
    size_t size = 0;
    char const *ptr = NULL;
    if (piter->offset < pstore->ss_used) {
        ptr = (const char *) stringstore_at(piter);
        size_t remaining_bytes = pstore->ss_used - piter->offset;
        int found_eos = (delim == NULL);
        while (size < remaining_bytes) {
            char ch = ptr[size++];
            if (ch == '\0' || (delim != NULL && strchr(delim, ch) != NULL)) {
                found_eos = 1;
                break;
            }
        }
        if (!found_eos) {
            ptr = NULL;
            size = 0;
        }
    }
    if (plength)
        *plength = size;
    piter->offset += size;
    return ptr;
}


char const *
stringstore_nextstr(stringstore_iterator_t *piter, size_t *plength) {
    return (char const *)stringstore_next(piter, plength, "\n");
}


size_t
stringstore_remaining(stringstore_iterator_t *piter) {
    return stringstore_length(piter->store) - piter->offset;
}


// stringstore_split: split string via list of delimiters

int
stringstore_split(stringstore_t *pstore, char const *delim) {
    int stringcount = 0;
    if (pstore->ss_used) {
        char *ptr = pstore->ss_buff;
        size_t remaining_bytes = pstore->ss_used;
        for (size_t offset = 0; offset < remaining_bytes; ) {
            char ch = ptr[offset];
            if (ch == '\0' || (delim != NULL && strchr(delim, ch))) {
                ptr[offset] = '\0';
                stringcount++;
            }
            offset++;
        }
    }
    return stringcount;
}

void
stringstore_clear(stringstore_t *pstore) {
    stringstore_consume(pstore, SS_ALL);
}

void
stringstore_consume(stringstore_t *pstore, size_t bytes) {
    if (bytes >= stringstore_length(pstore))
        pstore->ss_used = 0;    // easy case
    else {
        size_t remaining = pstore->ss_used - bytes;
        memmove(pstore->ss_buff, pstore->ss_buff+bytes, remaining);
        pstore->ss_used = remaining;
    }
}

