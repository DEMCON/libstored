# SPDX-FileCopyrightText: 2020-2024 Jochem Rutgers
#
# SPDX-License-Identifier: CC0-1.0

add_executable(1_hello main.cpp)
libstored_generate(TARGET 1_hello STORES ExampleHello.st)
install(TARGETS 1_hello RUNTIME DESTINATION bin)
add_launch_json(1_hello)
