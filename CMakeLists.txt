cmake_minimum_required(VERSION 3.1)
project(libstored)

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

option(LIBSTORED_ENABLE_ASAN "Build with Address Sanitizer" OFF)
option(LIBSTORED_ENABLE_LSAN "Build with Leak Sanitizer" OFF)
option(LIBSTORED_ENABLE_UBSAN "Build with Undefined Behavior Sanitizer" OFF)

add_subdirectory(version)

if(WIN32)
	# The documentation needs wavedrom (via sphinx), but sphinxcontrib-wavedrom
	# needs cairo.  In turn, that package needs libcairo-2.dll (64-bit,
	# matching python's word size), which cannot be installed automatically by
	# bootstrap.cmd. So, by default, leave it out.
	option(LIBSTORED_DOCUMENTATION "Create the HTML based API documentation" OFF)
else()
	option(LIBSTORED_DOCUMENTATION "Create the HTML based API documentation" ON)
endif()

if(LIBSTORED_DOCUMENTATION)
	add_subdirectory(sphinx)
endif()

if(WIN32)
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

option(LIBSTORED_HAVE_LIBZMQ "Use libzmq" ON)
option(LIBSTORED_HAVE_HEATSHRINK "Use heatshrink" ON)

add_subdirectory(extern)

set(LIBSTORED_SOURCE_DIR ${PROJECT_SOURCE_DIR} CACHE STRING "Path to the libstored sources" FORCE)

list(APPEND CMAKE_MODULE_PATH ${LIBSTORED_SOURCE_DIR}/cmake)
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

option(LIBSTORED_PIP_INSTALL_USER "Run pip install --user when installing wheel files" OFF)
add_subdirectory(client)

if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.11.0")
	if(CMAKE_CROSSCOMPILING)
		set(LIBSTORED_TESTS_DEFAULT OFF)
	else()
		set(LIBSTORED_TESTS_DEFAULT ON)
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
		set(CPPCHECK_EXAMPLES
			-I examples/1_hello/libstored/include
			-I examples/2_basic/libstored/include
			-I examples/3_scope/libstored/include
			-I examples/4_function/libstored/include
			-I examples/5_debug/libstored/include
			-I examples/6_hooks/libstored/include
			-I examples/7_protocol/libstored/include
			-I examples/8_sync/libstored/include
			-I examples/9_fpga/libstored/include
			-I examples/components/libstored/include
			-I examples/concurrency/libstored/include
			-I examples/terminal/libstored/include
			-I examples/zmqserver/libstored/include
			examples
		)
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
#			--xml
			-I include
			${CPPCHECK_EXAMPLES}
			src
#			--check-config
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		VERBATIM
	)

	option(LIBSTORED_CPPCHECK "Run cppcheck by default" ON)
	if(LIBSTORED_CPPCHECK)
		add_custom_target(libstored-cppcheck-all ALL DEPENDS libstored-cppcheck)
	endif()
endif()

