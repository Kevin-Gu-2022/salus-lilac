# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
set(BOARD nrf52840dk/nrf52840)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(blockchain)

target_link_libraries(app PRIVATE mbedTLS)

target_sources(app PRIVATE src/main.c lib/cJSON/cJSON.c)
target_include_directories(app PRIVATE lib/cJSON)