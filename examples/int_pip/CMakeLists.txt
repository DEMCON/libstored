# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

# This example shows how use libstored from pip, without having the repo checked out.
#
# Refer to 1_hello_pip/CMakeLists.txt for more information.

add_custom_command(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/1_hello_pip.timestamp
	DEPENDS ../1_hello/main.cpp ../1_hello/ExampleHello.st
	DEPENDS 1_hello_pip/CMakeLists.txt
	DEPENDS pylibstored-install
	# Prepare a new CMake build dir.
	COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/1_hello_pip
	# The current PYTHON_EXECUTABLE should have pylibstored installed (see DEPENDS
	# pylibstored-install above, but you would normally do python -m pip install libstored
	# yourself). You will see that the generated FindLibstored.cmake contains paths to your
	# python env, instead of the libstored repo.
	COMMAND ${PYTHON_EXECUTABLE} -m libstored.cmake
		${CMAKE_CURRENT_BINARY_DIR}/FindLibstored.cmake
	COMMAND
		${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}/1_hello_pip ${CMAKE_COMMAND}
		${CMAKE_CURRENT_SOURCE_DIR}/1_hello_pip -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
		-DCMAKE_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX} -G ${CMAKE_GENERATOR}
		-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE} -DLIBSTORED_INSTALL_STORE_LIBS=OFF
		-DLIBSTORED_CLANG_TIDY=${LIBSTORED_CLANG_TIDY}
		-DCMAKE_MODULE_PATH=${CMAKE_CURRENT_BINARY_DIR} --no-warn-unused-cli
		-DCMAKE_CXX_STANDARD=${CMAKE_CXX_STANDARD} -DCMAKE_C_STANDARD=${CMAKE_C_STANDARD}
	# Build 1_hello_pip.
	COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}/1_hello_pip ${CMAKE_COMMAND}
		--build . --config ${CMAKE_BUILD_TYPE}
	# Install 1_hello_pip into our deploy dir.
	COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR}/1_hello_pip ${CMAKE_COMMAND}
		--build . --config ${CMAKE_BUILD_TYPE} --target install
	# Done.
	COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/1_hello_pip.timestamp
	COMMENT "Building 1_hello using pylibstored"
	VERBATIM
)

add_custom_target(int_pip ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/1_hello_pip.timestamp)
