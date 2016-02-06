cmake_minimum_required(VERSION 3.4)

option(tests "Build all tests." OFF)         # cmake -Dtest=ON

set(PROJECT_NAME hdmi2usbd)
project(${PROJECT_NAME})

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
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
        src/iodev.c include/iodev.h
        src/selector.c include/selector.h
        src/serial.c include/serial.h
        src/nettcp.c include/nettcp.h)

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

    add_executable(runMiscUnitTests
            ${HDMI2USBD_SOURCE_FILES}
            ${SUPPORT_SOURCE_FILES}
            tests/misc/test_logging.cc
            tests/misc/test_array.cc
            tests/misc/test_buffer.cc)

    target_link_libraries(runMiscUnitTests gtest gtest_main)
    add_test(misc_tests runMiscUnitTests)

    add_executable(runSerialUnitTests
            ${HDMI2USBD_SOURCE_FILES}
            ${SUPPORT_SOURCE_FILES}
            tests/serial/test_find_serial.cc)

    target_link_libraries(runSerialUnitTests gtest gtest_main)
    add_test(serial_interface runSerialUnitTests)

endif()
