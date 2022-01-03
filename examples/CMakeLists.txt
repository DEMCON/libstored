# libstored, distributed debuggable data stores.
# Copyright (C) 2020-2022  Jochem Rutgers
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

if(MSVC)
	add_compile_options(/Wall /WX)
else()
	add_compile_options(-Wall -Wextra -Werror -Wdouble-promotion -Wformat=2 -Wundef -Wconversion -Wshadow -ffunction-sections -fdata-sections)
	if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND UNIX)
		set(LIBSTORED_ENABLE_ASAN ON)
		if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
			set(LIBSTORED_ENABLE_LSAN ON)
		endif()
		set(LIBSTORED_ENABLE_UBSAN ON)
	endif()
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
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

if(NOT MINGW AND NOT CMAKE_CXX_STANDARD EQUAL 98)
	# std::thread is not supported by MinGW.
	add_subdirectory(concurrency)
endif()

if(LIBSTORED_HAVE_LIBZMQ)
	add_subdirectory(components)
	add_subdirectory(control)
	add_subdirectory(zmqserver)
endif()

if(LIBSTORED_HAVE_ZTH)
	add_subdirectory(fibered)
endif()
