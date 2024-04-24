# SPDX-FileCopyrightText: 2020-2024 Jochem Rutgers
#
# SPDX-License-Identifier: CC0-1.0

add_executable(fibered main.cpp)
libstored_generate(TARGET fibered STORES ExampleFibered.st ZTH ZMQ)
install(TARGETS fibered RUNTIME DESTINATION bin)
add_launch_json(fibered)
