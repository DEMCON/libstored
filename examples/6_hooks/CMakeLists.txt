# SPDX-FileCopyrightText: 2020-2024 Jochem Rutgers
#
# SPDX-License-Identifier: CC0-1.0

add_executable(6_hooks main.cpp)
libstored_generate(TARGET 6_hooks STORES ExampleHooks.st)
install(TARGETS 6_hooks RUNTIME DESTINATION bin)
add_launch_json(6_hooks)
