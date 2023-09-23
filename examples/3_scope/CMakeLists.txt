# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: CC0-1.0

add_executable(3_scope main.cpp)
libstored_generate(TARGET 3_scope STORES ExampleScope.st)
install(TARGETS 3_scope RUNTIME DESTINATION bin)
