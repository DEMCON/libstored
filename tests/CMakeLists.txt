# libstored, distributed debuggable data stores.
# Copyright (C) 2020-2023  Jochem Rutgers
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

# The value will be appended to a STORED_POLL_ define.
set(LIBSTORED_POLL "AUTO" CACHE STRING "Poll method")

option(LIBSTORED_CLANG_TIDY "Run clang-tidy" ${LIBSTORED_DEV_OPTION})
set(LIBSTORED_CLANG_TIDY "OFF")
set(LIBSTORED_INSTALL_STORE_LIBS "OFF")
set(LIBSTORED_DRAFT_API "ON")

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
		if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "8.0.1")
			# Not exactly sure which clang version this applies to,
			# but for 8.0.0, these sanitizers seem to do more harm
			# than good. I've seen some random compile issues or
			# crashes...
			set(LIBSTORED_ENABLE_ASAN OFF)
			set(LIBSTORED_ENABLE_UBSAN OFF)
		else()
			set(LIBSTORED_ENABLE_ASAN ON)
			set(LIBSTORED_ENABLE_UBSAN ON)
		endif()
		if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
			set(LIBSTORED_ENABLE_LSAN OFF)
		else()
			set(LIBSTORED_ENABLE_LSAN ON)
		endif()
		add_compile_options(-fno-sanitize-recover=undefined)
	endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "8.0.1")
	add_compile_options(-Wno-defaulted-function-deleted)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	add_compile_options(-Wno-array-bounds)
endif()

if(APPLE)
	set(CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_PREFIX}/lib)
	set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
endif()

set(CMAKE_CXX_STANDARD 17)
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
libstored_generate(TARGET teststore STORES TestStore.st)
target_compile_definitions(teststore-libstored PUBLIC STORED_POLL_${LIBSTORED_POLL})
target_include_directories(teststore-libstored BEFORE PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

function(libstored_add_test TESTNAME)
	add_executable(${TESTNAME} ${ARGN} test_base.cpp)
	target_include_directories(${TESTNAME} BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
	target_link_libraries(${TESTNAME} gtest gmock gtest_main teststore-libstored)
	set_target_properties(${TESTNAME} PROPERTIES FOLDER tests)
	gtest_add_tests(TARGET ${TESTNAME} TEST_LIST tests)
	set_tests_properties(${tests} PROPERTIES TIMEOUT 60)
endfunction()

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
libstored_add_test(test_hooks test_hooks.cpp)
libstored_add_test(test_fifo test_fifo.cpp)
libstored_add_test(test_components test_components.cpp)
libstored_add_test(test_weak test_weak.cpp)
libstored_add_test(test_weak_override test_weak_override.cpp)

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_VERSION VERSION_GREATER "9.1.0")
	# Your really need proper C++17 support.
	libstored_add_test(test_pipes test_pipes.cpp)
endif()

libstored_add_test(test_signal test_signal.cpp)
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

# All test binaries are put in the same directory.
# Only copy the dlls once.
libstored_copy_dlls(test_debugger)

if(LIBSTORED_EXAMPLES)
	macro(libstored_example_test EXAMPLE)

		# Disabled in favor of sanitizers.
#		if(HAVE_VALGRIND)
#			add_test(NAME ${EXAMPLE}-valgrind
#				COMMAND valgrind --error-exitcode=1 --leak-check=full $<TARGET_FILE:${EXAMPLE}> ${ARGN})
#		endif()

		if(WIN32)
			add_test(NAME ${EXAMPLE}-run
				COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_INSTALL_PREFIX}/bin $ENV{ComSpec} /C "$<TARGET_FILE_NAME:${EXAMPLE}> ${ARGN} > $<SHELL_PATH:${PROJECT_BINARY_DIR}/examples/${EXAMPLE}/output.txt>")
		else()
			add_test(NAME ${EXAMPLE}-run
				COMMAND sh -c "'${CMAKE_INSTALL_PREFIX}/bin/$<TARGET_FILE_NAME:${EXAMPLE}>' ${ARGN} > '${PROJECT_BINARY_DIR}/examples/${EXAMPLE}/output.txt'")
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
	libstored_example_test(meta)
	if(TARGET concurrency)
		libstored_example_test(concurrency 3)
	endif()
	if(TARGET fibered)
		libstored_example_test(fibered)
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

if(LIBSTORED_HAVE_LIBZMQ AND LIBSTORED_EXAMPLES AND LIBSTORED_PYLIBSTORED)
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



# Disable sanitizers for the targets below, as it is probably going to be executed by callgrind/AFL++.
set(LIBSTORED_ENABLE_ASAN OFF)
set(LIBSTORED_ENABLE_UBSAN OFF)
set(LIBSTORED_ENABLE_LSAN OFF)
add_custom_target(teststore-perf)
libstored_generate(TARGET teststore-perf STORES TestStore.st DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/perf)
add_executable(perf_synchronizer perf_synchronizer.cpp)
target_link_libraries(perf_synchronizer teststore-perf-libstored)


add_executable(fuzz_debug fuzz_debug.cpp)
target_link_libraries(fuzz_debug teststore-perf-libstored)
