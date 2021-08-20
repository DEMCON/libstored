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

# The value will be appended to a STORED_POLL_ define.
set(LIBSTORED_POLL "AUTO" CACHE STRING "Poll method")

set(LIBSTORED_CLANG_TIDY "OFF")
set(LIBSTORED_INSTALL_STORE_LIBS "OFF")

if(MSVC)
	add_compile_options(/W1 /WX)

	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
		add_compile_options(/MTd)
	else()
		add_compile_options(/MT)
	endif()
else()
	add_compile_options(-Wall -Wextra -Werror -Wdouble-promotion -Wformat=2 -Wconversion -ffunction-sections -fdata-sections)
	if(UNIX)
		set(LIBSTORED_ENABLE_ASAN ON)
		if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
			set(LIBSTORED_ENABLE_LSAN ON)
		endif()
		set(LIBSTORED_ENABLE_UBSAN ON)
		add_compile_options(-fno-sanitize-recover=undefined)
	endif()
endif()

if(APPLE)
	set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)
	set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
	googletest
	GIT_REPOSITORY https://github.com/google/googletest.git
	GIT_TAG        release-1.11.0
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_custom_target(teststore)
libstored_generate(teststore TestStore.st)
target_compile_definitions(teststore-libstored PUBLIC STORED_POLL_${LIBSTORED_POLL})
target_include_directories(teststore-libstored BEFORE PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(LIBSTORED_ENABLE_UBSAN AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	# The combination of -fno-sanitize-recover and ubsan gives some issues with
	# vptr. This might be related: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=94325.
	# Disable it for now.
	add_compile_options(-fno-sanitize=vptr)
	target_compile_options(teststore-libstored PUBLIC -fno-sanitize=vptr)
endif()

macro(libstored_add_test TESTNAME)
	add_executable(${TESTNAME} ${ARGN} test_base.cpp)
	target_include_directories(${TESTNAME} BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
	target_link_libraries(${TESTNAME} gtest gmock gtest_main teststore-libstored)
	gtest_discover_tests(${TESTNAME} TEST_LIST tests)
	set_tests_properties(${tests} PROPERTIES TIMEOUT 60)
	set_target_properties(${TESTNAME} PROPERTIES FOLDER tests)
	if(CMAKE_GENERATOR MATCHES "Visual Studio.*")
		# Every binary has its own output directory.
		libstored_copy_dlls(${TESTNAME})
	endif()
endmacro()

libstored_add_test(test_allocator test_allocator.cpp)
libstored_add_test(test_types test_types.cpp)
libstored_add_test(test_init test_init.cpp)
libstored_add_test(test_function test_function.cpp)
libstored_add_test(test_array test_array.cpp)
libstored_add_test(test_directory test_directory.cpp)
libstored_add_test(test_spm test_spm.cpp)
libstored_add_test(test_protocol test_protocol.cpp)
libstored_add_test(test_debugger test_debugger.cpp)
libstored_add_test(test_synchronizer test_synchronizer.cpp)
libstored_add_test(test_fifo test_fifo.cpp)
if(WIN32)
	libstored_add_test(test_poller test_poller_win.cpp)
	if(LIBSTORED_HAVE_LIBZMQ)
		add_custom_target(libzmq-test-copy
			DEPENDS libzmq
			COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:libzmq> ${CMAKE_CURRENT_BINARY_DIR}
			VERBATIM
		)
		add_dependencies(test_poller libzmq-test-copy)
		add_dependencies(test_protocol libzmq-test-copy)
	endif()
else()
	libstored_add_test(test_poller test_poller.cpp)
endif()

if(CMAKE_GENERATOR MATCHES ".*Makefiles.*")
	# All test binaries are put in the same directory.
	# Only copy the dlls once.
	libstored_copy_dlls(test_debugger)
endif()

if(LIBSTORED_EXAMPLES)
	macro(libstored_example_test EXAMPLE)

		# Disabled in favor of sanitizers.
#		if(HAVE_VALGRIND)
#			add_test(NAME ${EXAMPLE}-valgrind
#				COMMAND valgrind --error-exitcode=1 --leak-check=full $<TARGET_FILE:${EXAMPLE}> ${ARGN})
#		endif()

		if(WIN32)
			add_test(NAME ${EXAMPLE}-run
				COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_INSTALL_PREFIX}/bin $ENV{ComSpec} /C "$<SHELL_PATH:$<TARGET_FILE_NAME:${EXAMPLE}>> ${ARGN} > $<SHELL_PATH:${PROJECT_BINARY_DIR}/examples/${EXAMPLE}/output.txt>")
		else()
			add_test(NAME ${EXAMPLE}-run
				COMMAND sh -c "${CMAKE_INSTALL_PREFIX}/bin/$<TARGET_FILE_NAME:${EXAMPLE}> ${ARGN} > ${PROJECT_BINARY_DIR}/examples/${EXAMPLE}/output.txt")
		endif()

		if(EXISTS ${PROJECT_SOURCE_DIR}/examples/${EXAMPLE}/output.txt)
			add_test(NAME ${EXAMPLE}-output
				COMMAND ${CMAKE_COMMAND} -E compare_files --ignore-eol ${PROJECT_SOURCE_DIR}/examples/${EXAMPLE}/output.txt ${PROJECT_BINARY_DIR}/examples/${EXAMPLE}/output.txt)
		endif()
	endmacro()

	libstored_example_test(1_hello)
	libstored_example_test(2_basic)
	libstored_example_test(3_scope)
	libstored_example_test(4_function)
	libstored_example_test(5_debug)
	libstored_example_test(6_hooks)
	if(TARGET concurrency)
		libstored_example_test(concurrency 3)
	endif()
endif()

find_program(SPIN_EXECUTABLE spin)

if(SPIN_EXECUTABLE)
	add_custom_target(
		arq-spin-run
		COMMAND ${SPIN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/DebugArqLayer.pml
	)

	set(ARQ_VERIFY_CMD ${SPIN_EXECUTABLE} -search -DBITSTATE -O3 -b -v -bitstate ${CMAKE_CURRENT_SOURCE_DIR}/DebugArqLayer.pml)

	add_custom_target(
		arq-spin-verify
		COMMAND ${ARQ_VERIFY_CMD}
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	)

	add_test(
		NAME DebugArqLayer-spin
		COMMAND ${ARQ_VERIFY_CMD}
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	)
endif()

if(WIN32)
	set(ENV_PATH_SEP ";")
else()
	set(ENV_PATH_SEP ":")
endif()

if(LIBSTORED_HAVE_LIBZMQ)
	add_test(
		NAME ZmqClient
		COMMAND ${CMAKE_COMMAND} -E env
			PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/../client
			"PATH=$<SHELL_PATH:$<TARGET_FILE_DIR:libzmq>>${ENV_PATH_SEP}$ENV{PATH}"
			${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_ZmqClient.py ${CMAKE_INSTALL_PREFIX}/bin/$<TARGET_FILE_NAME:zmqserver>
	)

	set_tests_properties(ZmqClient PROPERTIES TIMEOUT 60)
endif()

if(VIVADO_CMD)
	if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test_fpga/vivado/test_fpga/test_fpga.xpr)
		execute_process(
			COMMAND ${VIVADO_CMD} -mode batch -source test_fpga.tcl
			WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test_fpga/vivado
		)
	endif()

	add_test(
		NAME FPGA
		COMMAND $<SHELL_PATH:${VIVADO_CMD}> -mode batch -source ${CMAKE_CURRENT_SOURCE_DIR}/test_fpga/run.tcl ${CMAKE_CURRENT_SOURCE_DIR}/test_fpga/vivado/test_fpga/test_fpga.xpr
	)
endif()

