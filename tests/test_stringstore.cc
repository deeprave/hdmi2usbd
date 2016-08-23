//
// Created by David Nugent on 3/02/2016.
//

#include "gtest/gtest.h"

extern "C" {
#include "stringstore.h"
}

#define STRSTORE_ALLOC      0x5a1dc51
#define MY_STRSTORE_SIZE    (size_t)128

namespace {

    const char * TESTSTRINGS[] = {
            "This", " ", "",
            "is", " ",
            "a", " ", "",
            "test", " ",
            "somewhat", " ",
            "long", " ",
            "list", " ",
            "of", " ",
            "words", ".\n"
    };
    size_t NUMBER_OF_TESTSTRINGS = sizeof(TESTSTRINGS) / sizeof(const char *);
    size_t FIRST_STRING_SIZE = strlen(TESTSTRINGS[0]);

    void
    run_basic_tests(stringstore_t *pstore, size_t size =MY_STRSTORE_SIZE, size_t length =0) {
        ASSERT_NE(pstore, (stringstore_t *)NULL);
        ASSERT_NE(stringstore_buffer(pstore), (void *)NULL);
        ASSERT_EQ(stringstore_capacity(pstore), size);
        ASSERT_GE(stringstore_length(pstore), length);
    }

    void
    fill_stringstore(stringstore_t *pstore, size_t length =MY_STRSTORE_SIZE) {
        size_t slength = 0;
        size_t plength = stringstore_length(pstore);
        int i =0;
        while (slength < length) {
            const char *string = TESTSTRINGS[i++ % NUMBER_OF_TESTSTRINGS];
            stringstore_storestr(pstore, string);
            slength += strlen(string);
            ASSERT_EQ(stringstore_length(pstore), plength + slength);
        }
        ASSERT_GE(stringstore_length(pstore), plength + slength);
    }

    // Tests

    TEST(StringstoreFunctions, stringstoreCreationAutoAndStore) {
        stringstore_t store;
        stringstore_t *pstore = stringstore_init_n(&store, MY_STRSTORE_SIZE);
        ASSERT_NE(pstore, (stringstore_t const *)NULL);
        ASSERT_NE(store.alloc, STRSTORE_ALLOC);
        run_basic_tests(pstore);
        fill_stringstore(pstore, 1);
        run_basic_tests(pstore, MY_STRSTORE_SIZE, FIRST_STRING_SIZE);   // "This"
        fill_stringstore(pstore);
        run_basic_tests(pstore, MY_STRSTORE_SIZE*2, MY_STRSTORE_SIZE+FIRST_STRING_SIZE);
        fill_stringstore(pstore, MY_STRSTORE_SIZE);
        run_basic_tests(pstore, MY_STRSTORE_SIZE*3, MY_STRSTORE_SIZE*2+FIRST_STRING_SIZE);
        stringstore_free(pstore);
    }

    TEST(StringstoreFunctions, stringstoreCreateHeapAndStore) {
        stringstore_t *pstore = stringstore_init_n(NULL, MY_STRSTORE_SIZE);
        ASSERT_NE(pstore, (stringstore_t *)NULL);
        ASSERT_EQ(pstore->alloc, STRSTORE_ALLOC);
        run_basic_tests(pstore);
        fill_stringstore(pstore, 1);
        run_basic_tests(pstore, MY_STRSTORE_SIZE, 1);       // 4
        fill_stringstore(pstore);
        run_basic_tests(pstore, MY_STRSTORE_SIZE*2, MY_STRSTORE_SIZE+FIRST_STRING_SIZE);    // 4+128 = 132
        fill_stringstore(pstore, MY_STRSTORE_SIZE*2);
        run_basic_tests(pstore, MY_STRSTORE_SIZE*4, MY_STRSTORE_SIZE*3+FIRST_STRING_SIZE);  // 132+256 = 388
        stringstore_free(pstore);
    }

    TEST(StringstoreFunctions, stringstoreIterator) {
        stringstore_t *pstore = stringstore_init_n(NULL, MY_STRSTORE_SIZE);
        run_basic_tests(pstore);
        fill_stringstore(pstore);
        run_basic_tests(pstore, MY_STRSTORE_SIZE*2, MY_STRSTORE_SIZE);

        size_t i =0;
        size_t stringlength =0;
        for (i = 0; i < NUMBER_OF_TESTSTRINGS; i++)
            stringlength += strlen(TESTSTRINGS[i]);
        char fullstring[stringlength+1];
        fullstring[0] = '\0';
        for (i = 0; i < NUMBER_OF_TESTSTRINGS; i++)
            strcat(fullstring,TESTSTRINGS[i]);

        stringstore_iterator_t iter = stringstore_iterator(pstore);
        size_t length, bytecount =0;
        char const *s;
        while ((s = stringstore_nextstr(&iter, &length)) != NULL) {
            ASSERT_EQ(strncmp(s, fullstring, length), 0);
            bytecount += length;
        }
        ASSERT_NE(length, stringlength); // last one should be short
        ASSERT_GE(bytecount, MY_STRSTORE_SIZE - stringstore_remaining(&iter));

        stringstore_free(pstore);
    }

    TEST(StringstoreFunctions, stringstoreSplit) {
        stringstore_t *pstore = stringstore_init_n(NULL, MY_STRSTORE_SIZE);
        run_basic_tests(pstore);
        fill_stringstore(pstore);
        run_basic_tests(pstore, MY_STRSTORE_SIZE*2, MY_STRSTORE_SIZE);

        stringstore_iterator_t iter = stringstore_iterator(pstore);
        ASSERT_EQ(pstore, iter.store);
        ASSERT_EQ((size_t)0, iter.offset);

        size_t length;
        char const *s;
        int linecount = 0;
        while ((s = stringstore_nextstr(&iter, &length)) != NULL)
            linecount++;

        int stringcount = stringstore_split(pstore, "\n");
        ASSERT_EQ(linecount, stringcount);

        linecount = 0;
        while ((s = stringstore_nextstr(&iter, &length)) != NULL)
            linecount++;

        ASSERT_EQ(linecount, 0);
        stringstore_free(pstore);
    }

    TEST(StringstoreFunctions, stringstoreConsume) {
        stringstore_t *pstore = stringstore_init_n(NULL, MY_STRSTORE_SIZE);
        fill_stringstore(pstore);
        run_basic_tests(pstore, MY_STRSTORE_SIZE*2, MY_STRSTORE_SIZE);
        ASSERT_GE(stringstore_length(pstore), MY_STRSTORE_SIZE);
        size_t difference = stringstore_length(pstore) - MY_STRSTORE_SIZE;

        stringstore_consume(pstore, MY_STRSTORE_SIZE/2);
        run_basic_tests(pstore, MY_STRSTORE_SIZE*2, MY_STRSTORE_SIZE/2);
        ASSERT_EQ(stringstore_length(pstore), MY_STRSTORE_SIZE/2+difference);

        stringstore_free(pstore);
    }

} // namespace
