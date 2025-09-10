# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: CC0-1.0

# Assume this is your project, and you have generated the FindLibstored.cmake file, which can be
# found in the CMAKE_MODULE_PATH.

cmake_minimum_required(VERSION 3.10)
project(1_hello_findpackage)

# Default integration of ZeroMQ, Heathshrink, Zth, Qt5 and Qt6 can be enabled by providing these as
# COMPONENTS to find_package().
#
# If you want to have more control over these libraries, you have to do a find_package() yourself,
# and set LIBSTORED_HAVE_LIBZMQ (and friends) to ON. Refer to the list of options() at the top of of
# cmake/libstored.cmake for details.

find_package(Libstored REQUIRED COMPONENTS Heatshrink)

# Now, just continue with defining your application.
set(HELLO_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../1_hello)
add_executable(1_hello_findpackage ${HELLO_PATH}/main.cpp)

libstored_generate(
	TARGET 1_hello_findpackage STORES ${HELLO_PATH}/ExampleHello.st DESTINATION
	${CMAKE_CURRENT_BINARY_DIR}
)

install(TARGETS 1_hello_findpackage RUNTIME)
