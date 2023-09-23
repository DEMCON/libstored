# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

# This example shows how to integrate libstored in your own CMake project when using libstored as
# submodule.

# See 1_hello_submodule/CMakeLists.txt that would be your project.

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/1_hello_submodule.timestamp
	DEPENDS ../1_hello/main.cpp ../1_hello/ExampleHello.st
	DEPENDS 1_hello_submodule/CMakeLists.txt
	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/1_hello_submodule
	COMMAND
		${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}/1_hello_submodule
		${CMAKE_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/1_hello_submodule -G ${CMAKE_GENERATOR}
		-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
		-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}
		-DPYTHON_EXECUTABLE=${PYTHON_EXECUTABLE}
		-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE} -DLIBSTORED_INSTALL_STORE_LIBS=OFF
		-DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD} -DCMAKE_C_STANDARD=${CMAKE_C_STANDARD}
		--no-warn-unused-cli
	COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}/1_hello_submodule
		${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE}
	COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}/1_hello_submodule
		${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE} --target install
	COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/1_hello_submodule.timestamp
	COMMENT "Building 1_hello using libstored as submodule"
	VERBATIM
)

add_custom_target(int_submodule ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/1_hello_submodule.timestamp)
