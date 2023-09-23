# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: CC0-1.0

add_executable(components main.cpp)
set_target_properties(components PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED ON)
libstored_generate(TARGET components STORES ExampleComponents.st)
install(TARGETS components RUNTIME DESTINATION bin)
