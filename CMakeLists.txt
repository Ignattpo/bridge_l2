cmake_minimum_required(VERSION 3.5)

set(CMAKE_CXX_STANDARD 11)

project(bridge_l2 LANGUAGES C)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/")

find_package(PCAP REQUIRED)

add_executable(bridge_l2
    main.c
    local.c
    local.h
    remote.c
    remote.h
    interface.c
    interface.h)

target_link_libraries(bridge_l2
    PUBLIC ${PCAP_LIBRARY}
    PUBLIC pthread)
