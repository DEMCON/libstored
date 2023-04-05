# libstored, distributed debuggable data stores.
# Copyright (C) 2020-2023  Jochem Rutgers
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

if(MSVC)
	add_compile_options(/Wall /WX)
else()
	add_compile_options(-Wall -Wextra -Werror -Wdouble-promotion -Wformat=2 -Wundef -Wconversion -Wshadow -ffunction-sections -fdata-sections)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "8.0.1")
	add_compile_options(-Wno-defaulted-function-deleted)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 11)
	# gcc's updated -Warray-bounds seem to give a lot of bogus errors.
	# In this case, assigning to uint64_t& value in __time_s() seems to trip it.
	# Ignore this warning, for now.
	add_compile_options(-Wno-array-bounds)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT APPLE)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
endif()

add_subdirectory(1_hello)
add_subdirectory(2_basic)
add_subdirectory(3_scope)
add_subdirectory(4_function)
add_subdirectory(5_debug)
add_subdirectory(6_hooks)
add_subdirectory(7_protocol)
add_subdirectory(8_sync)
add_subdirectory(9_fpga)
add_subdirectory(installed)
add_subdirectory(terminal)
add_subdirectory(meta)

if(NOT MINGW AND NOT CMAKE_CXX_STANDARD EQUAL 98)
	# std::thread is not supported by MinGW.
	add_subdirectory(concurrency)
endif()

if(LIBSTORED_HAVE_LIBZMQ)
	if(LIBSTORED_DRAFT_API)
		add_subdirectory(components)
	endif()
	add_subdirectory(control)
	add_subdirectory(zmqserver)
endif()

if(LIBSTORED_HAVE_ZTH AND LIBSTORED_HAVE_LIBZMQ)
	add_subdirectory(fibered)
endif()

if(LIBSTORED_HAVE_QT AND LIBSTORED_HAVE_LIBZMQ)
	add_subdirectory(qt)
endif()

list(FIND CMAKE_CXX_COMPILE_FEATURES "cxx_std_17" _cxx17)
if(${_cxx17} GREATER -1)
	if(NOT CMAKE_CXX_STANDARD)
		set(CMAKE_CXX_STANDARD 17)
	endif()

	if(CMAKE_CXX_STANDARD GREATER_EQUAL 17 AND CMAKE_CXX_STANDARD LESS 98 AND LIBSTORED_DRAFT_API)
		add_subdirectory(pipes)
	endif()
endif()
