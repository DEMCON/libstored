# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: CC0-1.0

add_executable(terminal main.cpp)
libstored_generate(TARGET terminal STORES ExampleTerminal.st)
install(TARGETS terminal RUNTIME DESTINATION bin)
