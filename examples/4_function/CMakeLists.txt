# SPDX-FileCopyrightText: 2020-2024 Jochem Rutgers
#
# SPDX-License-Identifier: CC0-1.0

add_executable(4_function main.cpp)
libstored_generate(TARGET 4_function STORES ExampleFunction.st)
install(TARGETS 4_function RUNTIME DESTINATION bin)
add_launch_json(4_function)
