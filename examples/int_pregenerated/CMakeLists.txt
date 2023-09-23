# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

# This example shows how use pre-generated store files in an application. These pre-generated files
# could be committed to a repository and used later on.
#
# Refer to 1_hello_pregenerated/CMakeLists.txt for more information.

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/1_hello_pregenerated.timestamp
	DEPENDS ../1_hello/main.cpp
	DEPENDS 1_hello_pregenerated/CMakeLists.txt
	# In this case, we just depend on the generated files of 1_hello.
	DEPENDS 1_hello-libstored-generate
	# Prepare a new CMake build dir.
	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/1_hello_pregenerated
	COMMAND
		${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}/1_hello_pregenerated
		${CMAKE_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/1_hello_pregenerated
		-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
		-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} -G ${CMAKE_GENERATOR}
		-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE} -DLIBSTORED_INSTALL_STORE_LIBS=OFF
		--no-warn-unused-cli
	# Build 1_hello_pregenerated.
	COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}/1_hello_pregenerated
		${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE}
	# Install 1_hello_pregenerated into our deploy dir.
	COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}/1_hello_pregenerated
		${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE} --target install
	# Done.
	COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/1_hello_pregenerated.timestamp
	COMMENT "Building 1_hello using pregenerated source files"
	VERBATIM
)

add_custom_target(
	int_pregenerated ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/1_hello_pregenerated.timestamp
)
