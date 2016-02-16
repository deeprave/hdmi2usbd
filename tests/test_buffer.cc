//
// Created by David Nugent on 4/02/2016.
//

#include <iostream>
#include "gtest/gtest.h"

extern "C" {
#include "buffer.h"
#include "logging.h"
}

namespace {

#define BUFSIZE_NONE    0
#define BUFSIZE_TINY    37
#define BUFSIZE_SMALL   123
#define BUFSIZE_MEDIUM  1356
#define BUFSIZE_LARGE   8167
#define BUFSIZE_HUGE    32799
#define BUFSIZE_MASSIVE 65241

    size_t buffer_sizes[] = {
        BUFSIZE_NONE,
        BUFSIZE_TINY,
        BUFSIZE_SMALL,
        BUFSIZE_MEDIUM,
        BUFSIZE_LARGE,
        BUFSIZE_HUGE,
        BUFSIZE_MASSIVE
    };
    size_t sizes = sizeof(buffer_sizes) / sizeof(size_t);

    TEST(BufferFunctions, testBufferInit) {
        for (int i =0; i < sizes; i++) {
            size_t size = buffer_sizes[i];
            buffer_t *buffer = buffer_init(NULL, size);
            ASSERT_EQ(size, buffer_size(buffer));
            ASSERT_EQ(0, buffer_used(buffer));
            buffer_free(buffer);
        }
    }

    TEST(BufferFunctions, testBufferPutGetAndAvailable) {
        log_info("testBufferPutGetAvail: buffsize tests");
        for (int i =0; i < sizes; i++) {
            size_t size = buffer_sizes[i];
            buffer_t *buffer = buffer_init(NULL, size);
            log_info("%lu", size);
            if (size == BUFSIZE_NONE) {
                unsigned char value = '!';
                // Unbuffered needs to always fail put here
                ASSERT_EQ(0, buffer_put(buffer, &value, 1));
                // similarly, it should always fail get
                ASSERT_EQ(0, buffer_get(buffer, &value, 1));
                // and the value should be untouched
                ASSERT_EQ(value, '!');
            } else {
                // fill up the buffer with a series of chars
                for (int i =0; i++ < (size*2);) {
                    unsigned char value = (unsigned char) ('0' + (i % 10));
                    size_t rc = buffer_put(buffer, &value, 1);
                    if (i < size) {
                        // should succeed up to size-1 puts
                        EXPECT_EQ(1, rc);
                        EXPECT_EQ((size - i) - 1, buffer_available(buffer));
                    } else {
                        // buffer is full...
                        // after that, it should always fail
                        EXPECT_EQ(0, rc);
                        EXPECT_EQ(0, buffer_available(buffer));
                    }
                }
                // check the buffer contents by comparing with same sequence
                for (int i =0; i++ < size+1; ) {
                    unsigned char value = (unsigned char) ('0' + (i % 10));
                    unsigned char got;
                    size_t rc = buffer_get(buffer, &got, 1);
                    if (i < size) {
                        EXPECT_EQ(1, rc);
                        EXPECT_EQ(value, got);
                        // each value retrieved should make another byte available
                        EXPECT_EQ(i, buffer_available(buffer));
                    } else {
                        // buffer should now be empty and get should have failed
                        EXPECT_EQ(0, rc);
                        EXPECT_EQ(size - 1, buffer_available(buffer));
                    }
                }
            }
            buffer_free(buffer);
        }
        log_info("done.");
    }

#define STRINGLENGTH 17
#define REMOVELENGTH 11

