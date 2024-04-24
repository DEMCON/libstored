# SPDX-FileCopyrightText: 2020-2024 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

set(LIBSTORED_HAVE_HEATSHRINK 0)
add_executable(5_debug main.cpp)
libstored_generate(TARGET 5_debug STORES ExampleDebugSomeStore.st ExampleDebugAnotherStore.st)
install(TARGETS 5_debug RUNTIME DESTINATION bin)
add_dependencies(examples 5_debug)
