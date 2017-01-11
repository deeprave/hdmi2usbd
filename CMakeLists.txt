cmake_minimum_required(VERSION 3.4)

option(tests "Build all tests." OFF)         # cmake -Dtest=ON

set(PROJECT_NAME hdmi2usbd)
project(${PROJECT_NAME})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-unused-function")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

if (APPLE)
    add_definitions(-DGTEST_USE_OWN_TR1_TUPLE)
    add_definitions(-D__GLIBCXX__)
endif (APPLE)

include_directories(LOCAL include)

set(SUPPORT_SOURCE_FILES
        src/array.c include/array.h
        src/buffer.c include/buffer.h
        src/logging.c include/logging.h
        src/netutils.c include/netutils.h
        src/stringstore.c include/stringstore.h
        src/iodev.c include/iodev.h
        src/selector.c include/selector.h
        src/timer.c include/timer.h
        src/serial.c include/serial.h
        src/nettcp.c include/nettcp.h
        src/device.c include/device.h)

set(HDMI2USBD_SOURCE_FILES
        src/hdmi2usbd.c include/hdmi2usbd.h)

add_executable(hdmi2usbd
        src/main.c
        ${HDMI2USBD_SOURCE_FILES}
        ${SUPPORT_SOURCE_FILES})

####
# test support
####

if (tests)

    # add googletest gtest/gmock to test builds
    add_subdirectory( tests/googletest )
    enable_testing()

    include_directories( ${gtest_SOURCE_DIR}/include )
    include_directories( ${gmock_SOURCE_DIR}/include )

    ####
    # unit tests
    ####

    add_executable(runUnitTests
            ${HDMI2USBD_SOURCE_FILES}
            ${SUPPORT_SOURCE_FILES}
            tests/test_logging.cc
            tests/test_array.cc
            tests/test_buffer.cc
            tests/test_ipaddrs.cc
            tests/test_find_serial.cc
            tests/test_stringstore.cc )

    target_link_libraries(runUnitTests gtest gtest_main)
    add_test(unit_tests runUnitTests)

endif()
