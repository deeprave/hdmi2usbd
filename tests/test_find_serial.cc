//
// Created by David Nugent on 1/02/2016.
//


#include "gtest/gtest.h"

extern "C" {
#include "device.h"
#include "logging.h"
}

namespace {

    static const char portlist[] = "/dev/ttyVIZ*|ttyACM*|/dev/tty*";

    TEST(SerialFunctions, findSerialAll) {
        log_info("Searching for devices matching '%s'", portlist);
        struct ctrldev *first_dev = find_serial_all(portlist);
        // assuming we are running on a unix like system
        EXPECT_NE((struct ctrldev *)0, first_dev);
        for (struct ctrldev *next = first_dev; next != NULL; next = next->next) {
            ASSERT_NE((char *)0, next->devname);
            log_info("Found %s", next->devname);
            EXPECT_EQ(0, access(next->devname, R_OK|W_OK));
        }
    };

    TEST(SerialFunctions, findSerial) {
        char *ctrl_dev = find_serial(portlist);
        EXPECT_NE((char *)0, ctrl_dev);
        EXPECT_EQ(0, access(ctrl_dev, R_OK|W_OK));
        log_info("Selected device %s", ctrl_dev);
    }

} // namespace