    TEST(BufferFunctions, testBufferPutGetAndUsed) {
        unsigned char buf[STRINGLENGTH+1];
        for (int i =0; i < STRINGLENGTH; i++)
            buf[i] = (unsigned char)('0' + i);
        buf[STRINGLENGTH] = '\0';
        log_info("testBufferPutGetUsed: bufsize tests");
        for (int i =0; i < sizes; i++) {
            size_t size = buffer_sizes[i];
            buffer_t *b = buffer_init(NULL, size);
            log_info("%lu", size);
            if (size == BUFSIZE_NONE) {
                ASSERT_EQ(0, buffer_put(b, &buf, STRINGLENGTH));
            } else {
                // fill up the buffer with same string
                // until we can't take any more
                size_t bytecount = 0;
                while (buffer_available(b) > 0) {
                    size_t rc = buffer_put(b, &buf, STRINGLENGTH);
                    if (bytecount + STRINGLENGTH < size) {
                        ASSERT_EQ(rc, STRINGLENGTH);
                    } else {
                        // should be left over
                        ASSERT_EQ(rc, ((size-1) % STRINGLENGTH));
                    }
                    bytecount += rc;
                }
                ASSERT_EQ(size - 1, bytecount);
            }
            buffer_free(b);
        }
        log_info("testBufferPutGetMulti: buffsize tests");
        for (int i =0; i < sizes; i++) {
            size_t size = buffer_sizes[i];
            buffer_t *b = buffer_init(NULL, size);
            log_info("%lu", size);
            if (size == BUFSIZE_NONE) {
                ASSERT_EQ(0, buffer_put(b, &buf, STRINGLENGTH));
            } else {
                // fill up the buffer with same string
                // removing a few each time to force buffer wrap
                size_t bytecount = 0;
                while (buffer_available(b) > 0) {
                    size_t rc = buffer_put(b, &buf, STRINGLENGTH);
                    if (bytecount + STRINGLENGTH < size) {
                        ASSERT_EQ(rc, STRINGLENGTH);
                        bytecount += STRINGLENGTH;
                        unsigned char removed[REMOVELENGTH+1];
                        rc = buffer_get(b, &removed, REMOVELENGTH);
                        // should always have enough to remove
                        // as REMOVELENGTH < STRINGLENGTH
                        ASSERT_EQ(rc, REMOVELENGTH);
                        bytecount -= REMOVELENGTH;
                        EXPECT_EQ(bytecount, buffer_used(b));
                    } else {
                        // should be something left over
                        ASSERT_GT(rc, 0);
                        ASSERT_LT(rc, STRINGLENGTH);
                        bytecount += rc;
                    }
                }
                ASSERT_EQ(size - 1, bytecount);
            }
            buffer_free(b);
        }
        log_info("done.");
    }

    TEST(BufferFunctions, testBufferCopyMove) {
        unsigned char buf[STRINGLENGTH+1];
        for (int i =0; i < STRINGLENGTH; i++)
            buf[i] = (unsigned char)('0' + i);
        buf[STRINGLENGTH] = '\0';
        log_info("testBufferCopyMove: buffsize tests");
        for (int i =1; i < sizes; i++) {
            size_t len_a, len_b, size = buffer_sizes[i];
            buffer_t *a = buffer_init(NULL, size);
            buffer_t *b = buffer_init(NULL, size);
            log_info("%lu", size);
            len_b = buffer_put(b, &buf, STRINGLENGTH);
            EXPECT_EQ(STRINGLENGTH, len_b);
            // now have STRINGLENGTH bytes in b (src)
            len_a = buffer_used(a);
            EXPECT_EQ(0, len_a);
            size_t copied = buffer_copy(a, b, STRINGLENGTH*2);
            EXPECT_EQ(STRINGLENGTH, copied);
            len_a = buffer_used(a);
            EXPECT_EQ(STRINGLENGTH, len_a);
            len_b = buffer_used(b);
            EXPECT_EQ(STRINGLENGTH, len_b);
            copied = buffer_move(a, b, STRINGLENGTH*2);
            EXPECT_EQ(STRINGLENGTH, copied);
            len_a = buffer_used(a);
            EXPECT_EQ(STRINGLENGTH*2, len_a);
            len_b = buffer_used(b);
            EXPECT_EQ(0, len_b);
            buffer_free(b);
            buffer_free(a);
        }
        log_info("done.");
    }

} // namespace
