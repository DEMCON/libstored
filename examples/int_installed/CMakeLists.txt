# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

# This example shows how to link against an installed libstored static library with your stores.
#
# For this, we need the installed 1_hello static library. So, first build and install the examples.
# Next, build this example again.
#
# Refer to 1_hello_installed/CMakeLists.txt for more information.

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/1_hello_installed.timestamp
	DEPENDS ../1_hello/main.cpp
	DEPENDS 1_hello_installed/CMakeLists.txt
	# Prepare a new CMake build dir.
	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/1_hello_installed
	COMMAND
		${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}/1_hello_installed
		${CMAKE_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/1_hello_installed
		-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
		-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} -G ${CMAKE_GENERATOR}
		-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
		--no-warn-unused-cli
	# Build 1_hello_installed.
	COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}/1_hello_installed
		${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE}
	# Install 1_hello_installed into our deploy dir.
	COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}/1_hello_installed
		${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE} --target install
	# Done.
	COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/1_hello_installed.timestamp
	COMMENT "Building 1_hello using installed libraries"
	VERBATIM
)

add_custom_target(int_installed DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/1_hello_installed.timestamp)
