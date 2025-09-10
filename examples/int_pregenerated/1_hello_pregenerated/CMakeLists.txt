# SPDX-FileCopyrightText: 2020-2023 Jochem Rutgers
#
# SPDX-License-Identifier: CC0-1.0

# Assume this is your project, and you don't have libstored as submodule, but use the generated
# library source files directly. This top-level cmake file shows how to do that.

cmake_minimum_required(VERSION 3.10)
project(1_hello_pregenerated)

# In the 1_hello example, the files are generated. Include the generated CMakeLists.txt file.  Note
# that if you want ZeroMQ, do something like find_package(ZeroMQ) and set(LIBSTORED_HAVE_LIBZMQ)
# before calling this add_subdirectory(). Check the generated CMakeLists.txt for the available
# options.
add_subdirectory(
	${CMAKE_CURRENT_SOURCE_DIR}/../../1_hello/libstored ${CMAKE_CURRENT_BINARY_DIR}/libstored
)

# Normal definition of your executable.
add_executable(1_hello_pregenerated ${CMAKE_CURRENT_SOURCE_DIR}/../../1_hello/main.cpp)

# Now, the add_subdirectory() above defines the library we need. Just link against it.
target_link_libraries(1_hello_pregenerated 1_hello-libstored)

# Install this new binary too.
install(TARGETS 1_hello_pregenerated RUNTIME)

# Check out the installed binary. It should be next to 1_hello and should behave the same, but now
# without the store generator magic.
