# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: CC0-1.0

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED OFF)

add_executable(pipes main.cpp)
libstored_generate(TARGET pipes STORES ExamplePipes.st)
install(TARGETS pipes RUNTIME DESTINATION bin)
