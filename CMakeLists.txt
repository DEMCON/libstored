# libstored, distributed debuggable data stores.
# Copyright (C) 2020-2021  Jochem Rutgers
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

cmake_minimum_required(VERSION 3.1)
project(libstored)

cmake_policy(VERSION 3.1)

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

message(STATUS "CMAKE_BUILD_TYPE is ${CMAKE_BUILD_TYPE}")

if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_BINARY_DIR})
	message(FATAL_ERROR "CMake generation is not allowed within the source directory! \
		Remove the CMakeCache.txt file and try again from another folder, e.g.: \
		\
		  rm CMakeCache.txt \
		  mkdir build \
		  cd build \
		  cmake ..")
endif()

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/deploy" CACHE PATH "Override default install path" FORCE)
endif()

message(STATUS "Install prefix set to ${CMAKE_INSTALL_PREFIX}")
if(APPLE)
	list(APPEND CMAKE_INSTALL_RPATH "@executable_path/../lib")
	if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.8")
		list(APPEND CMAKE_BUILD_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
	else()
		set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
	endif()
elseif(UNIX)
	list(APPEND CMAKE_INSTALL_RPATH "\$ORIGIN/../lib")
endif()

option(LIBSTORED_INSTALL_STORE_LIBS "Install generated static libstored libraries" ON)

option(LIBSTORED_DEV "Enable by default development related build options" OFF)
if(LIBSTORED_DEV)
	set(LIBSTORED_DEV_OPTION ON)
	set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY TRUE)
else()
	set(LIBSTORED_DEV_OPTION OFF)
endif()

option(LIBSTORED_PYLIBSTORED "Build python libstored package" ON)

if(CMAKE_HOST_WIN32 OR NOT LIBSTORED_PYLIBSTORED)
	# The documentation needs wavedrom (via sphinx), but sphinxcontrib-wavedrom
	# needs cairo.  In turn, that package needs libcairo-2.dll (64-bit,
	# matching python's word size), which cannot be installed automatically by
	# bootstrap.cmd. So, by default, leave it out.
	option(LIBSTORED_DOCUMENTATION "Create the HTML based API documentation" OFF)
else()
	option(LIBSTORED_DOCUMENTATION "Create the HTML based API documentation" ON)
endif()

option(LIBSTORED_VENV "Create a python venv" ON)
if(LIBSTORED_VENV)
	message(STATUS "Checking venv...")
	if(CMAKE_HOST_UNIX)
		if(LIBSTORED_PYLIBSTORED OR LIBSTORED_DOCUMENTATION)
			set(venv_check "check")
		else()
			set(venv_check "check-minimal")
		endif()
		# Force CC and CXX to default, in case we are cross-compiling
		# and compiling wheels for the venv...
		execute_process(
			COMMAND ${CMAKE_COMMAND} -E env --unset=CC --unset=CXX ${PROJECT_SOURCE_DIR}/dist/common/venv.sh ${venv_check}
			RESULT_VARIABLE ret)
		list(APPEND CMAKE_PROGRAM_PATH "${PROJECT_SOURCE_DIR}/dist/.venv/bin")
	elseif(CMAKE_HOST_WIN32)
		execute_process(
			COMMAND ${CMAKE_COMMAND} -E env --unset=CC --unset=CXX ${PROJECT_SOURCE_DIR}/dist/win32/venv.cmd check
			RESULT_VARIABLE ret)
		list(APPEND CMAKE_PROGRAM_PATH "${PROJECT_SOURCE_DIR}/dist/.venv/Scripts")
	else()
		message(FATAL_ERROR "Don't know how to create a venv.")
	endif()
	if(ret EQUAL "2")
		message(FATAL_ERROR "Cannot create venv")
	endif()
endif()

# Should be defaulted to OFF some day.
option(LIBSTORED_DRAFT_API "Enable draft API" ON)
if(LIBSTORED_DRAFT_API)
	message(STATUS "Enable libstored draft API")
endif()

