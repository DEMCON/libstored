# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: CC0-1.0

add_executable(2_basic main.cpp)
libstored_generate(TARGET 2_basic STORES ExampleBasic.st)
install(TARGETS 2_basic RUNTIME DESTINATION bin)
