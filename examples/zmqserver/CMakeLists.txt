# SPDX-FileCopyrightText: 2020-2024 Jochem Rutgers
#
# SPDX-License-Identifier: CC0-1.0

add_executable(zmqserver main.cpp)
libstored_generate(TARGET zmqserver STORES ZmqServerStore.st)
install(TARGETS zmqserver RUNTIME DESTINATION bin)
add_launch_json(zmqserver)

if(WIN32)
	# Copy libzmq.dll to here, to allow starting zmqserver.exe without deploying.
	add_custom_command(
		TARGET zmqserver
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:libzmq> ${CMAKE_CURRENT_BINARY_DIR}
		VERBATIM
	)
endif()
