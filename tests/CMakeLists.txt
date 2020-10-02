if(MSVC)
	add_compile_options(/W1 /WX)

	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
		add_compile_options(/MTd)
	else()
		add_compile_options(/MT)
	endif()

	# MSVC does not have C++11.
	set(CMAKE_CXX_STANDARD 14)
else()
	add_compile_options(-Wall -Wextra -Werror -Wdouble-promotion -Wformat=2 -Wconversion -ffunction-sections -fdata-sections)
	set(CMAKE_CXX_STANDARD 11)
endif()

set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory("${PROJECT_SOURCE_DIR}/extern/googletest" "extern/googletest")

add_custom_target(teststore)
libstored_generate(teststore TestStore.st)

macro(libstored_add_test TESTNAME)
	add_executable(${TESTNAME} ${ARGN})
	target_link_libraries(${TESTNAME} gtest gmock gtest_main teststore-libstored)
	gtest_discover_tests(${TESTNAME}
		WORKING_DIRECTORY ${PROJECT_DIR}
		PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${PROJECT_DIR}"
	)
	set_target_properties(${TESTNAME} PROPERTIES FOLDER tests)
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

if(HAVE_VALGRIND AND LIBSTORED_EXAMPLES)
	macro(libstored_example_test EXAMPLE)
		add_test(NAME ${EXAMPLE} COMMAND valgrind --error-exitcode=1 --leak-check=full ${CMAKE_CURRENT_BINARY_DIR}/../examples/${EXAMPLE}/${EXAMPLE})
	endmacro()

	libstored_example_test(1_hello)
	libstored_example_test(2_basic)
	libstored_example_test(3_scope)
	libstored_example_test(4_function)
	libstored_example_test(5_debug)
	libstored_example_test(6_hooks)
endif()

find_program(SPIN_EXECUTABLE spin)

if(SPIN_EXECUTABLE)
	add_custom_target(
		arq-spin-run
		COMMAND ${SPIN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/ArqLayer.pml
	)

	set(ARQ_VERIFY_CMD ${SPIN_EXECUTABLE} -search -DBITSTATE -O3 -b -v -bitstate ${CMAKE_CURRENT_SOURCE_DIR}/ArqLayer.pml)

	add_custom_target(
		arq-spin-verify
		COMMAND ${ARQ_VERIFY_CMD}
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	)

	add_test(
		NAME ArqLayer-spin
		COMMAND ${ARQ_VERIFY_CMD}
		WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	)
endif()

add_test(
	NAME ZmqClient
	COMMAND ${CMAKE_COMMAND} -E env
		PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/../client
		"PATH=$<SHELL_PATH:$<TARGET_FILE_DIR:libzmq>;$ENV{PATH}>"
		${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_ZmqClient.py $<TARGET_FILE:zmqserver>
)
set_tests_properties(ZmqClient PROPERTIES TIMEOUT 60)

