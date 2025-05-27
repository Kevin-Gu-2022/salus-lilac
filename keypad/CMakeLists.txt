# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(task2)

target_sources(app PRIVATE src/main.c)

# dt_compat_check_additional_bindings(dts/bindings/input)