if(MINGW OR MSVC)
	set(LIBSTORED_ENABLE_ASAN_DEFAULT NO)
	set(LIBSTORED_ENABLE_LSAN_DEFAULT NO)
	set(LIBSTORED_ENABLE_UBSAN_DEFAULT NO)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
	set(LIBSTORED_ENABLE_ASAN_DEFAULT ${LIBSTORED_DEV_OPTION})
	set(LIBSTORED_ENABLE_LSAN_DEFAULT NO)
	set(LIBSTORED_ENABLE_UBSAN_DEFAULT ${LIBSTORED_DEV_OPTION})
else()
	set(LIBSTORED_ENABLE_ASAN_DEFAULT ${LIBSTORED_DEV_OPTION})
	set(LIBSTORED_ENABLE_LSAN_DEFAULT ${LIBSTORED_DEV_OPTION})
	set(LIBSTORED_ENABLE_UBSAN_DEFAULT ${LIBSTORED_DEV_OPTION})
endif()

option(LIBSTORED_ENABLE_ASAN "Build with Address Sanitizer" ${LIBSTORED_ENABLE_ASAN_DEFAULT})
option(LIBSTORED_ENABLE_LSAN "Build with Leak Sanitizer" ${LIBSTORED_ENABLE_LSAN_DEFAULT})
option(LIBSTORED_ENABLE_UBSAN "Build with Undefined Behavior Sanitizer" ${LIBSTORED_ENABLE_UBSAN_DEFAULT})

set(LIBSTORED_CLANG_TIDY_DEFAULT OFF)

if(${CMAKE_VERSION} VERSION_GREATER "3.6.0")
	find_program(CLANG_TIDY_EXE NAMES "clang-tidy" DOC "Path to clang-tidy executable")
	if(	# We must have clang-tidy.
		CLANG_TIDY_EXE AND
		# We must compile for C++11 or later...
		(NOT CMAKE_CXX_STANDARD OR NOT CMAKE_CXX_STANDARD EQUAL 98) AND
		# ...except when running in Windows, which only supports C++14 or later.
		(NOT WIN32 OR NOT CMAKE_CXX_STANDARD EQUAL 11)
	)
		# It seems that if clang is not installed, clang-tidy doesn't work properly.
		find_program(CLANG_EXE NAMES "clang" DOC "Path to clang executable")
		if(CLANG_EXE AND LIBSTORED_DEV)
			set(LIBSTORED_CLANG_TIDY_DEFAULT ${LIBSTORED_DEV_OPTION})
		endif()
	endif()
endif()

option(LIBSTORED_CLANG_TIDY "Run clang-tidy" ${LIBSTORED_CLANG_TIDY_DEFAULT})

add_subdirectory(version)

if(LIBSTORED_DOCUMENTATION AND LIBSTORED_PYLIBSTORED)
	add_subdirectory(sphinx)
endif()

if(CMAKE_HOST_WIN32)
	find_program(PYTHON_EXECUTABLE python)
else()
	find_program(PYTHON_EXECUTABLE python3)
	find_program(HAVE_VALGRIND valgrind)
endif()

if(NOT PYTHON_EXECUTABLE)
	message(FATAL_ERROR "Cannot find python")
else()
	message(STATUS "Using python ${PYTHON_EXECUTABLE}")
endif()

set(LIBSTORED_SOURCE_DIR ${PROJECT_SOURCE_DIR} CACHE STRING "Path to the libstored sources" FORCE)
list(APPEND CMAKE_MODULE_PATH ${LIBSTORED_SOURCE_DIR}/cmake)

option(LIBSTORED_HAVE_HEATSHRINK "Use heatshrink" ON)

if(LIBSTORED_HAVE_HEATSHRINK)
	find_package(Heatshrink)
endif()

option(LIBSTORED_HAVE_LIBZMQ "Use libzmq" ON)

if(LIBSTORED_HAVE_LIBZMQ)
	find_package(ZeroMQ)
endif()

add_subdirectory(extern)

include(libstored)

if(DEFINED ENV{XILINX_VIVADO})
	find_program(VIVADO_CMD NAMES vivado PATHS "$ENV{XILINX_VIVADO}/bin")
