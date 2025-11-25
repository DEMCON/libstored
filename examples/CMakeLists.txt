# SPDX-FileCopyrightText: 2020-2025 Jochem Rutgers
#
# SPDX-License-Identifier: MPL-2.0

if(MSVC)
	add_compile_options(/Wall /WX)
else()
	add_compile_options(
		-Wall
		-Wextra
		-Werror
		-Wdouble-promotion
		-Wformat=2
		-Wundef
		-Wconversion
		-Wshadow
		-ffunction-sections
		-fdata-sections
	)

	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
		add_compile_options(-g3 -ggdb)
	endif()
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "8.0.1")
	add_compile_options(-Wno-defaulted-function-deleted)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 11)
	# gcc's updated -Warray-bounds seem to give a lot of bogus errors. In this case, assigning
	# to uint64_t& value in __time_s() seems to trip it. Ignore this warning, for now.
	add_compile_options(-Wno-array-bounds)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND NOT APPLE)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections")
endif()

if(NOT CMAKE_CXX_STANDARD)
	foreach(
		cxx
		cxx_std_26
		cxx_std_23
		cxx_std_20
		cxx_std_17
		cxx_std_14
		cxx_std_11
		cxx_std_98
	)
		list(FIND CMAKE_CXX_COMPILE_FEATURES "${cxx}" _cxx)
		if(${_cxx} GREATER -1)
			string(REPLACE "cxx_std_" "" _std "${cxx}")
			set(CMAKE_CXX_STANDARD "${_std}")
			break()
		endif()
	endforeach()
endif()

add_subdirectory(lib)

add_subdirectory(1_hello)
add_subdirectory(2_basic)
add_subdirectory(3_scope)
add_subdirectory(4_function)
add_subdirectory(5_debug)
add_subdirectory(6_hooks)
add_subdirectory(7_protocol)
add_subdirectory(8_sync)
add_subdirectory(9_fpga)
add_subdirectory(int_findpackage)
add_subdirectory(int_installed)
add_subdirectory(int_pregenerated)
add_subdirectory(int_submodule)
add_subdirectory(terminal)
add_subdirectory(meta)

if(NOT MINGW AND NOT CMAKE_CXX_STANDARD EQUAL 98)
	# std::thread is not supported by MinGW.
	add_subdirectory(concurrency)
endif()

if(LIBSTORED_HAVE_LIBZMQ)
	add_subdirectory(components)
	add_subdirectory(control)
	add_subdirectory(zmqserver)

	if(CMAKE_CXX_STANDARD LESS 98 AND CMAKE_CXX_STANDARD GREATER_EQUAL 11)
		add_subdirectory(lossy_sync)
	endif()
endif()

if(LIBSTORED_HAVE_ZTH AND LIBSTORED_HAVE_LIBZMQ)
	add_subdirectory(fibered)
endif()

if(LIBSTORED_PYLIBSTORED AND TARGET pylibstored-install)
	add_subdirectory(int_pip)
endif()

if(CMAKE_CXX_STANDARD GREATER_EQUAL 17 AND CMAKE_CXX_STANDARD LESS 98)
	add_subdirectory(pipes)
endif()
