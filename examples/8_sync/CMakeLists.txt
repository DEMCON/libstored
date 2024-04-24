# SPDX-FileCopyrightText: 2020-2024 Jochem Rutgers
#
# SPDX-License-Identifier: CC0-1.0

if(LIBSTORED_HAVE_LIBZMQ)
	add_executable(8_sync main.cpp getopt_mini.cpp)
	libstored_generate(TARGET 8_sync STORES ExampleSync1.st ExampleSync2.st)
	install(TARGETS 8_sync RUNTIME DESTINATION bin)
	add_launch_json(8_sync)
endif()
