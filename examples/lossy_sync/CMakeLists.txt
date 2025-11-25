# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: CC0-1.0

if(LIBSTORED_HAVE_LIBZMQ)
	add_executable(lossy_sync main.cpp)
	set_target_properties(lossy_sync PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES)
	target_link_libraries(lossy_sync PRIVATE example_lib)
	libstored_generate(TARGET lossy_sync STORES ExampleSync.st)
	install(TARGETS lossy_sync RUNTIME DESTINATION bin)
	add_launch_json(lossy_sync)
endif()
