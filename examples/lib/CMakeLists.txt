# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: CC0-1.0

add_library(example_lib STATIC include/getopt_mini.h src/getopt_mini.cpp)
target_include_directories(example_lib PUBLIC include)
