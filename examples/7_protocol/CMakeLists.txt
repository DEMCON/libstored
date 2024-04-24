# SPDX-FileCopyrightText: 2020-2024 Jochem Rutgers
#
# SPDX-License-Identifier: CC0-1.0

add_executable(7_protocol main.cpp)
libstored_generate(TARGET 7_protocol STORES ExampleProtocol.st)
install(TARGETS 7_protocol RUNTIME DESTINATION bin)
add_dependencies(examples 7_protocol)
