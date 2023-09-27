# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

# This example shows how use libstored-cmake with find_package() to find libstored.
#
# Refer to 1_hello_findpackage/CMakeLists.txt for more information.

set(_more_flags)

if(CMAKE_CXX_STANDARD)
	list(APPEND _more_flags -DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD})
endif()
if(CMAKE_C_STANDARD)
	list(APPEND _more_flags -DCMAKE_C_STANDARD=${CMAKE_C_STANDARD})
endif()

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/1_hello_findpackage.timestamp
	DEPENDS ../1_hello/main.cpp ../1_hello/ExampleHello.st
	DEPENDS 1_hello_findpackage/CMakeLists.txt
	# Prepare a new CMake build dir.
	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/1_hello_findpackage
	COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH=${LIBSTORED_PYTHONPATH} ${PYTHON_EXECUTABLE} -m
		libstored.cmake ${CMAKE_CURRENT_BINARY_DIR}/FindLibstored.cmake
	COMMAND
		${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}/1_hello_findpackage
		${CMAKE_COMMAND} ${CMAKE_CURRENT_SOURCE_DIR}/1_hello_findpackage
		-DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
		-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} -G ${CMAKE_GENERATOR}
		-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE} -DLIBSTORED_INSTALL_STORE_LIBS=OFF
		-DCMAKE_MODULE_PATH=${CMAKE_CURRENT_BINARY_DIR} --no-warn-unused-cli ${_more_flags}
	# Build 1_hello_findpackage.
	COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}/1_hello_findpackage
		${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE}
	# Install 1_hello_findpackage into our deploy dir.
	COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}/1_hello_findpackage
		${CMAKE_COMMAND} --build . --config ${CMAKE_BUILD_TYPE} --target install
	# Done.
	COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/1_hello_findpackage.timestamp
	COMMENT "Building 1_hello using find_package()"
	VERBATIM
)

add_custom_target(
	int_findpackage ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/1_hello_findpackage.timestamp
)
