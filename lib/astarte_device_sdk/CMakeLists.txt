# (C) Copyright 2024, SECO Mind Srl
#
# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_include_directories(include)

FILE(GLOB lib_sources *.c bson/*.c data/*.c mqtt/*.c pairing/*.c storage/*.c)
if(NOT CONFIG_ASTARTE_DEVICE_SDK_PERMANENT_STORAGE)
    LIST(REMOVE_ITEM lib_sources ${CMAKE_CURRENT_LIST_DIR}/storage/core.c)
    LIST(REMOVE_ITEM lib_sources ${CMAKE_CURRENT_LIST_DIR}/storage/introsp.c)
    LIST(REMOVE_ITEM lib_sources ${CMAKE_CURRENT_LIST_DIR}/storage/prop.c)
    LIST(REMOVE_ITEM lib_sources ${CMAKE_CURRENT_LIST_DIR}/storage/sync.c)
    LIST(REMOVE_ITEM lib_sources ${CMAKE_CURRENT_LIST_DIR}/storage/key_value_entry.c)
    LIST(REMOVE_ITEM lib_sources ${CMAKE_CURRENT_LIST_DIR}/storage/key_value.c)
endif()
zephyr_library_sources(${lib_sources})

zephyr_library_link_libraries_ifdef(CONFIG_MBEDTLS mbedTLS)

if(CONFIG_ASTARTE_DEVICE_SDK_ADVANCED_CODE_GENERATION)
    include(generation.cmake)
endif()