endif()

if(NOT VIVADO_CMD)
	if(WIN32)
		file(GLOB VIVADO_CMDS "C:/Xilinx/Vivado/*/bin/vivado.bat")
	else()
		file(GLOB VIVADO_CMDS "/opt/Xilinx/Vivado/*/bin/vivado")
	endif()

	if(VIVADO_CMDS)
		list(SORT VIVADO_CMDS)
		list(GET VIVADO_CMDS -1 VIVADO_CMD)
	endif()
endif()

if(VIVADO_CMD)
	message(STATUS "Using vivado ${VIVADO_CMD}")
endif()

option(LIBSTORED_EXAMPLES "Build examples" ON)
if(LIBSTORED_EXAMPLES)
	add_subdirectory(examples)
endif()

if(LIBSTORED_VENV)
	set(LIBSTORED_PIP_INSTALL_USER_DEFAULT OFF)
else()
	set(LIBSTORED_PIP_INSTALL_USER_DEFAULT ON)
endif()

option(LIBSTORED_PIP_INSTALL_USER "Run pip install with --user flag when installing wheel files" ${LIBSTORED_PIP_INSTALL_USER_DEFAULT})

if(LIBSTORED_PYLIBSTORED)
	add_subdirectory(python)
endif()

if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.11.0")
	if(CMAKE_CROSSCOMPILING)
		set(LIBSTORED_TESTS_DEFAULT OFF)
	else()
		set(LIBSTORED_TESTS_DEFAULT ${LIBSTORED_DEV_OPTION})
	endif()

	option(LIBSTORED_TESTS "Build the tests" ${LIBSTORED_TESTS_DEFAULT})

	if(LIBSTORED_TESTS)
		enable_testing()

		if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.17.0")
			set(CTEST_OUTPUT_ON_FAILURE 1)
			list(APPEND CMAKE_CTEST_ARGUMENTS "--output-on-failure")
		endif()

		add_subdirectory(tests)
	endif()
endif()

find_program(CPPCHECK_CMD NAMES cppcheck)
if(CPPCHECK_CMD)
	if(LIBSTORED_EXAMPLES)
		macro(cppcheck_add app)
			if(TARGET ${app}-libstored-generate)
				set(CPPCHECK_EXAMPLES ${CPPCHECK_EXAMPLES} -I examples/${app}/libstored/include examples/${app})
				set(CPPCHECK_DEPENDS ${CPPCHECK_DEPENDS} ${app}-libstored-generate)
			endif()
		endmacro()

		cppcheck_add(1_hello)
		cppcheck_add(2_basic)
		cppcheck_add(3_scope)
		cppcheck_add(4_function)
		cppcheck_add(5_debug)
		cppcheck_add(6_hooks)
		cppcheck_add(7_protocol)
		cppcheck_add(8_sync)
		cppcheck_add(9_fpga)
		cppcheck_add(components)
		cppcheck_add(concurrency)
		cppcheck_add(control)
		cppcheck_add(terminal)
		cppcheck_add(zmqserver)
	endif()

	add_custom_target(libstored-cppcheck
		COMMAND
			${CPPCHECK_CMD}
			--enable=warning,style,information
			--force
			--inline-suppr
			--quiet
			--suppressions-list=${CMAKE_CURRENT_SOURCE_DIR}/.cppcheck_suppr
			--error-exitcode=1
			-DCPPCHECK
#			--xml
			-I include
			${CPPCHECK_EXAMPLES}
			src
#			--check-config
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		VERBATIM
	)

	if(CPPCHECK_DEPENDS)
		add_dependencies(libstored-cppcheck ${CPPCHECK_DEPENDS})
	endif()

	option(LIBSTORED_CPPCHECK "Run cppcheck by default" ${LIBSTORED_DEV_OPTION})
	if(LIBSTORED_CPPCHECK)
		add_custom_target(libstored-cppcheck-all ALL DEPENDS libstored-cppcheck)
	endif()
endif()

if(LIBSTORED_DIST_DIR)
	add_subdirectory(${LIBSTORED_DIST_DIR})
endif()
