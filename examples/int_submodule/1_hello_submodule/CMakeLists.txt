# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: CC0-1.0

# Assume this is your project, and you have installed libstored as a submodule somewhere.  Now,
# include libstored, such that you can generate stores.

cmake_minimum_required(VERSION 3.10)
project(1_hello_submodule)

# Point to the directory where you have cloned libstored.
set(LIBSTORED_CLONE ${PROJECT_SOURCE_DIR}/../../..)
list(APPEND CMAKE_MODULE_PATH ${LIBSTORED_CLONE}/cmake)

# libstored requires python. Make sure that PYTHON_EXECUTABLE is set if you have a venv, otherwise
# it is searched for using find_program().

# If you want to have ZMQ, heatshrink, Zth, and Qt, you have to do a find_package() yourself, and
# set LIBSTORED_HAVE_LIBZMQ (and friends) to ON. Refer to the list of options() at the top of of
# cmake/libstored.cmake for details.

# Include all libstored functions.
include(libstored)

# Now, just continue with defining your application.
set(HELLO_PATH ${LIBSTORED_CLONE}/examples/1_hello)
add_executable(1_hello_submodule ${HELLO_PATH}/main.cpp)

libstored_generate(
	TARGET 1_hello_submodule STORES ${HELLO_PATH}/ExampleHello.st DESTINATION
	${CMAKE_CURRENT_BINARY_DIR}
)

install(TARGETS 1_hello_submodule RUNTIME)
