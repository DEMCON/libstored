# The value will be appended to a STORED_POLL_ define.
set(LIBSTORED_POLL "AUTO" CACHE STRING "Poll method")

set(LIBSTORED_CLANG_TIDY "OFF")

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
		set(LIBSTORED_ENABLE_LSAN ON)
		set(LIBSTORED_ENABLE_UBSAN ON)
	endif()
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory("${PROJECT_SOURCE_DIR}/extern/googletest" "extern/googletest")

add_custom_target(teststore)
libstored_generate(teststore TestStore.st)
target_compile_definitions(teststore-libstored PUBLIC STORED_POLL_${LIBSTORED_POLL})

macro(libstored_add_test TESTNAME)
	add_executable(${TESTNAME} ${ARGN})
	target_link_libraries(${TESTNAME} gtest gmock gtest_main teststore-libstored)
	gtest_discover_tests(${TESTNAME} TEST_LIST tests)
	set_tests_properties(${tests} PROPERTIES TIMEOUT 60)
	set_target_properties(${TESTNAME} PROPERTIES FOLDER tests)
	if(CMAKE_GENERATOR MATCHES "Visual Studio.*")
		# Every binary has its own output directory.
		libstored_copy_dlls(${TESTNAME})
	endif()
endmacro()

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
				COMMAND ${CMAKE_COMMAND} -E chdir $<TARGET_FILE_DIR:${EXAMPLE}> $ENV{ComSpec} /C "$<SHELL_PATH:$<TARGET_FILE:${EXAMPLE}>> ${ARGN} > $<SHELL_PATH:${PROJECT_BINARY_DIR}/examples/${EXAMPLE}/output.txt>")
		else()
			add_test(NAME ${EXAMPLE}-run
				COMMAND sh -c "$<TARGET_FILE:${EXAMPLE}> ${ARGN} > ${PROJECT_BINARY_DIR}/examples/${EXAMPLE}/output.txt")
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
			${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_ZmqClient.py $<TARGET_FILE:zmqserver>
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

